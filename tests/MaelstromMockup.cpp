// SPDX-License-Identifier: GPL-3.0-only
#include "src/DistrhoUIInternal.hpp"
#include "MaelstromParameters.hpp"
#include "PreviewArtwork.hpp"
#include "MaelstromArtwork.hpp"
#include "OpenGL.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numbers>
#include <thread>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using namespace stormsurge;

namespace
{
constexpr float Width = 520, Height = 680;
constexpr float FootX = 260, FootY = 520;
struct Knob {
    float x, y;
    const char *label;
};
constexpr std::array<Knob, 4> knobs{
    {{155, 205, "DRIVE"}, {365, 205, "TIGHT"}, {155, 355, "BITE"}, {365, 355, "LEVEL"}}};

class MaelstromMockup final : public UI
{
  public:
    MaelstromMockup() : UI(520, 680)
    {
        loadSharedResources();
        setGeometryConstraints(390, 510, true);
        enclosure = createImageFromMemory(maelstromEnclosurePng, sizeof(maelstromEnclosurePng), 0);
        ledOff = createImageFromMemory(maelstromLedOffPng, sizeof(maelstromLedOffPng), 0);
        ledOn = createImageFromMemory(maelstromLedOnPng, sizeof(maelstromLedOnPng), 0);
        toggleLeft =
            createImageFromMemory(maelstromToggleLeftPng, sizeof(maelstromToggleLeftPng), 0);
        toggleRight =
            createImageFromMemory(maelstromToggleRightPng, sizeof(maelstromToggleRightPng), 0);
        knobMaterial = createImageFromMemory(previewKnobPng, sizeof(previewKnobPng), 0);
        footMaterial = createImageFromMemory(previewFootswitchPng, sizeof(previewFootswitchPng), 0);
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
    }

    bool exerciseControls()
    {
        if (!enclosure.isValid() || !knobMaterial.isValid() || !footMaterial.isValid() ||
            !ledOff.isValid() || !ledOn.isValid() || !toggleLeft.isValid() ||
            !toggleRight.isValid())
            return false;
        const auto initial = values;
        for (unsigned i = 0; i < knobs.size(); ++i) {
            MouseEvent click;
            click.button = 1;
            click.press = true;
            click.pos = windowPoint(knobs[i].x, knobs[i].y);
            if (!onMouse(click))
                return false;
            MotionEvent drag;
            drag.pos = windowPoint(knobs[i].x, knobs[i].y - 20);
            if (!onMotion(drag) || values[i] <= initial[i])
                return false;
            click.press = false;
            if (!onMouse(click))
                return false;
            ScrollEvent scroll;
            scroll.pos = click.pos;
            scroll.delta.setPos(0, 1);
            scroll.mod = kModifierShift;
            const float beforeFine = values[i];
            if (!onScroll(scroll) || std::abs(values[i] - beforeFine - 0.002f) > 0.0001f)
                return false;
            scroll.mod = 0;
            scroll.delta.setPos(0, -10000);
            if (!onScroll(scroll) || values[i] != 0)
                return false;
            scroll.delta.setPos(0, 10000);
            if (!onScroll(scroll) || values[i] != 1)
                return false;
        }
        MouseEvent foot;
        foot.button = 1;
        foot.press = true;
        foot.pos = windowPoint(FootX, FootY);
        if (!onMouse(foot) || enabled || !onMouse(foot) || !enabled)
            return false;
        MouseEvent mode;
        mode.button = 1;
        mode.press = true;
        mode.pos = windowPoint(260, MaelstromModeY);
        if (!onMouse(mode) || !grit || !onMouse(mode) || grit)
            return false;
        values = initial;
        repaint();
        return true;
    }

  protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        // The 390-pixel capture needs tightly packed RGB rows in DPF's PPM writer.
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        const float factor = drawingScale();
        translate((getWidth() - Width * factor) * 0.5f, (getHeight() - Height * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(260, 44, "STORMSURGE FX", 14);
        caption(260, 83, "MAELSTROM", 38);
        caption(260, 113, "TIGHTENING OVERDRIVE", 11);
        for (unsigned i = 0; i < knobs.size(); ++i)
            drawKnob(i);
        drawLed();
        drawModeSwitch();
        image(footMaterial, FootX - 42, FootY - 42, 84, 84);
        caption(260, 582, enabled ? "EFFECT ON" : "BYPASSED", 12);
        caption(260, 611, "DRAG OR SCROLL / SHIFT FOR FINE", 10);
        caption(260, 640, "VISUAL MOCKUP / NO AUDIO", 10);
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
        const auto point = pedalPoint(event.pos);
        for (unsigned i = 0; i < knobs.size(); ++i) {
            if (distanceSquared(point, knobs[i].x, knobs[i].y) <= 42 * 42) {
                dragging = static_cast<int>(i);
                lastY = point.getY();
                return true;
            }
        }
        if (distanceSquared(point, FootX, FootY) <= 42 * 42) {
            enabled = !enabled;
            repaint();
            return true;
        }
        if (maelstromModeAt({point.getX(), point.getY()})) {
            grit = !grit;
            repaint();
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &event) override
    {
        if (dragging < 0)
            return false;
        const double y = pedalPoint(event.pos).getY();
        const double sensitivity = event.mod & kModifierShift ? 0.0005 : 0.005;
        change(static_cast<unsigned>(dragging), (lastY - y) * sensitivity);
        lastY = y;
        return true;
    }

    bool onScroll(const ScrollEvent &event) override
    {
        const auto point = pedalPoint(event.pos);
        for (unsigned i = 0; i < knobs.size(); ++i) {
            if (distanceSquared(point, knobs[i].x, knobs[i].y) <= 42 * 42) {
                change(i, event.delta.getY() * (event.mod & kModifierShift ? 0.002 : 0.02));
                return true;
            }
        }
        return false;
    }

  private:
    float drawingScale() const { return std::min(getWidth() / Width, getHeight() / Height); }

    Point<double> pedalPoint(const Point<double> &point) const
    {
        const float factor = drawingScale();
        return {(point.getX() - (getWidth() - Width * factor) * 0.5) / factor,
                (point.getY() - (getHeight() - Height * factor) * 0.5) / factor};
    }

    Point<double> windowPoint(float x, float y) const
    {
        const float factor = drawingScale();
        return {x * factor + (getWidth() - Width * factor) * 0.5,
                y * factor + (getHeight() - Height * factor) * 0.5};
    }

    static double distanceSquared(const Point<double> &point, float x, float y)
    {
        return std::pow(point.getX() - x, 2) + std::pow(point.getY() - y, 2);
    }

    void change(unsigned index, double delta)
    {
        // Local visual state only. This prototype sends no host edits or audio commands.
        values[index] = std::clamp(values[index] + static_cast<float>(delta), 0.0f, 1.0f);
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

    void image(const NanoImage &material, float x, float y, float width, float height)
    {
        beginPath();
        rect(x, y, width, height);
        fillPaint(imagePattern(x, y, width, height, 0, material, 1));
        fill();
    }

    void drawEnclosure()
    {
        const float width = enclosure.getSize().getWidth();
        const float height = enclosure.getSize().getHeight();
        // Match Undertow's fixed-size rim/corners while allowing a shorter face.
        const float sourceX[]{0, 64, width - 64, width};
        const float sourceY[]{0, 64, height - 64, height};
        const float destX[]{8, 40, 480, 512};
        const float destY[]{8, 40, Height - 40, Height - 8};
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

    void drawKnob(unsigned index)
    {
        const auto &knob = knobs[index];
        caption(knob.x, knob.y - 59, knob.label, 16);
        image(knobMaterial, knob.x - 42, knob.y - 42, 84, 84);
        const float angle = (-135 + values[index] * 270) * std::numbers::pi_v<float> / 180;
        beginPath();
        moveTo(knob.x + std::sin(angle) * 12, knob.y - std::cos(angle) * 12);
        lineTo(knob.x + std::sin(angle) * 24, knob.y - std::cos(angle) * 24);
        lineCap(ROUND);
        strokeColor(Color(20, 24, 25));
        strokeWidth(3);
        stroke();
        char display[32];
        if (index == 3) {
            // A visual travel placeholder, not an approved DSP/host parameter taper.
            if (values[index] == 0)
                std::snprintf(display, sizeof(display), "Silence");
            else
                std::snprintf(display, sizeof(display), "%+.1f dB", -36 + values[index] * 48);
        } else {
            std::snprintf(display, sizeof(display), "%.0f %%", values[index] * 100);
        }
        caption(knob.x, knob.y + 53, display, 12);
    }

    void drawLed()
    {
        const auto &material = enabled ? ledOn : ledOff;
        image(material, 236, 423, 48, 48);
    }

    void drawModeSwitch()
    {
        caption(194, MaelstromModeY, "SMOOTH", 11);
        caption(326, MaelstromModeY, "GRIT", 11);
        const double x = grit ? MaelstromToggleRightX : MaelstromToggleLeftX;
        image(grit ? toggleRight : toggleLeft, x, MaelstromModeY - 30, 72, 59);
    }

    NanoImage enclosure, knobMaterial, footMaterial, ledOff, ledOn, toggleLeft, toggleRight;
    std::array<float, 4> values{{0.1f, 0.5f, 0.5f, 0.75f}};
    int dragging = -1;
    double lastY = 0;
    bool enabled = true;
    bool grit = false;
};

MaelstromMockup *mockup = nullptr;
} // namespace

const char *getPluginFormatName() noexcept
{
    return "GUI mockup / no audio";
}

UI *createUI()
{
    mockup = new MaelstromMockup;
    return mockup;
}

END_NAMESPACE_DISTRHO

int main(int argc, char **argv)
{
    const bool view = argc == 2 && std::strcmp(argv[1], "--view") == 0;
    const bool minimumSize = argc == 2 && std::strcmp(argv[1], "--small") == 0;
    if (argc != 1 && !view && !minimumSize) {
        std::fprintf(stderr, "Usage: maelstrom_mockup [--view|--small]\n");
        return 1;
    }
    using namespace DISTRHO;
    UIExporter editor(nullptr, 0, 48000.0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr);
    editor.setWindowTitle("Maelstrom: visual mockup, no audio");
    if (minimumSize)
        mockup->setSize(390, 510);
    editor.setWindowVisible(true);
    editor.focus();
    if (!mockup->exerciseControls()) {
        std::fprintf(stderr, "FAIL: artwork loading or local mockup controls\n");
        return 1;
    }
    uint width = 0, height = 0;
    bool aspect = false;
    editor.getGeometryConstraints(width, height, aspect);
    if (width != 390 || height != 510 || !aspect ||
        (minimumSize && (editor.getWidth() != 390 || editor.getHeight() != 510))) {
        std::fprintf(stderr, "FAIL: mockup geometry\n");
        return 1;
    }
    for (int frame = 0; view || frame < 30; ++frame) {
        if (view && !editor.isVisible())
            break;
        if (!editor.plugin_idle()) {
            if (!view) {
                std::fprintf(stderr, "FAIL: mockup closed before rendering completed\n");
                return 1;
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    editor.quit();
    std::puts(
        "PASS: Maelstrom mockup artwork, local controls and geometry; no DSP or audio device");
    return 0;
}
