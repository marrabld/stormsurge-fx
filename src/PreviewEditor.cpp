// SPDX-License-Identifier: GPL-3.0-only
#include "PreviewParameters.hpp"
#include "PreviewArtwork.hpp"
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
        setGeometryConstraints(390, 675, true);
        enclosure = createImageFromMemory(previewEnclosurePng, sizeof(previewEnclosurePng), 0);
        knobMaterial = createImageFromMemory(previewKnobPng, sizeof(previewKnobPng), 0);
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
        if (!enclosure.isValid() || !knobMaterial.isValid())
            return false;
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
        timing.pos.setPos(dx + 260 * scale, dy + 110 * scale);
        if (!onMouse(timing) || values[Sync] != 1)
            return false;
        parameterChanged(previewHostIndex(Sync, standalone), 0);
        if (values[Sync] != 0 || !onMouse(timing) || values[Sync] != 1)
            return false;
        for (int length = 0; length < 3; ++length) {
            timing.pos.setPos(dx + 145 * scale, dy + 154 * scale);
            if (!onMouse(timing) || popup != 0)
                return false;
            timing.pos.setPos(dx + 145 * scale, dy + (190 + length * 32) * scale);
            if (!onMouse(timing) || popup != -1)
                return false;
            for (int feel = 0; feel < 3; ++feel) {
                timing.pos.setPos(dx + 375 * scale, dy + 154 * scale);
                if (!onMouse(timing) || popup != 1)
                    return false;
                timing.pos.setPos(dx + 375 * scale, dy + (190 + feel * 32) * scale);
                if (!onMouse(timing) || values[Division] != length * 3 + feel || popup != -1)
                    return false;
            }
        }
        timing.pos.setPos(dx + 260 * scale, dy + 110 * scale);
        if (!onMouse(timing) || values[Sync] != 0)
            return false;
        // Selecting an inactive note must not silently enable Sync.
        timing.pos.setPos(dx + 145 * scale, dy + 154 * scale);
        if (!onMouse(timing))
            return false;
        parameterChanged(previewHostIndex(Division, standalone), 7);
        timing.pos.setPos(dx + 145 * scale, dy + 190 * scale);
        if (!onMouse(timing) || values[Sync] != 0 || values[Division] != 1)
            return false;
        timing.pos.setPos(dx + 260 * scale, dy + 194 * scale);
        if (!onMouse(timing) || popup != 2)
            return false;
        ScrollEvent bpm;
        bpm.pos.setPos(dx + 260 * scale, dy + 246 * scale);
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
        tempoDrag.pos.setPos(dx + 260 * scale, dy + 236 * scale);
        if (!onMotion(tempoDrag) || values[InternalTempo] <= coarseTempo + 0.1f)
            return false;
        timing.press = false;
        if (!onMouse(timing))
            return false;
        KeyboardEvent escape;
        escape.key = kKeyEscape;
        escape.press = true;
        if (!onKeyboard(escape) || popup != -1)
            return false;
        timing.press = true;
        timing.pos.setPos(dx + 145 * scale, dy + 154 * scale);
        if (!onMouse(timing))
            return false;
        ScrollEvent blockedScroll;
        blockedScroll.pos.setPos(dx + 155 * scale, dy + 280 * scale);
        blockedScroll.delta.setPos(0, 1);
        const float priorTime = values[Time];
        if (!onScroll(blockedScroll) || values[Time] != priorTime)
            return false;
        // Dismissing a popup over another control must not also edit that control.
        const float priorBypass = values[Bypass];
        if (!onMouse(foot) || popup != -1 || values[Bypass] != priorBypass)
            return false;
        if (standalone) {
            if (!onMouse(timing))
                return false;
            MouseEvent monitorClick = foot;
            monitorClick.pos.setPos(dx + 260 * scale, dy + (PreviewMonitorTop + 16) * scale);
            if (!onMouse(monitorClick) || popup != -1)
                return false;
        }
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
        if (const char *menu = std::getenv("STORMSURGE_CAPTURE_POPUP"))
            popup = std::strcmp(menu, "note") == 0 ? 0 : std::strcmp(menu, "feel") == 0 ? 1 : 2;
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
        drawEnclosure();
        save();
        scissor(30, 220, 460, 290);
        strokeWidth(1);
        strokeColor(Color(106, 148, 158, 0.22f));
        for (int line = 0; line < 19; ++line) {
            const float y = 230.0f + line * 15.0f;
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
        caption(222, 110, "SYNC", 13);
        beginPath();
        roundedRect(256, 99, 54, 22, 11);
        fillColor(synced ? Color(50, 105, 105) : Color(21, 43, 53));
        fill();
        circleFill(synced ? 298 : 268, 110, 8, Color(205, 209, 189));
        caption(335, 110, synced ? "ON" : "OFF", 11);
        const int division = static_cast<int>(values[Division]);
        const char *lengths[]{"NOTE: QUARTER  v", "NOTE: EIGHTH  v", "NOTE: SIXTEENTH  v"};
        const char *feels[]{"FEEL: STRAIGHT  v", "FEEL: DOTTED  v", "FEEL: TRIPLET  v"};
        timingButton(40, 138, 210, lengths[division / 3], popup == 0);
        timingButton(270, 138, 210, feels[division % 3], popup == 1);
        if (synced)
            std::snprintf(status, sizeof(status), "%s %.1f BPM / %.0f ms%s / ...",
                          tempoOriginName(static_cast<int>(values[TempoSource])),
                          values[EffectiveTempo], values[EffectiveTime],
                          values[SyncLimited] >= 0.5f ? " / LIMIT" : "");
        else
            std::snprintf(status, sizeof(status), "FREE TIME / %.0f ms / TEMPO ...", values[Time]);
        caption(260, 194, status, 12);
        for (const auto &knob : previewKnobs)
            drawKnob(knob);
        caption(260, 512, "REVERB / WET DELAY ONLY", 11);
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
            caption(260, PreviewMonitorTop + 16, "SHIFT + DRAG FOR FINE CONTROL", 10);
        }
        caption(260, PreviewHeight - 31, "0.4.0 DEVELOPMENT / START QUIETLY / GPLv3", 10);
        drawTimingPopup();
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
        if (popup >= 0) {
            if (popup == 2 && timingTempoAt(point)) {
                dragging = InternalTempo;
                lastY = point.y;
                position = normalizedPreview(InternalTempo, values[InternalTempo]);
                editParameter(previewHostIndex(InternalTempo, standalone), true);
                return true;
            }
            const int row = timingMenuRowAt(point, popup);
            if (row >= 0) {
                const int current = static_cast<int>(values[Division]);
                change(Division, static_cast<float>(popup == 0 ? row * 3 + current % 3
                                                               : current / 3 * 3 + row));
            }
            popup = -1;
            repaint();
            return true;
        }
        if (timingSyncAt(point)) {
            change(Sync, values[Sync] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        const int selector = timingSelectorAt(point);
        if (selector >= 0) {
            popup = selector;
            repaint();
            return true;
        }
        const int knob = knobAt(point);
        if (knob >= 0) {
            dragging = knob;
            lastY = point.y;
            position = normalizedPreview(knob, values[knob]);
            editParameter(previewHostIndex(knob, standalone), true);
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
        if (popup == 2 && timingTempoAt(point)) {
            const float step = (event.mod & kModifierShift) ? 0.1f : 1.0f;
            change(InternalTempo,
                   values[InternalTempo] + static_cast<float>(event.delta.getY()) * step);
            return true;
        }
        if (popup >= 0)
            return true;
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
        if (popup >= 0 && event.press && event.key == kKeyEscape) {
            popup = -1;
            repaint();
            return true;
        }
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

    void drawEnclosure()
    {
        if (!enclosure.isValid())
            return;
        const auto size = enclosure.getSize();
        const float width = static_cast<float>(size.getWidth());
        const float height = static_cast<float>(size.getHeight());
        // Nine slices preserve corner/rim thickness instead of stretching the
        // whole enclosure to the pedal's tall aspect ratio.
        const float sourceX[]{0, 64, width - 64, width};
        const float sourceY[]{0, 64, height - 64, height};
        const float destX[]{8, 40, 480, 512};
        const float destY[]{8, 40, static_cast<float>(PreviewHeight - 40),
                            static_cast<float>(PreviewHeight - 8)};
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

    void drawTimingPopup()
    {
        if (popup < 0)
            return;
        const float left = popup == 0 ? 40 : popup == 1 ? 270 : 100;
        const float top = popup == 2 ? 214 : 174;
        const float width = popup == 2 ? 320 : 210;
        beginPath();
        roundedRect(left - 3, top - 3, width + 6, 102, 5);
        fillColor(Color(13, 28, 37));
        fill();
        strokeColor(Color(142, 157, 147));
        strokeWidth(1);
        stroke();
        if (popup == 2) {
            char tempoText[48];
            std::snprintf(tempoText, sizeof(tempoText), "%s %.1f BPM",
                          standalone ? "INTERNAL" : "FALLBACK", values[InternalTempo]);
            timingButton(150, 230, 220, tempoText, false);
            caption(260, 279, "DRAG OR SCROLL / SHIFT FOR FINE", 11);
            caption(260, 298, standalone ? "STANDALONE CLOCK" : "DOES NOT OVERRIDE HOST / HELD",
                    10);
            return;
        }
        const char *lengths[]{"QUARTER  1/4", "EIGHTH  1/8", "SIXTEENTH  1/16"};
        const char *feels[]{"STRAIGHT", "DOTTED", "TRIPLET"};
        const int division = static_cast<int>(values[Division]);
        for (int row = 0; row < 3; ++row)
            timingButton(left, top + row * 32, width, popup == 0 ? lengths[row] : feels[row],
                         row == (popup == 0 ? division / 3 : division % 3));
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
        if (knobMaterial.isValid()) {
            beginPath();
            rect(knob.x - 42, knob.y - 42, 84, 84);
            fillPaint(imagePattern(knob.x - 42, knob.y - 42, 84, 84, 0, knobMaterial, 1));
            fill();
        } else {
            circleFill(knob.x, knob.y, 34, Color(110, 129, 130));
            circleFill(knob.x, knob.y, 29, Color(169, 181, 182));
        }
        const double angle =
            (-135.0 + normalizedPreview(index, value) * 270.0) * std::numbers::pi / 180.0;
        save();
        lineCap(ROUND);
        beginPath();
        moveTo(knob.x + static_cast<float>(std::sin(angle) * 12) + 0.6f,
               knob.y - static_cast<float>(std::cos(angle) * 12) + 0.7f);
        lineTo(knob.x + static_cast<float>(std::sin(angle) * 24) + 0.6f,
               knob.y - static_cast<float>(std::cos(angle) * 24) + 0.7f);
        strokeColor(Color(241, 239, 222, 0.6f));
        strokeWidth(4);
        stroke();
        beginPath();
        moveTo(knob.x + static_cast<float>(std::sin(angle) * 12),
               knob.y - static_cast<float>(std::cos(angle) * 12));
        lineTo(knob.x + static_cast<float>(std::sin(angle) * 24),
               knob.y - static_cast<float>(std::cos(angle) * 24));
        strokeColor(Color(20, 24, 25));
        strokeWidth(3);
        stroke();
        restore();
        caption(knob.x, knob.y + 53, display, 12);
    }

    const bool standalone;
    DGL_NAMESPACE::NanoImage enclosure;
    DGL_NAMESPACE::NanoImage knobMaterial;
    std::array<float, PreviewParameterCount> values{};
    bool monitor = false;
    int dragging = -1;
    int popup = -1;
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
        testUI->setSize(390, 675);
}
#endif

END_NAMESPACE_DISTRHO
