// SPDX-License-Identifier: GPL-3.0-only
#include "src/DistrhoUIInternal.hpp"
#include "ConceptMockupArtwork.hpp"
#include "OpenGL.hpp"
#include "PreviewArtwork.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::NanoImage;
using namespace stormsurge;

namespace
{
constexpr float Width = 520;
constexpr float Height = 700;
constexpr float ModeY = 365;
constexpr float LedY = 520;
constexpr float FootY = 590;

class SquallMockup final : public UI
{
  public:
    SquallMockup() : UI(static_cast<uint32_t>(Width), static_cast<uint32_t>(Height))
    {
        loadSharedResources();
        setGeometryConstraints(390, 525, true);
        enclosure = createImageFromMemory(squallEnclosurePng, sizeof(squallEnclosurePng), 0);
        footswitch = createImageFromMemory(previewFootswitchPng, sizeof(previewFootswitchPng), 0);
        ledOff = createImageFromMemory(previewLedOffPng, sizeof(previewLedOffPng), 0);
        ledOn = createImageFromMemory(previewLedOnPng, sizeof(previewLedOnPng), 0);
        toggleLeft = createImageFromMemory(previewToggleLeftPng, sizeof(previewToggleLeftPng), 0);
        toggleRight =
            createImageFromMemory(previewToggleRightPng, sizeof(previewToggleRightPng), 0);
#ifdef STORMSURGE_GUI_HARNESS
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
#endif
    }

    bool exerciseControls()
    {
        if (!enclosure.isValid() || !footswitch.isValid() || !ledOff.isValid() ||
            !ledOn.isValid() || !toggleLeft.isValid() || !toggleRight.isValid())
            return false;
        MouseEvent event;
        event.button = 1;
        event.press = true;
        event.pos = windowPoint(Width * 0.5, ModeY);
        if (!onMouse(event) || !heavy || !onMouse(event) || heavy)
            return false;
        event.pos = windowPoint(Width * 0.5, FootY);
        if (!onMouse(event) || enabled || !onMouse(event) || !enabled)
            return false;
        return true;
    }

    void resizeForTest() { setSize(390, 525); }

  protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        const float factor = drawingScale();
        translate((getWidth() - Width * factor) * 0.5f, (getHeight() - Height * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(260, 42, "STORMSURGE FX", 13);
        caption(260, 81, "SQUALL", 36);
        caption(260, 110, "VOCAL EFFECT CONCEPT", 11);
        drawModeControl();
        caption(260, 423,
                heavy ? "BLENDED GRIT / KEEP THE WORDS CLEAR" : "NATURAL VOICE / A LITTLE ROOM",
                10);
        drawImage(enabled ? ledOn : ledOff, 246, LedY - 14, 28, 28);
        drawImage(footswitch, 218, FootY - 42, 84, 84);
        caption(260, 650, enabled ? "LOCAL MOCKUP STATE: ON" : "LOCAL MOCKUP STATE: OFF", 10);
        caption(260, 678, "CONCEPT ONLY / NO AUDIO / SOUND PROVISIONAL", 9);
    }

    bool onMouse(const MouseEvent &event) override
    {
        if (event.button != 1 || !event.press)
            return false;
        const auto point = pedalPoint(event.pos);
        if (std::abs(point.getX() - Width * 0.5) <= 55 && std::abs(point.getY() - ModeY) <= 38) {
            heavy = !heavy;
            repaint();
            return true;
        }
        if (std::hypot(point.getX() - Width * 0.5, point.getY() - FootY) <= 45) {
            enabled = !enabled;
            repaint();
            return true;
        }
        return false;
    }

  private:
    float drawingScale() const { return std::min(getWidth() / Width, getHeight() / Height); }

    DGL_NAMESPACE::Point<double> pedalPoint(const DGL_NAMESPACE::Point<double> &point) const
    {
        const float factor = drawingScale();
        return {(point.getX() - (getWidth() - Width * factor) * 0.5) / factor,
                (point.getY() - (getHeight() - Height * factor) * 0.5) / factor};
    }

    DGL_NAMESPACE::Point<double> windowPoint(float x, float y) const
    {
        const float factor = drawingScale();
        return {x * factor + (getWidth() - Width * factor) * 0.5,
                y * factor + (getHeight() - Height * factor) * 0.5};
    }

    void caption(float x, float y, const char *label, float size)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(225, 229, 231));
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

    void drawModeControl()
    {
        const float y = ModeY;
        caption(188, y, "CLEAN", 11);
        caption(332, y, "HEAVY", 11);
        drawImage(heavy ? toggleRight : toggleLeft, heavy ? 235 : 224, y - 30, 72, 59);
    }

    NanoImage enclosure, footswitch, ledOff, ledOn, toggleLeft, toggleRight;
    bool enabled = true;
    bool heavy = false;
};

SquallMockup *mockup = nullptr;
} // namespace

const char *getPluginFormatName() noexcept
{
    return "Concept mockup / no audio";
}

UI *createUI()
{
    mockup = new SquallMockup;
    return mockup;
}

END_NAMESPACE_DISTRHO

int main(int argc, char **argv)
{
    const bool view = argc == 2 && std::strcmp(argv[1], "--view") == 0;
    const bool minimumSize = argc == 2 && std::strcmp(argv[1], "--small") == 0;
    const bool snapshot = argc == 2 && std::strcmp(argv[1], "--snapshot") == 0;
    if (argc != 1 && !view && !minimumSize && !snapshot) {
        std::fprintf(stderr, "Usage: squall_mockup [--view|--small|--snapshot]\n");
        return 1;
    }

    using namespace DISTRHO;
    UIExporter editor(nullptr, 0, 48000.0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr);
    editor.setWindowTitle("Squall concept mockup / no audio");
    if (minimumSize)
        mockup->resizeForTest();
    editor.setWindowVisible(true);
    editor.focus();
    if (!view && !snapshot && !mockup->exerciseControls()) {
        std::fprintf(stderr, "FAIL: Squall artwork or local controls\n");
        return 1;
    }
    uint width = 0, height = 0;
    bool aspect = false;
    editor.getGeometryConstraints(width, height, aspect);
    if (width != 390 || height != 525 || !aspect ||
        (minimumSize && (editor.getWidth() != 390 || editor.getHeight() != 525))) {
        std::fprintf(stderr, "FAIL: Squall mockup geometry\n");
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
    std::puts("PASS: Squall mockup artwork, controls and geometry; no DSP or audio device");
    return 0;
}
