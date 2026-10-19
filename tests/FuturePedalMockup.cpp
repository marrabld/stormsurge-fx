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
constexpr float Width = 520, RiptideWidth = 500, Height = 680;
constexpr std::array<float, 4> KnobX{155, 365, 155, 365};
constexpr std::array<float, 4> WhitecapKnobY{205, 205, 355, 355};
enum class Pedal { Whitecap, Riptide };
Pedal selectedPedal = Pedal::Whitecap;

float widthFor(Pedal pedal)
{
    return pedal == Pedal::Riptide ? RiptideWidth : Width;
}

class FuturePedalMockup final : public UI
{
  public:
    explicit FuturePedalMockup(Pedal type)
        : UI(static_cast<uint32_t>(widthFor(type)), 680), pedal(type),
          lightText(type == Pedal::Riptide)
    {
        loadSharedResources();
        setGeometryConstraints(390, pedal == Pedal::Riptide ? 530 : 510, true);
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
        event.pos = windowPoint(KnobX[0], knobY(0));
        if (!onMouse(event))
            return false;
        MotionEvent motion;
        motion.pos = windowPoint(KnobX[0], knobY(0) - 20);
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
        translate((getWidth() - canvasWidth() * factor) * 0.5f,
                  (getHeight() - Height * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(controlX(260), 39, "STORMSURGE FX", 13);
        caption(controlX(260), 77, pedal == Pedal::Whitecap ? "WHITECAP" : "RIPTIDE", 34);
        caption(controlX(260), 105,
                pedal == Pedal::Whitecap ? "CHORUS / VIBRATO CONCEPT" : "PITCH-SHIFT CONCEPT", 10);
        if (pedal == Pedal::Riptide)
            drawPitchReadout();
        for (unsigned i = 0; i < values.size(); ++i)
            drawKnob(i);
        if (pedal == Pedal::Whitecap)
            drawModeToggle();
        drawImage(enabled ? ledOn : ledOff, controlX(245), 485, 30, 30);
        drawImage(footMaterial, controlX(218), 518, 84, 84);
        caption(controlX(260), 615, enabled ? "LOCAL VISUAL STATE: ON" : "LOCAL VISUAL STATE: OFF",
                10);
        caption(controlX(260), 642, "CONCEPT MOCKUP / NO AUDIO / CONTROLS PROVISIONAL", 9);
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
            if (distanceSquared(point, controlX(KnobX[i]), knobY(i)) <= 42 * 42) {
                dragging = static_cast<int>(i);
                lastY = point.getY();
                return true;
            }
        }
        if (distanceSquared(point, controlX(260), 560) <= 42 * 42) {
            enabled = !enabled;
            repaint();
            return true;
        }
        if (pedal == Pedal::Whitecap && point.getX() >= controlX(224) &&
            point.getX() <= controlX(296) && point.getY() >= 418 && point.getY() <= 477) {
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
            if (distanceSquared(point, controlX(KnobX[i]), knobY(i)) <= 42 * 42) {
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
    float canvasWidth() const { return widthFor(pedal); }

    float controlX(float x) const { return x + (canvasWidth() - Width) * 0.5f; }

    float knobY(unsigned index) const
    {
        if (pedal == Pedal::Riptide)
            return index < 2 ? 270.0f : 410.0f;
        return WhitecapKnobY[index];
    }

    float drawingScale() const
    {
        return std::min(getWidth() / canvasWidth(), getHeight() / Height);
    }

    Point<double> pedalPoint(const Point<double> &point) const
    {
        const float factor = drawingScale();
        return {(point.getX() - (getWidth() - canvasWidth() * factor) * 0.5) / factor,
                (point.getY() - (getHeight() - Height * factor) * 0.5) / factor};
    }

    Point<double> windowPoint(float x, float y) const
    {
        const float factor = drawingScale();
        return {controlX(x) * factor + (getWidth() - canvasWidth() * factor) * 0.5,
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
        const float targetWidth = std::min(Height * imageWidth / imageHeight, canvasWidth());
        const float targetHeight = targetWidth * imageHeight / imageWidth;
        drawImage(enclosure, (canvasWidth() - targetWidth) * 0.5f, (Height - targetHeight) * 0.5f,
                  targetWidth, targetHeight);
    }

    void drawKnob(unsigned index)
    {
        static constexpr const char *whitecapLabels[]{"RATE", "DEPTH", "MIX", "WIDTH"};
        static constexpr const char *riptideLabels[]{"RISE", "FALL", "GLIDE", "BLEND"};
        const auto &labels = pedal == Pedal::Whitecap ? whitecapLabels : riptideLabels;
        const float x = controlX(KnobX[index]), y = knobY(index);
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
        caption(controlX(194), 448, "CHORUS", 10);
        caption(controlX(326), 448, "VIBRATO", 10);
        const auto &material = vibrato ? toggleRight : toggleLeft;
        drawImage(material, controlX(vibrato ? 235 : 224), 418, 72, 59);
    }

    void drawPitchReadout()
    {
        const float center = controlX(260);
        caption(center, 119, "SHIFTED OUTPUT / STATIC EXAMPLE", 8);

        for (int tick = -20; tick <= 20; ++tick) {
            const float normalized = static_cast<float>(tick) / 20.0f;
            const float x = center + normalized * 102.0f;
            const float y = 124.0f + normalized * normalized * 20.0f;
            const bool major = tick % 5 == 0;
            const float length = major ? 8.0f : 4.0f;
            beginPath();
            moveTo(x, y);
            lineTo(x, y + length);
            strokeColor(tick == 0 ? Color(235, 72, 62) : Color(210, 216, 211));
            strokeWidth(tick == 0 ? 3.0f : (major ? 2.0f : 1.0f));
            stroke();
        }
        caption(center - 105, 157, "-50", 8);
        caption(center + 105, 157, "+50", 8);
        caption(center, 161, "CENTS", 7);

        caption(center - 55, 186, "IN E", 10);
        caption(center - 19, 186, "→", 13);
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(25);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(95, 205, 255));
        text(center + 23, 183, "B", nullptr);
        caption(center + 23, 201, "OUT", 6);
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
    const uint32_t minimumWidth = 390;
    const uint32_t minimumHeight = selectedPedal == Pedal::Riptide ? 530 : 510;
    if (minimumSize)
        mockup->setSize(minimumWidth, minimumHeight);
    editor.setWindowVisible(true);
    editor.focus();
    if (!mockup->exerciseControls()) {
        std::fprintf(stderr, "FAIL: concept artwork or local controls\n");
        return 1;
    }
    uint width = 0, height = 0;
    bool aspect = false;
    editor.getGeometryConstraints(width, height, aspect);
    if (width != minimumWidth || height != minimumHeight || !aspect ||
        (minimumSize &&
         (editor.getWidth() != minimumWidth || editor.getHeight() != minimumHeight))) {
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
