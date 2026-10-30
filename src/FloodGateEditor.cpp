// SPDX-License-Identifier: GPL-3.0-only
#include "FloodGateParameters.hpp"
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

class FloodGateUI final : public UI
{
  public:
    FloodGateUI()
        : UI(static_cast<uint32_t>(FloodGateWidth), static_cast<uint32_t>(FloodGateHeight))
    {
        loadSharedResources();
        setGeometryConstraints(390, 510, true);
        // No FloodGate artwork has been generated or approved yet, so the enclosure is drawn
        // in code in a colour no other pedal uses. The LEDs are Maelstrom's approved renders and
        // the knob and footswitch are Undertow's originals; no new image is bundled.
        ledOff = createImageFromMemory(maelstromLedOffPng, sizeof(maelstromLedOffPng), 0);
        ledOn = createImageFromMemory(maelstromLedOnPng, sizeof(maelstromLedOnPng), 0);
        knobMaterial = createImageFromMemory(previewKnobPng, sizeof(previewKnobPng), 0);
        footswitch = createImageFromMemory(previewFootswitchPng, sizeof(previewFootswitchPng), 0);
        for (std::uint32_t i = 0; i < FloodGateParameterCount; ++i)
            values[i] = floodGateSpecs[i].initial;
#ifdef STORMSURGE_GUI_HARNESS
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
#endif
    }

#ifdef STORMSURGE_GUI_HARNESS
    bool exerciseControls()
    {
        if (!knobMaterial.isValid() || !footswitch.isValid() || !ledOff.isValid() ||
            !ledOn.isValid())
            return false;
        for (const auto &knob : floodGateKnobs) {
            const float before = values[knob.parameter];
            MouseEvent mouse;
            mouse.button = 1;
            mouse.press = true;
            mouse.pos = windowPoint(knob.x, knob.y);
            if (!onMouse(mouse))
                return false;
            MotionEvent motion;
            motion.pos = windowPoint(knob.x, knob.y - 20);
            if (!onMotion(motion) || values[knob.parameter] <= before)
                return false;
            mouse.press = false;
            if (!onMouse(mouse))
                return false;
        }
        MouseEvent foot;
        foot.button = 1;
        foot.press = true;
        foot.pos = windowPoint(FloodGateWidth * 0.5, FloodGateFootY);
        if (!onMouse(foot) || values[FloodGateBypass] != 1.0f)
            return false;
        if (!onMouse(foot) || values[FloodGateBypass] != 0.0f)
            return false;
        return true;
    }

    void resizeForTest() { setSize(390, 510); }
#endif

  protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < FloodGateParameterCount) {
            values[index] = sanitizeFloodGate(index, value);
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float factor = drawingScale();
        translate((getWidth() - static_cast<float>(FloodGateWidth) * factor) * 0.5f,
                  (getHeight() - static_cast<float>(FloodGateHeight) * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(260, 44, "STORMSURGE FX", 14);
        caption(260, 83, "FLOODGATE", 38);
        caption(260, 113, "NOISE GATE", 11);
        for (const auto &knob : floodGateKnobs)
            drawKnob(knob);
        drawImage(enabled() ? ledOn : ledOff, 236, static_cast<float>(FloodGateLedY) - 24, 48, 48);
        drawImage(footswitch, 218, static_cast<float>(FloodGateFootY) - 42, 84, 84);
        caption(260, 582, enabled() ? "EFFECT ON" : "BYPASSED", 12);
        caption(260, 611, "DRAG OR SCROLL / SHIFT FOR FINE", 10);
        caption(260, 640, "V0.1 / SET BY EAR", 10);
    }

    bool onMouse(const MouseEvent &event) override
    {
        if (event.button != 1)
            return false;
        if (!event.press) {
            const int parameter = dragging;
            dragging = -1;
            if (parameter >= 0)
                editParameter(static_cast<std::uint32_t>(parameter), false);
            return parameter >= 0;
        }
        if (dragging >= 0) {
            editParameter(static_cast<std::uint32_t>(dragging), false);
            dragging = -1;
        }
        const auto point =
            floodGatePoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const int knob = floodGateKnobAt(point);
        if (knob >= 0) {
            dragging = knob;
            lastY = point.y;
            // Track the unquantised position so a log-scaled knob keeps moving under slow drags.
            position = normalizedFloodGate(static_cast<std::uint32_t>(knob), values[knob]);
            editParameter(static_cast<std::uint32_t>(knob), true);
            return true;
        }
        if (floodGateFootAt(point)) {
            change(FloodGateBypass, enabled() ? 1.0f : 0.0f);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &event) override
    {
        if (dragging < 0)
            return false;
        const auto point =
            floodGatePoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const double sensitivity = event.mod & kModifierShift ? 0.001 : 0.006;
        position = std::clamp(position + (lastY - point.y) * sensitivity, 0.0, 1.0);
        lastY = point.y;
        const auto index = static_cast<std::uint32_t>(dragging);
        send(index, denormalizedFloodGate(index, static_cast<float>(position)));
        return true;
    }

    bool onScroll(const ScrollEvent &event) override
    {
        const auto point =
            floodGatePoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const int knob = floodGateKnobAt(point);
        if (knob < 0)
            return false;
        const auto index = static_cast<std::uint32_t>(knob);
        const double step = event.mod & kModifierShift ? 0.002 : 0.02;
        change(index,
               denormalizedFloodGate(index, normalizedFloodGate(index, values[index]) +
                                                static_cast<float>(event.delta.getY() * step)));
        return true;
    }

  private:
    float drawingScale() const
    {
        return std::min(getWidth() / static_cast<float>(FloodGateWidth),
                        getHeight() / static_cast<float>(FloodGateHeight));
    }

    DGL_NAMESPACE::Point<double> windowPoint(double x, double y) const
    {
        const float factor = drawingScale();
        return {x * factor + (getWidth() - FloodGateWidth * factor) * 0.5,
                y * factor + (getHeight() - FloodGateHeight * factor) * 0.5};
    }

    bool enabled() const { return values[FloodGateBypass] < 0.5f; }

    void change(std::uint32_t index, float target)
    {
        editParameter(index, true);
        send(index, target);
        editParameter(index, false);
    }

    void send(std::uint32_t index, float value)
    {
        values[index] = sanitizeFloodGate(index, value);
        setParameterValue(index, values[index]);
        repaint();
    }

    void caption(float x, float y, const char *label, float size)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(225, 218, 200));
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
        const float width = static_cast<float>(FloodGateWidth);
        const float height = static_cast<float>(FloodGateHeight);
        // Deep violet: navy, white, slate, teal and red are already taken by the other pedals,
        // so FloodGate stays recognisable on a board at a glance.
        beginPath();
        roundedRect(8, 8, width - 16, height - 16, 30);
        fillPaint(linearGradient(0, 8, 0, height - 8, Color(178, 176, 184), Color(74, 70, 82)));
        fill();
        beginPath();
        roundedRect(16, 16, width - 32, height - 32, 24);
        fillPaint(linearGradient(0, 16, width, height - 16, Color(84, 50, 112), Color(38, 22, 58)));
        fill();
        // Soft inner shading so the face reads as a painted, slightly domed panel.
        beginPath();
        roundedRect(16, 16, width - 32, height - 32, 24);
        fillPaint(boxGradient(16, 16, width - 32, height - 32, 24, 70, Color(0, 0, 0, 0),
                              Color(0, 0, 0, 110)));
        fill();
        beginPath();
        roundedRect(16.5f, 16.5f, width - 33, height - 33, 24);
        strokeColor(Color(255, 255, 255, 40));
        strokeWidth(1);
        stroke();
    }

    void drawKnob(const FloodGateKnobPosition &knob)
    {
        const auto index = knob.parameter;
        caption(knob.x, knob.y - 59, knob.label, 16);
        drawImage(knobMaterial, knob.x - 42, knob.y - 42, 84, 84);
        const float normalized = normalizedFloodGate(index, values[index]);
        const float angle = (-135 + normalized * 270) * std::numbers::pi_v<float> / 180;
        beginPath();
        moveTo(knob.x + std::sin(angle) * 12, knob.y - std::cos(angle) * 12);
        lineTo(knob.x + std::sin(angle) * 24, knob.y - std::cos(angle) * 24);
        lineCap(ROUND);
        strokeColor(Color(20, 24, 25));
        strokeWidth(3);
        stroke();
        char display[24];
        if (index == FloodGateRelease)
            std::snprintf(display, sizeof(display), "%.0f ms", values[index]);
        else
            std::snprintf(display, sizeof(display), "%.0f dB", values[index]);
        caption(knob.x, knob.y + 53, display, 12);
    }

    NanoImage knobMaterial, footswitch, ledOff, ledOn;
    std::array<float, FloodGateParameterCount> values{};
    int dragging = -1;
    double lastY = 0, position = 0;
};

#ifdef STORMSURGE_GUI_HARNESS
static FloodGateUI *testUI = nullptr;

bool floodGateExerciseControls() noexcept
{
    return testUI != nullptr && testUI->exerciseControls();
}

void floodGateResizeForTest() noexcept
{
    if (testUI != nullptr)
        testUI->resizeForTest();
}
#endif

UI *createUI()
{
    auto *ui = new FloodGateUI;
#ifdef STORMSURGE_GUI_HARNESS
    testUI = ui;
#endif
    return ui;
}

END_NAMESPACE_DISTRHO
