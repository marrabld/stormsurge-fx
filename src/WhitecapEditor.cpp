// SPDX-License-Identifier: GPL-3.0-only
#include "WhitecapParameters.hpp"
#include "ConceptMockupArtwork.hpp"
#include "MaelstromArtwork.hpp"
#include "PreviewArtwork.hpp"
#include "DistrhoUI.hpp"
#include "DistrhoPluginUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::NanoImage;
using namespace stormsurge;

class WhitecapUI final : public UI
{
  public:
    WhitecapUI() : UI(static_cast<uint32_t>(WhitecapUiWidth), static_cast<uint32_t>(WhitecapHeight))
    {
        loadSharedResources();
        setGeometryConstraints(390, 510, true);
        enclosure = createImageFromMemory(whitecapEnclosurePng, sizeof(whitecapEnclosurePng), 0);
        knobMaterial = createImageFromMemory(previewKnobPng, sizeof(previewKnobPng), 0);
        footswitch = createImageFromMemory(previewFootswitchPng, sizeof(previewFootswitchPng), 0);
        toggleLeft =
            createImageFromMemory(maelstromToggleLeftPng, sizeof(maelstromToggleLeftPng), 0);
        toggleRight =
            createImageFromMemory(maelstromToggleRightPng, sizeof(maelstromToggleRightPng), 0);
        for (std::uint32_t i = 0; i < WhitecapParameterCount; ++i)
            values[i] = whitecapSpecs[i].initial;
#ifdef STORMSURGE_GUI_HARNESS
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
#endif
    }

#ifdef STORMSURGE_GUI_HARNESS
    bool exerciseControls()
    {
        if (!enclosure.isValid() || !knobMaterial.isValid() || !footswitch.isValid() ||
            !toggleLeft.isValid() || !toggleRight.isValid())
            return false;
        const float factor = drawingScale();
        const double dx = (getWidth() - WhitecapUiWidth * factor) * 0.5;
        const double dy = (getHeight() - WhitecapHeight * factor) * 0.5;
        for (const auto &knob : whitecapKnobs) {
            const float before = values[knob.parameter];
            MouseEvent mouse;
            mouse.button = 1;
            mouse.press = true;
            mouse.pos.setPos(dx + knob.x * factor, dy + knob.y * factor);
            if (!onMouse(mouse))
                return false;
            MotionEvent motion;
            motion.pos.setPos(dx + knob.x * factor, dy + (knob.y - 20) * factor);
            if (!onMotion(motion) || values[knob.parameter] <= before)
                return false;
            mouse.press = false;
            if (!onMouse(mouse))
                return false;
            ScrollEvent scroll;
            scroll.pos = mouse.pos;
            scroll.delta.setPos(0, 1);
            if (!onScroll(scroll))
                return false;
        }
        MouseEvent event;
        event.button = 1;
        event.press = true;
        event.pos.setPos(dx + WhitecapUiWidth * 0.5 * factor, dy + WhitecapModeY * factor);
        if (!onMouse(event) || values[WhitecapModeParameter] != 1.0f || !onMouse(event) ||
            values[WhitecapModeParameter] != 0.0f)
            return false;
        event.pos.setPos(dx + WhitecapUiWidth * 0.5 * factor, dy + WhitecapFootY * factor);
        if (!onMouse(event) || values[WhitecapBypass] != 1.0f || !onMouse(event) ||
            values[WhitecapBypass] != 0.0f)
            return false;
        return true;
    }

    void resizeForTest() { setSize(390, 510); }
#endif

  protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < WhitecapParameterCount) {
            values[index] = sanitizeWhitecap(index, value);
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float factor = drawingScale();
        translate((getWidth() - static_cast<float>(WhitecapUiWidth) * factor) * 0.5f,
                  (getHeight() - static_cast<float>(WhitecapHeight) * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(260, 42, "STORMSURGE FX", 13);
        caption(260, 78, "WHITECAP", 35);
        caption(260, 108, "CHORUS / VIBRATO", 11);
        for (const auto &knob : whitecapKnobs)
            drawKnob(knob);
        drawModeSwitch();
        drawLed();
        drawImage(footswitch, 218, static_cast<float>(WhitecapFootY) - 42, 84, 84);
        caption(260, 657, enabled() ? "EFFECT ON" : "BYPASSED", 10);
    }

    bool onMouse(const MouseEvent &event) override
    {
        if (event.button != 1)
            return false;
        if (!event.press) {
            const bool handled = dragging >= 0;
            if (handled)
                editParameter(static_cast<std::uint32_t>(dragging), false);
            dragging = -1;
            return handled;
        }
        if (dragging >= 0) {
            editParameter(static_cast<std::uint32_t>(dragging), false);
            dragging = -1;
        }
        const auto point =
            whitecapPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const int knob = whitecapKnobAt(point);
        if (knob >= 0) {
            dragging = knob;
            lastY = point.y;
            position = normalizedWhitecap(static_cast<std::uint32_t>(knob), values[knob]);
            editParameter(static_cast<std::uint32_t>(knob), true);
            return true;
        }
        if (whitecapModeAt(point)) {
            change(WhitecapModeParameter, values[WhitecapModeParameter] < 0.5f ? 1.0f : 0.0f);
            return true;
        }
        if (whitecapFootAt(point)) {
            change(WhitecapBypass, values[WhitecapBypass] < 0.5f ? 1.0f : 0.0f);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &event) override
    {
        if (dragging < 0)
            return false;
        const auto point =
            whitecapPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const double sensitivity = event.mod & kModifierShift ? 0.001 : 0.006;
        position = std::clamp(position + (lastY - point.y) * sensitivity, 0.0, 1.0);
        lastY = point.y;
        send(static_cast<std::uint32_t>(dragging),
             denormalizedWhitecap(static_cast<std::uint32_t>(dragging),
                                  static_cast<float>(position)));
        return true;
    }

    bool onScroll(const ScrollEvent &event) override
    {
        const auto point =
            whitecapPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const int knob = whitecapKnobAt(point);
        if (knob < 0)
            return false;
        const double step = event.mod & kModifierShift ? 0.002 : 0.02;
        change(static_cast<std::uint32_t>(knob),
               denormalizedWhitecap(
                   static_cast<std::uint32_t>(knob),
                   normalizedWhitecap(static_cast<std::uint32_t>(knob), values[knob]) +
                       static_cast<float>(event.delta.getY() * step)));
        return true;
    }

  private:
    float drawingScale() const
    {
        return std::min(getWidth() / static_cast<float>(WhitecapUiWidth),
                        getHeight() / static_cast<float>(WhitecapHeight));
    }

    bool enabled() const { return values[WhitecapBypass] < 0.5f; }

    void change(std::uint32_t index, float target)
    {
        editParameter(index, true);
        send(index, target);
        editParameter(index, false);
    }

    void send(std::uint32_t index, float value)
    {
        values[index] = sanitizeWhitecap(index, value);
        setParameterValue(index, values[index]);
        repaint();
    }

    void caption(float x, float y, const char *label, float size)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(34, 57, 70));
        text(x, y, label, nullptr);
    }

    void drawImage(const NanoImage &image, float x, float y, float width, float height)
    {
        if (!image.isValid())
            return;
        beginPath();
        rect(x, y, width, height);
        fillPaint(imagePattern(x, y, width, height, 0, image, 1));
        fill();
    }

    void drawEnclosure()
    {
        if (!enclosure.isValid())
            return;
        const float imageWidth = static_cast<float>(enclosure.getSize().getWidth());
        const float imageHeight = static_cast<float>(enclosure.getSize().getHeight());
        const float targetWidth = static_cast<float>(WhitecapHeight) * imageWidth / imageHeight;
        drawImage(enclosure, (static_cast<float>(WhitecapUiWidth) - targetWidth) * 0.5f, 0,
                  targetWidth, static_cast<float>(WhitecapHeight));
    }

    void drawKnob(const WhitecapKnobPosition &knob)
    {
        const auto index = knob.parameter;
        const float value = values[index];
        caption(knob.x, knob.y - 59, knob.label, 15);
        drawImage(knobMaterial, knob.x - 42, knob.y - 42, 84, 84);
        const float normalized = normalizedWhitecap(index, value);
        const float angle = (-135 + normalized * 270) * std::numbers::pi_v<float> / 180;
        beginPath();
        moveTo(knob.x + std::sin(angle) * 12, knob.y - std::cos(angle) * 12);
        lineTo(knob.x + std::sin(angle) * 24, knob.y - std::cos(angle) * 24);
        lineCap(ROUND);
        strokeColor(Color(20, 24, 25));
        strokeWidth(3);
        stroke();
        char display[24];
        switch (index) {
        case WhitecapRate:
            std::snprintf(display, sizeof(display), "%.2f Hz", value);
            break;
        case WhitecapDepth:
            std::snprintf(display, sizeof(display), "%.1f ms", value);
            break;
        case WhitecapMix:
            std::snprintf(display, sizeof(display), "%.0f %%", value);
            break;
        default:
            std::snprintf(display, sizeof(display), "%.0f deg", value);
            break;
        }
        caption(knob.x, knob.y + 53, display, 11);
    }

    void drawModeSwitch()
    {
        const float y = static_cast<float>(WhitecapModeY);
        const bool vibrato = values[WhitecapModeParameter] >= 0.5f;
        caption(190, y, "CHORUS", 11);
        caption(330, y, "VIBRATO", 11);
        const auto &material = vibrato ? toggleRight : toggleLeft;
        drawImage(material, vibrato ? 235 : 224, y - 30, 72, 59);
    }

    void drawLed()
    {
        const float y = static_cast<float>(WhitecapLedY);
        beginPath();
        circle(260, y, 11);
        fillPaint(radialGradient(257, y - 3, 1, 11,
                                 enabled() ? Color(170, 255, 223) : Color(62, 105, 96),
                                 enabled() ? Color(20, 131, 115) : Color(25, 57, 55)));
        fill();
    }

    NanoImage enclosure, knobMaterial, footswitch, toggleLeft, toggleRight;
    std::array<float, WhitecapParameterCount> values{};
    int dragging = -1;
    double lastY = 0, position = 0;
};

#ifdef STORMSURGE_GUI_HARNESS
static WhitecapUI *testUI = nullptr;

bool whitecapExerciseControls() noexcept
{
    return testUI != nullptr && testUI->exerciseControls();
}

void whitecapResizeForTest() noexcept
{
    if (testUI != nullptr)
        testUI->resizeForTest();
}
#endif

UI *createUI()
{
    auto *ui = new WhitecapUI;
#ifdef STORMSURGE_GUI_HARNESS
    testUI = ui;
#endif
    return ui;
}

END_NAMESPACE_DISTRHO
