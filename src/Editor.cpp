// SPDX-License-Identifier: GPL-3.0-only
#include "DistrhoUI.hpp"
#include "DistrhoPluginUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#ifdef STORMSURGE_GUI_HARNESS
#include <cstdlib>
#endif

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;

class FoundationUI final : public UI
{
  public:
    FoundationUI()
        : UI(520, 680), standalone(std::strcmp(getPluginFormatName(), "JACK/Standalone") == 0)
    {
        loadSharedResources();
        setGeometryConstraints(390, 510, true);
#ifdef STORMSURGE_GUI_HARNESS
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
#endif
    }

  protected:
    void parameterChanged(uint32_t index, float value) override
    {
        // The read-only status is polled by DPF, so device restarts that reset
        // the monitor gate cannot leave an inaccurate "ON" indicator behind.
        if (standalone && index == 1) {
            monitor = value >= 0.5f;
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float factor = std::min(getWidth() / 520.0f, getHeight() / 680.0f);
        translate((getWidth() - 520.0f * factor) * 0.5f, (getHeight() - 680.0f * factor) * 0.5f);
        scale(factor, factor);

        beginPath();
        roundedRect(8, 8, 504, 664, 28);
        fillPaint(linearGradient(0, 0, 520, 680, Color(142, 151, 153), Color(45, 54, 60)));
        fill();
        beginPath();
        roundedRect(15, 15, 490, 650, 22);
        fillPaint(linearGradient(0, 0, 520, 680, Color(25, 64, 82), Color(9, 29, 44)));
        fill();

        // Original procedural decoration. Generated source images are not embedded.
        save();
        scissor(30, 120, 460, 330);
        strokeWidth(1.0f);
        strokeColor(Color(106, 148, 158, 0.22f));
        for (int line = 0; line < 19; ++line) {
            const float y = 155.0f + line * 15.0f;
            beginPath();
            moveTo(30, y);
            bezierTo(210, y - 120, 310, y + 110, 490, y - 20);
            stroke();
        }
        restore();

        caption(260, 48, "STORMSURGE FX", 17);
        caption(260, 90, "UNDERTOW", 39);
        caption(260, 117, "FOUNDATION / NOT AN EFFECT YET", 11);

        knob(155, 220, "TIME", "800 ms");
        knob(365, 220, "FEEDBACK", "Preview");
        knob(105, 365, "TONE", "Preview");
        knob(260, 365, "MIX", "Preview");
        knob(415, 365, "LEVEL", "Preview");
        caption(260, 443, "KNOBS ARE VISUAL PLACEHOLDERS", 11);

        circleFill(260, 485, 7,
                   standalone && monitor ? Color(106, 225, 180) : Color(100, 116, 120));
        circleFill(260, 542, 36, Color(13, 24, 30));
        beginPath();
        circle(260, 540, 31);
        fillPaint(linearGradient(232, 512, 286, 570, Color(217, 222, 215), Color(82, 99, 106)));
        fill();
        circleFill(260, 540, 22, Color(158, 170, 170));
        caption(260, 596,
                standalone
                    ? (monitor ? "MONITOR ON / SPACE TO MUTE" : "MUTED / CLICK OR SPACE TO MONITOR")
                    : "STEREO PASS-THROUGH / AUDIO UNCHANGED",
                11);
        caption(260, 622,
                standalone ? "JACK ROUTING REQUIRED / START QUIETLY"
                           : "NO DELAY, GAIN OR FILTERING",
                11);
        caption(260, 646, "0.0.1 DEVELOPMENT / GPLv3", 10);
    }

    bool onMouse(const MouseEvent &event) override
    {
        if (!standalone || event.button != 1 || !event.press)
            return false;
        const float factor = std::min(getWidth() / 520.0f, getHeight() / 680.0f);
        const double x = (event.pos.getX() - (getWidth() - 520 * factor) * 0.5) / factor;
        const double y = (event.pos.getY() - (getHeight() - 680 * factor) * 0.5) / factor;
        if ((x - 260) * (x - 260) + (y - 540) * (y - 540) > 40 * 40)
            return false;
        toggleMonitor();
        return true;
    }

    bool onKeyboard(const KeyboardEvent &event) override
    {
        if (!standalone || event.key != ' ' || !event.press)
            return false;
        toggleMonitor();
        return true;
    }

  private:
    void toggleMonitor()
    {
        monitor = !monitor;
        editParameter(0, true);
        setParameterValue(0, monitor ? 1.0f : 0.0f);
        editParameter(0, false);
        repaint();
    }

    void caption(float x, float y, const char *label, float size)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(222, 221, 199));
        text(x, y, label, nullptr);
    }

    void circleFill(float x, float y, float radius, Color colour)
    {
        beginPath();
        circle(x, y, radius);
        fillColor(colour);
        fill();
    }

    void knob(float x, float y, const char *label, const char *value)
    {
        caption(x, y - 59, label, 16);
        circleFill(x + 3, y + 5, 37, Color(5, 17, 23, 0.7f));
        beginPath();
        circle(x, y, 34);
        fillPaint(linearGradient(x - 25, y - 30, x + 30, y + 30, Color(221, 223, 216),
                                 Color(100, 118, 124)));
        fill();
        circleFill(x, y, 29, Color(169, 181, 182));
        beginPath();
        moveTo(x, y - 24);
        lineTo(x, y - 10);
        strokeColor(Color(26, 37, 41));
        strokeWidth(3);
        stroke();
        caption(x, y + 53, value, 12);
    }

    const bool standalone;
    bool monitor = false;
};

UI *createUI()
{
    return new FoundationUI;
}

END_NAMESPACE_DISTRHO
