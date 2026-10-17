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
#include "OpenGL.hpp"
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
        : UI(520, PreviewHeight),
          standalone(std::strcmp(getPluginFormatName(), "JACK/Standalone") == 0)
    {
        loadSharedResources();
        setGeometryConstraints(390, 720, true);
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
        const double scale = std::min(getWidth() / 520.0, getHeight() / PreviewHeight);
        const double dx = (getWidth() - 520 * scale) * 0.5;
        const double dy = (getHeight() - PreviewHeight * scale) * 0.5;
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
        foot.pos.setPos(dx + 260 * scale, dy + PreviewFootY * scale);
        if (!onMouse(foot) || values[Bypass] != 1)
            return false;
        MouseEvent mode = foot;
        mode.pos.setPos(dx + 260 * scale, dy + (PreviewModeTop + 13) * scale);
        if (!onMouse(mode) || values[PingPong] != 1)
            return false;
        MouseEvent timing = foot;
        timing.pos.setPos(dx + 370 * scale, dy + 110 * scale);
        if (!onMouse(timing) || values[Sync] != 1)
            return false;
        parameterChanged(previewHostIndex(Sync, standalone), 0);
        if (values[Sync] != 0 || !onMouse(timing) || values[Sync] != 1)
            return false;
        for (int length = 0; length < 3; ++length) {
            timing.pos.setPos(dx + (110 + length * 150) * scale, dy + 154 * scale);
            if (!onMouse(timing))
                return false;
            for (int feel = 0; feel < 3; ++feel) {
                timing.pos.setPos(dx + (110 + feel * 150) * scale, dy + 198 * scale);
                if (!onMouse(timing) || values[Division] != length * 3 + feel)
                    return false;
            }
        }
        timing.pos.setPos(dx + 110 * scale, dy + 110 * scale);
        if (!onMouse(timing) || values[Sync] != 0)
            return false;
        // Selecting an inactive note must not silently enable Sync.
        timing.pos.setPos(dx + 110 * scale, dy + 154 * scale);
        if (!onMouse(timing) || values[Sync] != 0 || values[Division] != 2)
            return false;
        ScrollEvent bpm;
        bpm.pos.setPos(dx + 390 * scale, dy + 242 * scale);
        bpm.delta.setPos(0, 1);
        const float beforeTempo = values[InternalTempo];
        if (!onScroll(bpm) || values[InternalTempo] <= beforeTempo)
            return false;
        const float coarseTempo = values[InternalTempo];
        bpm.mod = kModifierShift;
        if (!onScroll(bpm) || std::abs(values[InternalTempo] - coarseTempo - 0.1f) > 0.001f)
            return false;
        timing.pos = bpm.pos;
        if (!onMouse(timing))
            return false;
        MotionEvent tempoDrag;
        tempoDrag.pos.setPos(dx + 390 * scale, dy + 232 * scale);
        if (!onMotion(tempoDrag) || values[InternalTempo] <= coarseTempo + 0.1f)
            return false;
        timing.press = false;
        if (!onMouse(timing))
            return false;
        for (int division = 0; division < 9; ++division) {
            parameterChanged(previewHostIndex(Division, standalone), static_cast<float>(division));
            if (values[Division] != division)
                return false;
        }
        parameterChanged(previewHostIndex(Sync, standalone), 1);
        // Mock polled DSP readouts as well as exercising input gestures.
        parameterChanged(previewHostIndex(EffectiveTempo, standalone), values[InternalTempo]);
        parameterChanged(
            previewHostIndex(TempoSource, standalone),
            static_cast<float>(standalone ? TempoOrigin::Internal : TempoOrigin::Fallback));
        parameterChanged(
            previewHostIndex(EffectiveTime, standalone),
            static_cast<float>(60000.0 / values[InternalTempo] *
                               noteDivisions[static_cast<int>(values[Division])].beats));
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
#ifdef STORMSURGE_GUI_HARNESS
        // DPF's PPM capture assumes tightly packed RGB rows. At 390 pixels,
        // default four-byte alignment would skew rows and overrun its buffer.
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
#endif
        const float factor =
            std::min(getWidth() / 520.0f, getHeight() / static_cast<float>(PreviewHeight));
        translate((getWidth() - 520.0f * factor) * 0.5f,
                  (getHeight() - static_cast<float>(PreviewHeight) * factor) * 0.5f);
        scale(factor, factor);
        beginPath();
        roundedRect(8, 8, 504, PreviewHeight - 16, 28);
        fillPaint(
            linearGradient(0, 0, 520, PreviewHeight, Color(119, 127, 126), Color(45, 54, 60)));
        fill();
        beginPath();
        roundedRect(15, 15, 490, PreviewHeight - 30, 22);
        fillPaint(linearGradient(0, 0, 520, PreviewHeight, Color(25, 58, 69), Color(9, 29, 44)));
        fill();
        drawWear();
        save();
        scissor(30, 280, 460, 290);
        strokeWidth(1);
        strokeColor(Color(106, 148, 158, 0.22f));
        for (int line = 0; line < 19; ++line) {
            const float y = 290.0f + line * 15.0f;
            beginPath();
            moveTo(30, y);
            bezierTo(210, y - 120, 310, y + 110, 490, y - 20);
            stroke();
        }
        restore();
        caption(260, 34, "STORMSURGE FX", 14);
        caption(260, 66, "UNDERTOW", 33);
        char status[112];
        const bool synced = values[Sync] >= 0.5f;
        timingButton(40, 94, 215, "FREE TIME", !synced);
        timingButton(265, 94, 215, "TEMPO SYNC", synced);
        const int division = static_cast<int>(values[Division]);
        const char *lengths[]{"QUARTER  1/4", "EIGHTH  1/8", "SIXTEENTH  1/16"};
        const char *feels[]{"STRAIGHT", "DOTTED", "TRIPLET"};
        for (int column = 0; column < 3; ++column) {
            timingButton(40 + column * 150, 138, 140, lengths[column], division / 3 == column,
                         synced);
            timingButton(40 + column * 150, 182, 140, feels[column], division % 3 == column,
                         synced);
        }
        std::snprintf(status, sizeof(status), "%s %.1f BPM",
                      tempoOriginName(static_cast<int>(values[TempoSource])),
                      values[EffectiveTempo]);
        caption(163, 237, synced ? status : "FREE: USE TIME KNOB", 12);
        caption(163, 254, synced ? "ACTIVE CLOCK" : "NOTE CHOICES STORED FOR SYNC", 9);
        char tempoText[48];
        std::snprintf(tempoText, sizeof(tempoText), "%s %.1f BPM",
                      standalone ? "INTERNAL" : "FALLBACK", values[InternalTempo]);
        timingButton(300, 226, 180, tempoText, false);
        std::snprintf(status, sizeof(status), "%s / %.0f ms%s",
                      synced ? noteDivisions[division].label : "FREE TIME", values[EffectiveTime],
                      synced && values[SyncLimited] >= 0.5f ? " / RANGE LIMIT" : "");
        caption(260, 270, status, 12);
        for (const auto &knob : previewKnobs)
            drawKnob(knob);
        caption(260, 555, "DRAG KNOBS / SHIFT FOR FINE CONTROL", 10);
        caption(260, 572, "REVERB / WET DELAY ONLY / AMOUNT ZERO = DELAY ONLY", 10);
        beginPath();
        roundedRect(105, PreviewModeTop, 310, 26, 6);
        fillColor(values[PingPong] >= 0.5f ? Color(30, 90, 95) : Color(30, 52, 65));
        fill();
        caption(260, PreviewModeTop + 13,
                values[PingPong] >= 0.5f ? "MODE: PING-PONG / CLICK FOR NORMAL"
                                         : "MODE: NORMAL / CLICK FOR PING-PONG",
                10);
        const bool enabled = values[Bypass] < 0.5f;
        circleFill(260, PreviewFootY - 47, 7,
                   enabled ? Color(106, 225, 180) : Color(100, 116, 120));
        circleFill(260, PreviewFootY + 2, 36, Color(13, 24, 30));
        beginPath();
        circle(260, PreviewFootY, 31);
        fillPaint(linearGradient(232, PreviewFootY - 28, 286, PreviewFootY + 30,
                                 Color(217, 222, 215), Color(82, 99, 106)));
        fill();
        circleFill(260, PreviewFootY, 22, Color(158, 170, 170));
        caption(260, PreviewFootY + 51,
                enabled ? "EFFECT ON / CLICK TO BYPASS" : "BYPASSED / EXISTING TAILS FADE", 11);
        if (standalone) {
            beginPath();
            roundedRect(50, PreviewMonitorTop, 420, 32, 6);
            fillColor(monitor ? Color(30, 90, 75) : Color(85, 45, 35));
            fill();
            caption(260, PreviewMonitorTop + 16,
                    monitor ? "MONITOR ON / CLICK OR SPACE TO MUTE"
                            : "MONITOR MUTED / CLICK OR SPACE TO ENABLE",
                    11);
        } else {
            caption(260, PreviewMonitorTop + 16, "TEMPO FIELD: DRAG OR SCROLL / SHIFT FOR FINE",
                    10);
        }
        caption(260, PreviewHeight - 31, "0.4.0 DEVELOPMENT / START QUIETLY / GPLv3", 10);
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
            change(Sync, static_cast<float>(timingModeAt(point)));
            return true;
        }
        if (timing == Division) {
            const int length = timingColumnAt(point, 138);
            const int feel = timingColumnAt(point, 182);
            const int current = static_cast<int>(values[Division]);
            change(Division, static_cast<float>(length >= 0 ? length * 3 + current % 3
                                                            : current / 3 * 3 + feel));
            return true;
        }
        if (timing == InternalTempo) {
            dragging = InternalTempo;
            lastY = point.y;
            position = normalizedPreview(InternalTempo, values[InternalTempo]);
            editParameter(previewHostIndex(InternalTempo, standalone), true);
            return true;
        }
        if (std::hypot(point.x - 260, point.y - PreviewFootY) <= 40) {
            change(Bypass, values[Bypass] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        if (pingPongButtonAt(point)) {
            change(PingPong, values[PingPong] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        if (standalone && point.x >= 50 && point.x <= 470 && point.y >= PreviewMonitorTop &&
            point.y <= PreviewMonitorTop + 32) {
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
    void timingButton(float x, float y, float width, const char *label, bool selected,
                      bool active = true)
    {
        beginPath();
        roundedRect(x, y, width, 32, 5);
        fillColor(selected && active ? Color(50, 105, 105) : Color(21, 43, 53));
        fill();
        strokeWidth(selected ? 2 : 1);
        strokeColor(selected ? Color(159, 178, 159) : Color(72, 91, 96));
        stroke();
        caption(x + width * 0.5f, y + 16, label, 12);
    }

    void drawWear()
    {
        // Fixed marks avoid animated grain and keep paint loss away from labels.
        // All artwork is procedural; local reference images are not redistributed.
        std::uint32_t seed = 0x554e4454;
        auto random = [&seed]() {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / 16777216.0f;
        };
        for (int i = 0; i < 190; ++i) {
            const bool right = i % 2 != 0;
            const float x = right ? 493 + random() * 12 : 15 + random() * 12;
            const float y = 32 + random() * (PreviewHeight - 64);
            const float width = 1 + random() * 7;
            const float height = 2 + random() * 12;
            beginPath();
            moveTo(x, y);
            lineTo(x + width, y + height * 0.2f);
            lineTo(x + width * 0.65f, y + height);
            lineTo(x - 1, y + height * 0.7f);
            closePath();
            fillColor(i % 4 == 0 ? Color(135, 129, 111, 0.65f) : Color(7, 19, 23, 0.65f));
            fill();
        }
        for (int i = 0; i < 55; ++i) {
            const float x = 38 + random() * 440;
            const float y = i % 2 == 0 ? 18 + random() * 5 : PreviewHeight - 24 + random() * 6;
            beginPath();
            moveTo(x, y);
            lineTo(x + 2 + random() * 18, y + random() * 3);
            strokeColor(Color(172, 163, 138, 0.35f));
            strokeWidth(0.7f + random());
            stroke();
        }
        for (float x : {34.0f, 486.0f}) {
            for (float y : {34.0f, static_cast<float>(PreviewHeight - 34)}) {
                circleFill(x, y, 7, Color(8, 18, 22));
                circleFill(x, y, 5, Color(106, 110, 104));
                beginPath();
                moveTo(x - 3, y + 2);
                lineTo(x + 3, y - 2);
                strokeColor(Color(31, 39, 39));
                strokeWidth(2);
                stroke();
            }
        }
        // Footswitch scuffs sit below the control bank, not over value readouts.
        for (int i = 0; i < 65; ++i) {
            const float x = 170 + random() * 180;
            const float y = PreviewFootY - 28 + random() * 58;
            beginPath();
            moveTo(x, y);
            lineTo(x + 3 + random() * 15, y - 2 + random() * 4);
            strokeColor(Color(150, 149, 127, 0.13f));
            strokeWidth(1);
            stroke();
        }
    }

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
        const char *label = index == ReverbSpace      ? "SPACE"
                            : index == ReverbDarkness ? "DARKNESS"
                            : index == ReverbAmount   ? "AMOUNT"
                            : index == Level          ? "LEVEL"
                            : index == Time           ? "TIME"
                            : index == Feedback       ? "FEEDBACK"
                            : index == Tone           ? "TONE"
                                                      : "MIX";
        char display[32];
        if (index == ReverbSpace)
            std::snprintf(display, sizeof(display), "%.1f s", value);
        else if (index == Time)
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
void previewResizeForTest()
{
    if (testUI != nullptr)
        testUI->setSize(390, 720);
}
#endif

END_NAMESPACE_DISTRHO
