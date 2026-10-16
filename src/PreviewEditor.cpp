// SPDX-License-Identifier: GPL-3.0-only
#include "PreviewParameters.hpp"
#include "TempoSync.hpp"
#include "DistrhoUI.hpp"
#include "DistrhoPluginUtils.hpp"

#include <cstdio>
#include <cstring>
#include <numbers>
#ifdef STORMSURGE_GUI_HARNESS
#include <cstdlib>
#endif

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using namespace stormsurge;

#ifdef STORMSURGE_GUI_HARNESS
class PreviewUI;
static PreviewUI *testUI = nullptr;
#endif

class PreviewUI final : public UI
{
  public:
    PreviewUI()
        : UI(520, 680), standalone(std::strcmp(getPluginFormatName(), "JACK/Standalone") == 0)
    {
        loadSharedResources();
        setGeometryConstraints(390, 510, true);
        if (standalone)
            getWindow().setIgnoringKeyRepeat(true);
        for (std::uint32_t i = 0; i < PreviewParameterCount; ++i)
            values[i] = previewSpecs[i].initial;
#ifdef STORMSURGE_GUI_HARNESS
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
#endif
    }

#ifdef STORMSURGE_GUI_HARNESS
    bool exerciseControls()
    {
        // Dispatch real widget events without opening an audio device.
        const double scale = std::min(getWidth() / 520.0, getHeight() / 680.0);
        const double dx = (getWidth() - 520 * scale) * 0.5;
        const double dy = (getHeight() - 680 * scale) * 0.5;
        for (const auto &knob : previewKnobs) {
            const float before = values[knob.parameter];
            MouseEvent mouse;
            mouse.button = 1;
            mouse.press = true;
            mouse.pos.setPos(dx + knob.x * scale, dy + knob.y * scale);
            if (!onMouse(mouse))
                return false;
            MotionEvent motion;
            motion.pos.setPos(dx + knob.x * scale, dy + (knob.y - 15) * scale);
            if (!onMotion(motion) || values[knob.parameter] <= before)
                return false;
            mouse.press = false;
            if (!onMouse(mouse))
                return false;
            ScrollEvent scroll;
            scroll.pos = mouse.pos;
            scroll.delta.setPos(0, -1);
            if (!onScroll(scroll))
                return false;
        }
        MouseEvent foot;
        foot.button = 1;
        foot.press = true;
        foot.pos.setPos(dx + 260 * scale, dy + 540 * scale);
        if (!onMouse(foot) || values[Bypass] != 1)
            return false;
        MouseEvent mode = foot;
        mode.pos.setPos(dx + 260 * scale, dy + 467 * scale);
        if (!onMouse(mode) || values[PingPong] != 1)
            return false;
        MouseEvent timing = foot;
        timing.pos.setPos(dx + 110 * scale, dy + 142 * scale);
        if (!onMouse(timing) || values[Sync] != 1)
            return false;
        parameterChanged(previewHostIndex(Sync, standalone), 0);
        if (values[Sync] != 0 || !onMouse(timing) || values[Sync] != 1)
            return false;
        timing.pos.setPos(dx + 260 * scale, dy + 142 * scale);
        if (!onMouse(timing) || values[Division] != 1)
            return false;
        ScrollEvent bpm;
        bpm.pos.setPos(dx + 410 * scale, dy + 142 * scale);
        bpm.delta.setPos(0, 1);
        const float beforeTempo = values[InternalTempo];
        if (!onScroll(bpm) || values[InternalTempo] <= beforeTempo)
            return false;
        // Mock polled DSP readouts as well as exercising input gestures.
        parameterChanged(previewHostIndex(EffectiveTempo, standalone), values[InternalTempo]);
        parameterChanged(
            previewHostIndex(TempoSource, standalone),
            static_cast<float>(standalone ? TempoOrigin::Internal : TempoOrigin::Fallback));
        parameterChanged(previewHostIndex(EffectiveTime, standalone),
                         60000.0f / values[InternalTempo] * 1.5f);
        parameterChanged(previewHostIndex(SyncLimited, standalone), 1);
        if (values[EffectiveTempo] != values[InternalTempo] || values[SyncLimited] != 1)
            return false;
        parameterChanged(previewHostIndex(SyncLimited, standalone), 0);
        parameterChanged(previewHostIndex(PingPong, standalone), 0);
        if (values[PingPong] != 0)
            return false;
        if (!onMouse(mode) || values[PingPong] != 1)
            return false;
        KeyboardEvent key;
        key.key = ' ';
        key.press = true;
        if (onKeyboard(key) != standalone)
            return false;
        return true;
    }
#endif

  protected:
    void parameterChanged(uint32_t index, float value) override
    {
        const int effect = previewLogicalIndex(index, standalone);
        if (effect >= 0)
            values[effect] = sanitizePreview(effect, value);
        else if (standalone && index == MonitorActive)
            monitor = std::isfinite(value) && value >= 0.5f;
        repaint();
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
        save();
        scissor(30, 120, 460, 330);
        strokeWidth(1);
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
        char status[112];
        std::snprintf(status, sizeof(status), "%s / %s %.1f BPM / %.0f ms",
                      values[Sync] >= 0.5f ? (values[SyncLimited] >= 0.5f ? "SYNC LIMIT" : "SYNC")
                                           : "FREE MS",
                      tempoOriginName(static_cast<int>(values[TempoSource])),
                      values[EffectiveTempo], values[EffectiveTime]);
        caption(260, 117, status, 10);
        for (float x : {40.0f, 190.0f, 340.0f}) {
            beginPath();
            roundedRect(x, 130, 140, 23, 5);
            fillColor(Color(30, 65, 80));
            fill();
        }
        caption(110, 142, values[Sync] >= 0.5f ? "TIME: SYNC" : "TIME: MS", 10);
        char note[32], tempoText[32];
        const auto division = static_cast<std::size_t>(values[Division]);
        std::snprintf(note, sizeof(note), "NOTE: %s", noteDivisions[division].label);
        std::snprintf(tempoText, sizeof(tempoText), "%s %.1f BPM", standalone ? "INT" : "FB",
                      values[InternalTempo]);
        caption(260, 142, note, 10);
        caption(410, 142, tempoText, 10);
        for (const auto &knob : previewKnobs)
            drawKnob(knob);
        caption(260, 443, "DRAG KNOBS / SHIFT FOR FINE CONTROL", 11);
        beginPath();
        roundedRect(105, 454, 310, 26, 6);
        fillColor(values[PingPong] >= 0.5f ? Color(30, 90, 95) : Color(30, 52, 65));
        fill();
        caption(260, 467,
                values[PingPong] >= 0.5f ? "MODE: PING-PONG / CLICK FOR NORMAL"
                                         : "MODE: NORMAL / CLICK FOR PING-PONG",
                10);
        const bool enabled = values[Bypass] < 0.5f;
        circleFill(260, 493, 7, enabled ? Color(106, 225, 180) : Color(100, 116, 120));
        circleFill(260, 542, 36, Color(13, 24, 30));
        beginPath();
        circle(260, 540, 31);
        fillPaint(linearGradient(232, 512, 286, 570, Color(217, 222, 215), Color(82, 99, 106)));
        fill();
        circleFill(260, 540, 22, Color(158, 170, 170));
        caption(260, 589, enabled ? "DELAY ON / CLICK TO BYPASS" : "BYPASSED / EXISTING TAILS FADE",
                11);
        if (standalone) {
            beginPath();
            roundedRect(50, 605, 420, 32, 6);
            fillColor(monitor ? Color(30, 90, 75) : Color(85, 45, 35));
            fill();
            caption(260, 621,
                    monitor ? "MONITOR ON / CLICK OR SPACE TO MUTE"
                            : "MONITOR MUTED / CLICK OR SPACE TO ENABLE",
                    11);
        } else {
            caption(260, 621, "MS OR SYNC / D = DOTTED / T = TRIPLET", 10);
        }
        caption(260, 649, "0.3.0 DEVELOPMENT / START QUIETLY / GPLv3", 10);
    }

    bool onMouse(const MouseEvent &event) override
    {
        if (event.button != 1)
            return false;
        if (!event.press) {
            if (dragging < 0)
                return false;
            editParameter(previewHostIndex(static_cast<uint32_t>(dragging), standalone), false);
            dragging = -1;
            return true;
        }
        const auto point = pedalPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        if (dragging >= 0) {
            editParameter(previewHostIndex(static_cast<uint32_t>(dragging), standalone), false);
            dragging = -1;
        }
        const int knob = knobAt(point);
        if (knob >= 0) {
            dragging = knob;
            lastY = point.y;
            position = normalizedPreview(knob, values[knob]);
            editParameter(previewHostIndex(knob, standalone), true);
            return true;
        }
        const int timing = timingControlAt(point);
        if (timing == Sync) {
            change(Sync, values[Sync] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        if (timing == Division) {
            const int step = (event.mod & kModifierShift) ? 8 : 1;
            change(Division, static_cast<float>((static_cast<int>(values[Division]) + step) % 9));
            return true;
        }
        if (timing == InternalTempo) {
            dragging = InternalTempo;
            lastY = point.y;
            position = normalizedPreview(InternalTempo, values[InternalTempo]);
            editParameter(previewHostIndex(InternalTempo, standalone), true);
            return true;
        }
        if (std::hypot(point.x - 260, point.y - 540) <= 40) {
            change(Bypass, values[Bypass] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        if (pingPongButtonAt(point)) {
            change(PingPong, values[PingPong] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        if (standalone && point.x >= 50 && point.x <= 470 && point.y >= 605 && point.y <= 637) {
            requestMonitor();
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &event) override
    {
        if (dragging < 0)
            return false;
        const auto point = pedalPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        const double sensitivity = (event.mod & kModifierShift) ? 0.001 : 0.006;
        position = std::clamp(position + (lastY - point.y) * sensitivity, 0.0, 1.0);
        lastY = point.y;
        send(static_cast<uint32_t>(dragging),
             denormalizedPreview(dragging, static_cast<float>(position)));
        return true;
    }

    bool onScroll(const ScrollEvent &event) override
    {
        if (dragging >= 0)
            return false;
        const auto point = pedalPoint(event.pos.getX(), event.pos.getY(), getWidth(), getHeight());
        if (timingControlAt(point) == InternalTempo) {
            const float step = (event.mod & kModifierShift) ? 0.1f : 1.0f;
            change(InternalTempo,
                   values[InternalTempo] + static_cast<float>(event.delta.getY()) * step);
            return true;
        }
        const int knob = knobAt(point);
        if (knob < 0)
            return false;
        const float step = (event.mod & kModifierShift) ? 0.002f : 0.02f;
        change(knob, denormalizedPreview(knob, normalizedPreview(knob, values[knob]) +
                                                   static_cast<float>(event.delta.getY()) * step));
        return true;
    }

    bool onKeyboard(const KeyboardEvent &event) override
    {
        // Leave the DAW's Space transport shortcut alone in plugin mode.
        if (!standalone || event.key != ' ' || !event.press)
            return false;
        requestMonitor();
        return true;
    }

  private:
    void send(uint32_t index, float value)
    {
        values[index] = sanitizePreview(index, value);
        setParameterValue(previewHostIndex(index, standalone), values[index]);
        repaint();
    }

    void change(uint32_t index, float value)
    {
        editParameter(previewHostIndex(index, standalone), true);
        send(index, value);
        editParameter(previewHostIndex(index, standalone), false);
    }

    void requestMonitor()
    {
        editParameter(MonitorRequest, true);
        setParameterValue(MonitorRequest, monitor ? 0.0f : 1.0f);
        editParameter(MonitorRequest, false);
        // Only the polled DSP status updates the indicator; a request is not consent
        // surviving a restart. The effect footswitch never changes monitoring.
    }

    void caption(float x, float y, const char *label, float size)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(222, 221, 199));
        text(x, y, label, nullptr);
    }

    void circleFill(float x, float y, float radius, Color color)
    {
        beginPath();
        circle(x, y, radius);
        fillColor(color);
        fill();
    }

    void drawKnob(const KnobPosition &knob)
    {
        const auto index = knob.parameter;
        const float value = values[index];
        const char *label = index == Level      ? "LEVEL"
                            : index == Time     ? "TIME"
                            : index == Feedback ? "FEEDBACK"
                            : index == Tone     ? "TONE"
                                                : "MIX";
        char display[32];
        if (index == Time)
            std::snprintf(display, sizeof(display),
                          values[Sync] >= 0.5f ? "%.0f ms free" : "%.0f ms", value);
        else if (index == Tone)
            std::snprintf(display, sizeof(display), "%.0f Hz", value);
        else if (index == Level) {
            if (value == 0)
                std::snprintf(display, sizeof(display), "Muted");
            else
                std::snprintf(display, sizeof(display), "%+.1f dB", 20.0 * std::log10(value));
        } else
            std::snprintf(display, sizeof(display), "%.0f %%", value);
        caption(knob.x, knob.y - 59, label, 16);
        circleFill(knob.x + 3, knob.y + 5, 37, Color(5, 17, 23, 0.7f));
        circleFill(knob.x, knob.y, 34, Color(110, 129, 130));
        circleFill(knob.x, knob.y, 29, Color(169, 181, 182));
        const double angle =
            (-135.0 + normalizedPreview(index, value) * 270.0) * std::numbers::pi / 180.0;
        beginPath();
        moveTo(knob.x + static_cast<float>(std::sin(angle) * 12),
               knob.y - static_cast<float>(std::cos(angle) * 12));
        lineTo(knob.x + static_cast<float>(std::sin(angle) * 24),
               knob.y - static_cast<float>(std::cos(angle) * 24));
        strokeColor(Color(26, 37, 41));
        strokeWidth(3);
        stroke();
        caption(knob.x, knob.y + 53, display, 12);
    }

    const bool standalone;
    std::array<float, PreviewParameterCount> values{};
    bool monitor = false;
    int dragging = -1;
    double lastY = 0;
    double position = 0;
};

UI *createUI()
{
    auto *ui = new PreviewUI;
#ifdef STORMSURGE_GUI_HARNESS
    testUI = ui;
#endif
    return ui;
}

#ifdef STORMSURGE_GUI_HARNESS
bool previewExerciseControls()
{
    return testUI != nullptr && testUI->exerciseControls();
}
#endif

END_NAMESPACE_DISTRHO
