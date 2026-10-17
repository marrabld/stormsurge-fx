// SPDX-License-Identifier: GPL-3.0-only
#include "MaelstromParameters.hpp"
#include "MaelstromArtwork.hpp"
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
using namespace stormsurge;

namespace
{
#ifdef STORMSURGE_GUI_HARNESS
class MaelstromUI;
MaelstromUI *testUI = nullptr;
#endif
} // namespace

class MaelstromUI final : public UI
{
  public:
    MaelstromUI()
        : UI(static_cast<uint32_t>(MaelstromWidth), static_cast<uint32_t>(MaelstromHeight))
    {
        loadSharedResources();
        setGeometryConstraints(390, 510, true);
        enclosure = createImageFromMemory(maelstromEnclosurePng, sizeof(maelstromEnclosurePng), 0);
        // The knob and footswitch materials are Undertow's original renders, so
        // both pedals stay visibly part of one product family.
        knobMaterial = createImageFromMemory(previewKnobPng, sizeof(previewKnobPng), 0);
        footswitchMaterial =
            createImageFromMemory(previewFootswitchPng, sizeof(previewFootswitchPng), 0);
        for (std::uint32_t i = 0; i < MaelstromParameterCount; ++i)
            values[i] = maelstromSpecs[i].initial;
#ifdef STORMSURGE_GUI_HARNESS
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
#endif
    }

#ifdef STORMSURGE_GUI_HARNESS
    bool exerciseControls()
    {
        if (!enclosure.isValid() || !knobMaterial.isValid() || !footswitchMaterial.isValid())
            return false;
        const float scale = drawingScale();
        const double dx = (getWidth() - MaelstromWidth * scale) * 0.5;
        const double dy = (getHeight() - MaelstromHeight * scale) * 0.5;
        for (const auto &knob : maelstromKnobs) {
            const float before = values[knob.parameter];
            MouseEvent mouse;
            mouse.button = 1;
            mouse.press = true;
            mouse.pos.setPos(dx + knob.x * scale, dy + knob.y * scale);
            if (!onMouse(mouse))
                return false;
            MotionEvent motion;
            motion.pos.setPos(dx + knob.x * scale, dy + (knob.y - 20) * scale);
            if (!onMotion(motion) || values[knob.parameter] <= before)
                return false;
            mouse.press = false;
            if (!onMouse(mouse))
                return false;
            ScrollEvent scroll;
            scroll.pos = mouse.pos;
            scroll.delta.setPos(0, -10000);
            if (!onScroll(scroll))
                return false;
        }
        MouseEvent foot;
        foot.button = 1;
        foot.press = true;
        foot.pos.setPos(dx + 260 * scale, dy + MaelstromFootY * scale);
        if (!onMouse(foot) || values[MaelstromBypass] != 1)
            return false;
        if (!onMouse(foot) || values[MaelstromBypass] != 0)
            return false;
        return true;
    }
#endif

  protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < MaelstromParameterCount) {
            values[index] = sanitizeMaelstrom(index, value);
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float factor = drawingScale();
        translate((getWidth() - static_cast<float>(MaelstromWidth) * factor) * 0.5f,
                  (getHeight() - static_cast<float>(MaelstromHeight) * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(260, 44, "STORMSURGE FX", 14);
        caption(260, 83, "MAELSTROM", 38);
        caption(260, 113, "TIGHTENING OVERDRIVE", 11);
        for (const auto &knob : maelstromKnobs)
            drawKnob(knob);
        drawLed();
        if (footswitchMaterial.isValid())
            image(footswitchMaterial, 218, static_cast<float>(MaelstromFootY) - 42, 84, 84);
        else
            circleFill(260, static_cast<float>(MaelstromFootY), 31, Color(158, 170, 170));
        caption(260, 582, enabled() ? "EFFECT ON" : "BYPASSED", 12);
        caption(260, 611, "DRAG OR SCROLL / SHIFT FOR FINE", 10);
        caption(260, 640, "START QUIETLY / FEED YOUR AMP", 10);
    }

    bool onMouse(const MouseEvent &event) override
    {
        if (event.button != 1)
            return false;
        if (!event.press) {
            const bool handled = dragging >= 0;
            dragging = -1;
            return handled;
        }
        dragging = -1;
        const auto point =
            maelstromPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const int knob = maelstromKnobAt(point);
        if (knob >= 0) {
            dragging = knob;
            lastY = point.y;
            return true;
        }
        if (maelstromFootAt(point)) {
            change(MaelstromBypass, values[MaelstromBypass] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &event) override
    {
        if (dragging < 0)
            return false;
        const auto point =
            maelstromPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const double sensitivity = event.mod & kModifierShift ? 0.0005 : 0.005;
        change(static_cast<std::uint32_t>(dragging),
               (lastY - point.y) * sensitivity *
                   (maelstromSpecs[dragging].maximum - maelstromSpecs[dragging].minimum));
        lastY = point.y;
        return true;
    }

    bool onScroll(const ScrollEvent &event) override
    {
        const auto point =
            maelstromPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const int knob = maelstromKnobAt(point);
        if (knob < 0)
            return false;
        const auto &spec = maelstromSpecs[knob];
        const double step = event.mod & kModifierShift ? 0.002 : 0.02;
        change(static_cast<std::uint32_t>(knob),
               event.delta.getY() * step * (spec.maximum - spec.minimum));
        return true;
    }

  private:
    float drawingScale() const
    {
        return std::min(getWidth() / static_cast<float>(MaelstromWidth),
                        getHeight() / static_cast<float>(MaelstromHeight));
    }

    bool enabled() const { return values[MaelstromBypass] < 0.5f; }

    void change(std::uint32_t index, double target)
    {
        if (!std::isfinite(target))
            target = maelstromSpecs[index].initial;
        // Host gestures bracket a knob drag so automation can follow it.
        editParameter(index, true);
        const float clamped = std::clamp(static_cast<float>(target), maelstromSpecs[index].minimum,
                                         maelstromSpecs[index].maximum);
        values[index] = sanitizeMaelstrom(index, clamped);
        setParameterValue(index, values[index]);
        editParameter(index, false);
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

    void circleFill(float x, float y, float radius, Color color)
    {
        beginPath();
        circle(x, y, radius);
        fillColor(color);
        fill();
    }

    void image(const NanoImage &material, float x, float y, float width, float height)
    {
        beginPath();
        rect(x, y, width, height);
        fillPaint(imagePattern(x, y, width, height, 0, material, 1));
        fill();
    }

    void drawEnclosure()
    {
        if (!enclosure.isValid())
            return;
        const float width = enclosure.getSize().getWidth();
        const float height = enclosure.getSize().getHeight();
        // Nine slices retain the rim and corner proportions of the prepared
        // texture instead of stretching it across the shorter face.
        const float sourceX[]{0, 64, width - 64, width};
        const float sourceY[]{0, 64, height - 64, height};
        const float destX[]{8, 40, 480, 512};
        const float destY[]{8, 40, static_cast<float>(MaelstromHeight) - 40,
                            static_cast<float>(MaelstromHeight) - 8};
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const float sx =
                    (destX[column + 1] - destX[column]) / (sourceX[column + 1] - sourceX[column]);
                const float sy = (destY[row + 1] - destY[row]) / (sourceY[row + 1] - sourceY[row]);
                beginPath();
                rect(destX[column], destY[row], destX[column + 1] - destX[column],
                     destY[row + 1] - destY[row]);
                fillPaint(imagePattern(destX[column] - sourceX[column] * sx,
                                       destY[row] - sourceY[row] * sy, width * sx, height * sy, 0,
                                       enclosure, 1));
                fill();
            }
        }
    }

    void drawKnob(const MaelstromKnobPosition &knob)
    {
        const auto index = knob.parameter;
        const float value = values[index];
        caption(knob.x, knob.y - 59, knob.label, 16);
        image(knobMaterial, knob.x - 42, knob.y - 42, 84, 84);
        const float position = normalizedMaelstrom(index, value);
        const float angle = (-135 + position * 270) * std::numbers::pi_v<float> / 180;
        beginPath();
        moveTo(knob.x + std::sin(angle) * 12, knob.y - std::cos(angle) * 12);
        lineTo(knob.x + std::sin(angle) * 24, knob.y - std::cos(angle) * 24);
        lineCap(ROUND);
        strokeColor(Color(20, 24, 25));
        strokeWidth(3);
        stroke();
        char display[32];
        if (index == MaelstromLevel) {
            // Linear amplitude is what the host stores, so read it out in dB.
            if (value <= 0.0f)
                std::snprintf(display, sizeof(display), "Silence");
            else
                std::snprintf(display, sizeof(display), "%+.1f dB", 20.0f * std::log10(value));
        } else
            std::snprintf(display, sizeof(display), "%.0f %%", value);
        caption(knob.x, knob.y + 53, display, 12);
    }

    void drawLed()
    {
        const bool on = enabled();
        beginPath();
        circle(260, static_cast<float>(MaelstromLedY), 11);
        fillPaint(linearGradient(250, static_cast<float>(MaelstromLedY) - 10, 269,
                                 static_cast<float>(MaelstromLedY) + 11, Color(9, 8, 8),
                                 Color(125, 116, 105)));
        fill();
        beginPath();
        circle(260, static_cast<float>(MaelstromLedY), 7);
        fillPaint(radialGradient(258, static_cast<float>(MaelstromLedY) - 2, 1, 8,
                                 on ? Color(255, 169, 99) : Color(74, 36, 27),
                                 on ? Color(119, 29, 13) : Color(25, 14, 12)));
        fill();
    }

    DGL_NAMESPACE::NanoImage enclosure;
    DGL_NAMESPACE::NanoImage knobMaterial;
    DGL_NAMESPACE::NanoImage footswitchMaterial;
    std::array<float, MaelstromParameterCount> values{};
    int dragging = -1;
    double lastY = 0;
};

#ifdef STORMSURGE_GUI_HARNESS
MaelstromUI *maelstromTestUI() noexcept
{
    return testUI;
}
void setMaelstromTestUI(MaelstromUI *ui) noexcept
{
    testUI = ui;
}
#endif

UI *createUI()
{
    auto *ui = new MaelstromUI;
#ifdef STORMSURGE_GUI_HARNESS
    testUI = ui;
#endif
    return ui;
}

END_NAMESPACE_DISTRHO
