// SPDX-License-Identifier: GPL-3.0-only
#include "RiptideParameters.hpp"
#include "ConceptMockupArtwork.hpp"
#include "PreviewArtwork.hpp"
#include "DistrhoUI.hpp"
#include "DistrhoPluginUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::NanoImage;
using namespace stormsurge;

class RiptideUI final : public UI
{
  public:
    RiptideUI() : UI(static_cast<uint32_t>(RiptideUiWidth), static_cast<uint32_t>(RiptideUiHeight))
    {
        loadSharedResources();
        setGeometryConstraints(390, 530, true);
        enclosure = createImageFromMemory(riptideEnclosurePng, sizeof(riptideEnclosurePng), 0);
        knobMaterial = createImageFromMemory(previewKnobPng, sizeof(previewKnobPng), 0);
        footswitch = createImageFromMemory(previewFootswitchPng, sizeof(previewFootswitchPng), 0);
        ledOff = createImageFromMemory(previewLedOffPng, sizeof(previewLedOffPng), 0);
        ledOn = createImageFromMemory(previewLedOnPng, sizeof(previewLedOnPng), 0);
        for (std::uint32_t i = 0; i < RiptidePluginParameterCount; ++i)
            values[i] = riptideSpecs[i].initial;
    }

#ifdef STORMSURGE_GUI_HARNESS
    bool exerciseControls()
    {
        if (!enclosure.isValid() || !knobMaterial.isValid() || !footswitch.isValid() ||
            !ledOff.isValid() || !ledOn.isValid())
            return false;
        MouseEvent event;
        event.button = 1;
        event.press = true;
        event.pos = windowPoint(riptideShiftX, RiptideKnobY);
        if (!onMouse(event))
            return false;
        MotionEvent motion;
        motion.pos = windowPoint(riptideShiftX, RiptideKnobY - 40);
        if (!onMotion(motion) || values[RiptideShiftParameter] >= 0.0f)
            return false;
        event.press = false;
        event.pos = windowPoint(riptideShiftX, RiptideKnobY - 40);
        if (!onMouse(event))
            return false;
        event.press = true;
        event.pos = windowPoint(RiptideUiWidth * 0.5, RiptideFootY);
        if (!onMouse(event) || values[RiptideBypassParameter] != 1.0f)
            return false;
        if (!onMouse(event) || values[RiptideBypassParameter] != 0.0f)
            return false;
        return true;
    }

    void resizeForTest() { setSize(390, 530); }
#endif

  protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < RiptidePluginParameterCount) {
            values[index] = sanitizeRiptideParameter(index, value);
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float factor = drawingScale();
        translate((getWidth() - static_cast<float>(RiptideUiWidth) * factor) * 0.5f,
                  (getHeight() - static_cast<float>(RiptideUiHeight) * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(250, 50, "STORMSURGE FX", 12);
        caption(250, 90, "RIPTIDE", 36);
        caption(250, 119, "PITCH-DOWN TUNING", 10);
        drawReadout();
        drawKnob(RiptideShiftParameter, "SHIFT", static_cast<float>(riptideShiftX));
        drawKnob(RiptideBlendParameter, "BLEND", static_cast<float>(riptideBlendX));
        drawImage(enabled() ? ledOn : ledOff, 236, static_cast<float>(RiptideLedY) - 14, 28, 28);
        drawImage(footswitch, 208, static_cast<float>(RiptideFootY) - 42, 84, 84);
        caption(250, 612, enabled() ? "EFFECT ON" : "BYPASSED", 10);
        caption(250, 640, "V0.1 / TUNE BY EAR", 9);
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
            riptideUiPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        for (std::uint32_t index = 0; index < 2; ++index) {
            if (riptideKnobAt(point, index)) {
                dragging = static_cast<int>(index);
                lastY = point.y;
                editParameter(index, true);
                return true;
            }
        }
        if (riptideFootAt(point)) {
            change(RiptideBypassParameter, enabled() ? 1.0f : 0.0f);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &event) override
    {
        if (dragging < 0)
            return false;
        const auto point =
            riptideUiPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const double sensitivity = event.mod & kModifierShift ? 0.001 : 0.005;
        const auto index = static_cast<std::uint32_t>(dragging);
        double delta = (lastY - point.y) * sensitivity;
        // Turning the Shift knob clockwise must add shift, which means moving
        // toward the parameter's negative end. Blend keeps the usual direction.
        if (index == RiptideShiftParameter)
            delta = -delta;
        const double next = normalizedRiptide(index, values[index]) + delta;
        lastY = point.y;
        send(index, denormalizedRiptide(index, static_cast<float>(next)));
        return true;
    }

    bool onScroll(const ScrollEvent &event) override
    {
        const auto point =
            riptideUiPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        for (std::uint32_t index = 0; index < 2; ++index) {
            if (!riptideKnobAt(point, index))
                continue;
            const double step = event.mod & kModifierShift ? 0.002 : 0.02;
            double delta = event.delta.getY() * step;
            if (index == RiptideShiftParameter)
                delta = -delta;
            change(index, denormalizedRiptide(index, normalizedRiptide(index, values[index]) +
                                                         static_cast<float>(delta)));
            return true;
        }
        return false;
    }

  private:
    float drawingScale() const
    {
        return std::min(getWidth() / static_cast<float>(RiptideUiWidth),
                        getHeight() / static_cast<float>(RiptideUiHeight));
    }

    DGL_NAMESPACE::Point<double> windowPoint(double x, double y) const
    {
        const float factor = drawingScale();
        return {x * factor + (getWidth() - RiptideUiWidth * factor) * 0.5,
                y * factor + (getHeight() - RiptideUiHeight * factor) * 0.5};
    }

    bool enabled() const { return values[RiptideBypassParameter] < 0.5f; }

    float knobNormalized(std::uint32_t index) const
    {
        const float normalized = normalizedRiptide(index, values[index]);
        return index == RiptideShiftParameter ? 1.0f - normalized : normalized;
    }

    void change(std::uint32_t index, float target)
    {
        editParameter(index, true);
        send(index, target);
        editParameter(index, false);
    }

    void send(std::uint32_t index, float value)
    {
        values[index] = sanitizeRiptideParameter(index, value);
        setParameterValue(index, values[index]);
        repaint();
    }

    void caption(float x, float y, const char *label, float size)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(226, 233, 230));
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
        const float targetWidth = static_cast<float>(RiptideUiHeight) * imageWidth / imageHeight;
        drawImage(enclosure, (static_cast<float>(RiptideUiWidth) - targetWidth) * 0.5f, 0,
                  targetWidth, static_cast<float>(RiptideUiHeight));
    }

    void drawReadout()
    {
        char display[24];
        std::snprintf(display, sizeof(display), "%.0f ST", values[RiptideShiftParameter]);
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(44);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(179, 224, 213));
        text(250, 184, display, nullptr);
        caption(250, 215, "SEMITONES DOWN", 9);
    }

    void drawKnob(std::uint32_t index, const char *label, float x)
    {
        caption(x, RiptideKnobY - 59, label, 15);
        drawImage(knobMaterial, x - 42, static_cast<float>(RiptideKnobY) - 42, 84, 84);
        const float normalized = knobNormalized(index);
        const float angle = (-135 + normalized * 270) * std::numbers::pi_v<float> / 180;
        beginPath();
        moveTo(x + std::sin(angle) * 12, RiptideKnobY - std::cos(angle) * 12);
        lineTo(x + std::sin(angle) * 24, RiptideKnobY - std::cos(angle) * 24);
        lineCap(ROUND);
        strokeColor(Color(20, 24, 25));
        strokeWidth(3);
        stroke();
        char display[24];
        if (index == RiptideShiftParameter)
            std::snprintf(display, sizeof(display), "%.0f ST", values[index]);
        else
            std::snprintf(display, sizeof(display), "%.0f %%", values[index]);
        caption(x, RiptideKnobY + 58, display, 11);
    }

    NanoImage enclosure, knobMaterial, footswitch, ledOff, ledOn;
    std::array<float, RiptidePluginParameterCount> values{};
    int dragging = -1;
    double lastY = 0;
};

#ifdef STORMSURGE_GUI_HARNESS
static RiptideUI *testUI = nullptr;

bool riptideExerciseControls() noexcept
{
    return testUI != nullptr && testUI->exerciseControls();
}

void riptideResizeForTest() noexcept
{
    if (testUI != nullptr)
        testUI->resizeForTest();
}
#endif

UI *createUI()
{
    auto *ui = new RiptideUI;
#ifdef STORMSURGE_GUI_HARNESS
    testUI = ui;
#endif
    return ui;
}

END_NAMESPACE_DISTRHO
