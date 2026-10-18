// SPDX-License-Identifier: GPL-3.0-only
#include "src/DistrhoUIInternal.hpp"
#include "ConceptMockupArtwork.hpp"
#include "MaelstromArtwork.hpp"
#include "OpenGL.hpp"
#include "PreviewArtwork.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using namespace stormsurge;

namespace
{
constexpr float Width = 520, Height = 680;
constexpr std::array<float, 4> KnobX{155, 365, 155, 365};
constexpr std::array<float, 4> KnobY{205, 205, 355, 355};
enum class Pedal { Whitecap, Riptide };
Pedal selectedPedal = Pedal::Whitecap;

class FuturePedalMockup final : public UI
{
  public:
    explicit FuturePedalMockup(Pedal type)
        : UI(520, 680), pedal(type), lightText(type == Pedal::Riptide)
    {
        loadSharedResources();
        setGeometryConstraints(390, 510, true);
        if (pedal == Pedal::Whitecap)
            enclosure =
                createImageFromMemory(whitecapEnclosurePng, sizeof(whitecapEnclosurePng), 0);
        else
            enclosure = createImageFromMemory(riptideEnclosurePng, sizeof(riptideEnclosurePng), 0);
        knobMaterial = createImageFromMemory(previewKnobPng, sizeof(previewKnobPng), 0);
        footMaterial = createImageFromMemory(previewFootswitchPng, sizeof(previewFootswitchPng), 0);
        ledOff = createImageFromMemory(maelstromLedOffPng, sizeof(maelstromLedOffPng), 0);
        ledOn = createImageFromMemory(maelstromLedOnPng, sizeof(maelstromLedOnPng), 0);
        toggleLeft =
            createImageFromMemory(maelstromToggleLeftPng, sizeof(maelstromToggleLeftPng), 0);
        toggleRight =
            createImageFromMemory(maelstromToggleRightPng, sizeof(maelstromToggleRightPng), 0);
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
    }

    bool exerciseControls()
    {
        if (!enclosure.isValid() || !knobMaterial.isValid() || !footMaterial.isValid() ||
            !ledOff.isValid() || !ledOn.isValid() || !toggleLeft.isValid() ||
            !toggleRight.isValid())
            return false;
        MouseEvent event;
        event.button = 1;
        event.press = true;
        event.pos = windowPoint(KnobX[0], KnobY[0]);
        if (!onMouse(event))
            return false;
        MotionEvent motion;
        motion.pos = windowPoint(KnobX[0], KnobY[0] - 20);
        if (!onMotion(motion) || values[0] <= 0.5f)
            return false;
        event.press = false;
        if (!onMouse(event))
            return false;
        event.press = true;
        event.pos = windowPoint(260, 560);
        if (!onMouse(event) || enabled || !onMouse(event) || !enabled)
            return false;
        if (pedal == Pedal::Whitecap) {
            event.pos = windowPoint(260, 448);
            if (!onMouse(event) || !vibrato || !onMouse(event) || vibrato)
                return false;
        }
        return true;
    }

  protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        const float factor = drawingScale();
        translate((getWidth() - Width * factor) * 0.5f, (getHeight() - Height * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(260, 39, "STORMSURGE FX", 13);
        caption(260, 77, pedal == Pedal::Whitecap ? "WHITECAP" : "RIPTIDE", 34);
        caption(260, 105,
                pedal == Pedal::Whitecap ? "CHORUS / VIBRATO CONCEPT" : "PITCH-SHIFT CONCEPT", 10);
        for (unsigned i = 0; i < values.size(); ++i)
            drawKnob(i);
        if (pedal == Pedal::Whitecap)
            drawModeToggle();
        else
            caption(260, 448, "UP / DOWN PITCH MOVEMENT", 11);
        drawImage(enabled ? ledOn : ledOff, 245, 485, 30, 30);
        drawImage(footMaterial, 218, 518, 84, 84);
        caption(260, 615, enabled ? "LOCAL VISUAL STATE: ON" : "LOCAL VISUAL STATE: OFF", 10);
        caption(260, 642, "CONCEPT MOCKUP / NO AUDIO / CONTROLS PROVISIONAL", 9);
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
        for (unsigned i = 0; i < values.size(); ++i) {
            if (distanceSquared(point, KnobX[i], KnobY[i]) <= 42 * 42) {
                dragging = static_cast<int>(i);
                lastY = point.getY();
                return true;
            }
        }
        if (distanceSquared(point, 260, 560) <= 42 * 42) {
            enabled = !enabled;
            repaint();
            return true;
        }
        if (pedal == Pedal::Whitecap && point.getX() >= 224 && point.getX() <= 296 &&
            point.getY() >= 418 && point.getY() <= 477) {
            vibrato = !vibrato;
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
        values[dragging] = std::clamp(
            values[dragging] + static_cast<float>((lastY - y) * sensitivity), 0.0f, 1.0f);
        lastY = y;
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent &event) override
    {
        const auto point = pedalPoint(event.pos);
        for (unsigned i = 0; i < values.size(); ++i) {
            if (distanceSquared(point, KnobX[i], KnobY[i]) <= 42 * 42) {
                const float step = event.mod & kModifierShift ? 0.002f : 0.02f;
                values[i] = std::clamp(values[i] + static_cast<float>(event.delta.getY()) * step,
                                       0.0f, 1.0f);
                repaint();
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

    void caption(float x, float y, const char *label, float size)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(lightText ? Color(230, 229, 214) : Color(29, 45, 48));
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
        const auto size = enclosure.getSize();
        const float imageWidth = static_cast<float>(size.getWidth());
        const float imageHeight = static_cast<float>(size.getHeight());
        const float targetWidth = Height * imageWidth / imageHeight;
        drawImage(enclosure, (Width - targetWidth) * 0.5f, 0, targetWidth, Height);
    }

    void drawKnob(unsigned index)
    {
        static constexpr const char *whitecapLabels[]{"RATE", "DEPTH", "MIX", "WIDTH"};
        static constexpr const char *riptideLabels[]{"RISE", "FALL", "GLIDE", "BLEND"};
        const auto &labels = pedal == Pedal::Whitecap ? whitecapLabels : riptideLabels;
        const float x = KnobX[index], y = KnobY[index];
        caption(x, y - 59, labels[index], 15);
        drawImage(knobMaterial, x - 42, y - 42, 84, 84);
        const float angle = (-135 + values[index] * 270) * 3.14159265f / 180;
        beginPath();
        moveTo(x + std::sin(angle) * 12, y - std::cos(angle) * 12);
        lineTo(x + std::sin(angle) * 24, y - std::cos(angle) * 24);
        lineCap(ROUND);
        strokeColor(Color(20, 24, 25));
        strokeWidth(3);
        stroke();
        char readout[16];
        std::snprintf(readout, sizeof(readout), "%.0f %%", values[index] * 100);
        caption(x, y + 53, readout, 11);
    }

    void drawModeToggle()
    {
        caption(194, 448, "CHORUS", 10);
        caption(326, 448, "VIBRATO", 10);
        const auto &material = vibrato ? toggleRight : toggleLeft;
        drawImage(material, vibrato ? 235 : 224, 418, 72, 59);
    }

    const Pedal pedal;
    const bool lightText;
    NanoImage enclosure, knobMaterial, footMaterial, ledOff, ledOn, toggleLeft, toggleRight;
    std::array<float, 4> values{{0.5f, 0.5f, 0.5f, 0.5f}};
    int dragging = -1;
    double lastY = 0;
    bool enabled = true;
    bool vibrato = false;
};

FuturePedalMockup *mockup = nullptr;
} // namespace

const char *getPluginFormatName() noexcept
{
    return "Concept mockup / no audio";
}

UI *createUI()
{
    mockup = new FuturePedalMockup(selectedPedal);
    return mockup;
}

END_NAMESPACE_DISTRHO

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3 ||
        (std::strcmp(argv[1], "whitecap") != 0 && std::strcmp(argv[1], "riptide") != 0)) {
        std::fprintf(stderr, "Usage: future_pedal_mockup <whitecap|riptide> [--view|--small]\n");
        return 1;
    }
    selectedPedal = std::strcmp(argv[1], "whitecap") == 0 ? Pedal::Whitecap : Pedal::Riptide;
    const bool view = argc == 3 && std::strcmp(argv[2], "--view") == 0;
    const bool minimumSize = argc == 3 && std::strcmp(argv[2], "--small") == 0;
    if (argc == 3 && !view && !minimumSize) {
        std::fprintf(stderr, "Usage: future_pedal_mockup <whitecap|riptide> [--view|--small]\n");
        return 1;
    }

    using namespace DISTRHO;
    UIExporter editor(nullptr, 0, 48000.0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr);
    editor.setWindowTitle(selectedPedal == Pedal::Whitecap ? "Whitecap concept / no audio"
                                                           : "Riptide concept / no audio");
    if (minimumSize)
        mockup->setSize(390, 510);
    editor.setWindowVisible(true);
    editor.focus();
    if (!mockup->exerciseControls()) {
        std::fprintf(stderr, "FAIL: concept artwork or local controls\n");
        return 1;
    }
    uint width = 0, height = 0;
    bool aspect = false;
    editor.getGeometryConstraints(width, height, aspect);
    if (width != 390 || height != 510 || !aspect ||
        (minimumSize && (editor.getWidth() != 390 || editor.getHeight() != 510))) {
        std::fprintf(stderr, "FAIL: concept mockup geometry\n");
        return 1;
    }
    for (int frame = 0; view || frame < 30; ++frame) {
        if (view && !editor.isVisible())
            break;
        if (!editor.plugin_idle()) {
            if (!view)
                return 1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    editor.quit();
    std::puts("PASS: concept mockup artwork, local controls and geometry; no DSP or audio device");
    return 0;
}
