// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stormsurge
{

// Logical effect IDs. Host indices differ for new controls in standalone because
// its original monitor request/status already occupy slots 6/7. Use the helpers.
// New parameters append at the end, never before the published readouts.
enum PreviewParameter : std::uint32_t {
    Time = 0,
    Feedback,
    Mix,
    Tone,
    Level,
    Bypass,
    PingPong,
    Sync,
    Division,
    InternalTempo,
    EffectiveTempo,
    TempoSource,
    EffectiveTime,
    SyncLimited,
    ReverbSpace,
    ReverbDarkness,
    ReverbAmount,
    PreviewParameterCount
};

// Raw standalone host slots, not logical effect IDs or previewSpecs indices.
inline constexpr std::uint32_t MonitorRequest = 6;
inline constexpr std::uint32_t MonitorActive = 7;
inline constexpr std::uint32_t StandaloneParameterCount = PreviewParameterCount + 2;

inline constexpr std::uint32_t previewHostIndex(std::uint32_t effect, bool standalone) noexcept
{
    return standalone && effect >= PingPong ? effect + 2 : effect;
}

inline constexpr int previewLogicalIndex(std::uint32_t host, bool standalone) noexcept
{
    if (standalone && (host == MonitorRequest || host == MonitorActive))
        return -1;
    const auto effect = standalone && host > MonitorActive ? host - 2 : host;
    return effect < PreviewParameterCount ? static_cast<int>(effect) : -1;
}

struct ParameterSpec {
    const char *name;
    const char *symbol;
    const char *unit;
    float initial, minimum, maximum;
    bool logarithmic;
    bool readOnly = false;
};

inline constexpr std::array<ParameterSpec, PreviewParameterCount> previewSpecs{{
    {"Time", "time_ms", "ms", 400, 20, 2000, true},
    {"Feedback", "feedback", "%", 35, 0, 90, false},
    {"Mix", "mix", "%", 20, 0, 100, false},
    {"Tone", "tone_hz", "Hz", 2500, 400, 16000, true},
    {"Output gain", "output_gain", "", 1, 0, 1.995262315f, false},
    {"Bypass", "bypass", "", 0, 0, 1, false},
    {"Ping-pong", "ping_pong", "", 0, 0, 1, false},
    {"Tempo sync", "tempo_sync", "", 0, 0, 1, false},
    {"Note division", "note_division", "", 0, 0, 8, false},
    {"Internal/fallback tempo", "internal_tempo", "BPM", 120, 20, 300, false},
    {"Effective tempo", "effective_tempo", "BPM", 120, 1, 1000, false, true},
    {"Tempo source", "tempo_source", "", 0, 0, 3, false, true},
    {"Delay target", "delay_target_ms", "ms", 400, 20, 8000, false, true},
    {"Sync time limited", "sync_limited", "", 0, 0, 1, false, true},
    {"Reverb Space", "reverb_space", "s", 6, 1, 12, false},
    {"Reverb Darkness", "reverb_darkness", "%", 65, 0, 100, false},
    {"Reverb Amount", "reverb_amount", "%", 0, 0, 100, false},
}};

inline int previewEffectIndex(std::uint32_t host, bool standalone) noexcept
{
    const int logical = previewLogicalIndex(host, standalone);
    return logical >= 0 && !previewSpecs[logical].readOnly ? logical : -1;
}

inline float sanitizePreview(std::uint32_t index, float value) noexcept
{
    const auto &spec = previewSpecs[index];
    if (!std::isfinite(value))
        return spec.initial;
    value = std::clamp(value, spec.minimum, spec.maximum);
    if (index == Bypass || index == PingPong || index == Sync || index == SyncLimited)
        return value >= 0.5f ? 1.0f : 0.0f;
    return index == Division || index == TempoSource ? std::round(value) : value;
}

inline float normalizedPreview(std::uint32_t index, float value) noexcept
{
    const auto &spec = previewSpecs[index];
    value = sanitizePreview(index, value);
    return spec.logarithmic ? std::log(value / spec.minimum) / std::log(spec.maximum / spec.minimum)
                            : (value - spec.minimum) / (spec.maximum - spec.minimum);
}

inline float denormalizedPreview(std::uint32_t index, float position) noexcept
{
    const auto &spec = previewSpecs[index];
    position = std::isfinite(position) ? std::clamp(position, 0.0f, 1.0f) : 0.0f;
    return sanitizePreview(
        index, spec.logarithmic ? spec.minimum * std::pow(spec.maximum / spec.minimum, position)
                                : spec.minimum + position * (spec.maximum - spec.minimum));
}

struct KnobPosition {
    std::uint32_t parameter;
    float x, y;
};
inline constexpr double PreviewHeight = 900;
inline constexpr double PreviewFootY = 756;
inline constexpr double PreviewModeTop = 670;
inline constexpr double PreviewMonitorTop = 823;
inline constexpr std::array<KnobPosition, 8> previewKnobs{{
    {Time, 155, 280},
    {Feedback, 365, 280},
    {Tone, 105, 425},
    {Mix, 260, 425},
    {Level, 415, 425},
    {ReverbSpace, 105, 590},
    {ReverbDarkness, 260, 590},
    {ReverbAmount, 415, 590},
}};

// Coordinates shared by drawing, hit testing and display-independent UI tests.
struct PedalPoint {
    double x, y;
};

inline PedalPoint pedalPoint(double x, double y, double width, double height) noexcept
{
    const double factor = std::min(width / 520.0, height / PreviewHeight);
    return {(x - (width - 520 * factor) * 0.5) / factor,
            (y - (height - PreviewHeight * factor) * 0.5) / factor};
}

inline int knobAt(PedalPoint point) noexcept
{
    for (const auto &knob : previewKnobs)
        if (std::hypot(point.x - knob.x, point.y - knob.y) <= 40)
            return static_cast<int>(knob.parameter);
    return -1;
}

inline bool pingPongButtonAt(PedalPoint point) noexcept
{
    return point.x >= 105 && point.x <= 415 && point.y >= PreviewModeTop &&
           point.y <= PreviewModeTop + 26;
}

// Popup targets are shared with tests so a scaled menu cannot edit a knob beneath it.
inline int timingSelectorAt(PedalPoint point) noexcept
{
    if (point.y >= 138 && point.y <= 170) {
        if (point.x >= 40 && point.x <= 250)
            return 0;
        if (point.x >= 270 && point.x <= 480)
            return 1;
    }
    if (point.x >= 40 && point.x <= 480 && point.y >= 178 && point.y <= 210)
        return 2;
    return -1;
}

inline bool timingSyncAt(PedalPoint point) noexcept
{
    return point.x >= 170 && point.x <= 350 && point.y >= 94 && point.y <= 126;
}

inline int timingMenuRowAt(PedalPoint point, int menu) noexcept
{
    const double left = menu == 0 ? 40 : 270;
    if ((menu == 0 || menu == 1) && point.x >= left && point.x <= left + 210 && point.y >= 174 &&
        point.y < 270)
        return static_cast<int>((point.y - 174) / 32);
    return -1;
}

inline bool timingTempoAt(PedalPoint point) noexcept
{
    return point.x >= 150 && point.x <= 370 && point.y >= 230 && point.y <= 262;
}

} // namespace stormsurge
