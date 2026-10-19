// SPDX-License-Identifier: GPL-3.0-only
#include "SquallParameters.hpp"
#include "ConceptMockupArtwork.hpp"
#include "PreviewArtwork.hpp"
#include "DistrhoUI.hpp"
#include "DistrhoPluginUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::NanoImage;
using namespace stormsurge;

class SquallUI final : public UI
{
  public:
    SquallUI() : UI(static_cast<uint32_t>(SquallUiWidth), static_cast<uint32_t>(SquallUiHeight))
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
        for (std::uint32_t i = 0; i < SquallPluginParameterCount; ++i)
            values[i] = squallSpecs[i].initial;
#ifdef STORMSURGE_GUI_HARNESS
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
#endif
    }

#ifdef STORMSURGE_GUI_HARNESS
    bool exerciseControls()
    {
        if (!enclosure.isValid() || !footswitch.isValid() || !ledOff.isValid() ||
            !ledOn.isValid() || !toggleLeft.isValid() || !toggleRight.isValid())
            return false;
        MouseEvent event;
        event.button = 1;
        event.press = true;
        event.pos = windowPoint(SquallUiWidth * 0.5, SquallModeY);
        if (!onMouse(event) || values[SquallModeParameter] != 1.0f || !onMouse(event) ||
            values[SquallModeParameter] != 0.0f)
            return false;
        event.pos = windowPoint(SquallUiWidth * 0.5, SquallFootY);
        if (!onMouse(event) || values[SquallBypassParameter] != 1.0f || !onMouse(event) ||
            values[SquallBypassParameter] != 0.0f)
            return false;
        return true;
    }

    void resizeForTest() { setSize(390, 525); }
#endif

  protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < SquallPluginParameterCount) {
            values[index] = sanitizeSquallParameter(index, value);
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float factor = drawingScale();
        translate((getWidth() - static_cast<float>(SquallUiWidth) * factor) * 0.5f,
                  (getHeight() - static_cast<float>(SquallUiHeight) * factor) * 0.5f);
        scale(factor, factor);
        drawEnclosure();
        caption(260, 42, "STORMSURGE FX", 13);
        caption(260, 81, "SQUALL", 36);
        caption(260, 110, "VOCAL EFFECT", 11);
        drawModeControl();
        caption(260, 423,
                heavy() ? "BLENDED GRIT / CONTROLLED GROWL" : "NATURAL VOICE / SHORT ROOM", 10);
        drawImage(enabled() ? ledOn : ledOff, 246, static_cast<float>(SquallLedY) - 14, 28, 28);
        drawImage(footswitch, 218, static_cast<float>(SquallFootY) - 42, 84, 84);
        caption(260, 650, enabled() ? "EFFECT ON" : "BYPASSED", 10);
        caption(260, 678, "V0.1 SETTINGS / TUNE BY EAR", 9);
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
            squallUiPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        if (squallModeAt(point)) {
            change(SquallModeParameter, heavy() ? 0.0f : 1.0f);
            return true;
        }
        if (squallFootAt(point)) {
            change(SquallBypassParameter, enabled() ? 1.0f : 0.0f);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &) override { return false; }
    bool onScroll(const ScrollEvent &) override { return false; }

  private:
    float drawingScale() const
    {
        return std::min(getWidth() / static_cast<float>(SquallUiWidth),
                        getHeight() / static_cast<float>(SquallUiHeight));
    }

    DGL_NAMESPACE::Point<double> windowPoint(double x, double y) const
    {
        const float factor = drawingScale();
        return {x * factor + (getWidth() - SquallUiWidth * factor) * 0.5,
                y * factor + (getHeight() - SquallUiHeight * factor) * 0.5};
    }

    bool heavy() const { return values[SquallModeParameter] >= 0.5f; }
    bool enabled() const { return values[SquallBypassParameter] < 0.5f; }

    void change(std::uint32_t index, float value)
    {
        editParameter(index, true);
        values[index] = sanitizeSquallParameter(index, value);
        setParameterValue(index, values[index]);
        editParameter(index, false);
        repaint();
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
        const float targetWidth = static_cast<float>(SquallUiHeight) * imageWidth / imageHeight;
        drawImage(enclosure, (static_cast<float>(SquallUiWidth) - targetWidth) * 0.5f, 0,
                  targetWidth, static_cast<float>(SquallUiHeight));
    }

    void drawModeControl()
    {
        const float y = static_cast<float>(SquallModeY);
        caption(188, y, "CLEAN", 11);
        caption(332, y, "HEAVY", 11);
        drawImage(heavy() ? toggleRight : toggleLeft, heavy() ? 235 : 224, y - 30, 72, 59);
    }

    NanoImage enclosure, footswitch, ledOff, ledOn, toggleLeft, toggleRight;
    std::array<float, SquallPluginParameterCount> values{};
    int dragging = -1;
};

#ifdef STORMSURGE_GUI_HARNESS
static SquallUI *testUI = nullptr;

bool squallExerciseControls() noexcept
{
    return testUI != nullptr && testUI->exerciseControls();
}

void squallResizeForTest() noexcept
{
    if (testUI != nullptr)
        testUI->resizeForTest();
}
#endif

UI *createUI()
{
    auto *ui = new SquallUI;
#ifdef STORMSURGE_GUI_HARNESS
    testUI = ui;
#endif
    return ui;
}

END_NAMESPACE_DISTRHO
