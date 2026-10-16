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
enum PreviewParameter : std::uint32_t {
    Time = 0,
    Feedback,
    Mix,
    Tone,
    Level,
    Bypass,
    PingPong,
    EffectParameterCount
};

// Raw standalone host slots, not logical effect IDs or previewSpecs indices.
inline constexpr std::uint32_t MonitorRequest = 6;
inline constexpr std::uint32_t MonitorActive = 7;
inline constexpr std::uint32_t StandaloneParameterCount = EffectParameterCount + 2;

inline constexpr std::uint32_t previewHostIndex(std::uint32_t effect, bool standalone) noexcept
{
    return standalone && effect >= PingPong ? effect + 2 : effect;
}

inline constexpr int previewEffectIndex(std::uint32_t host, bool standalone) noexcept
{
    if (standalone && (host == MonitorRequest || host == MonitorActive))
        return -1;
    const auto effect = standalone && host > MonitorActive ? host - 2 : host;
    return effect < EffectParameterCount ? static_cast<int>(effect) : -1;
}

struct ParameterSpec {
    const char *name;
    const char *symbol;
    const char *unit;
    float initial, minimum, maximum;
    bool logarithmic;
};

inline constexpr std::array<ParameterSpec, EffectParameterCount> previewSpecs{{
    {"Time", "time_ms", "ms", 400, 20, 2000, true},
    {"Feedback", "feedback", "%", 35, 0, 90, false},
    {"Mix", "mix", "%", 20, 0, 100, false},
    {"Tone", "tone_hz", "Hz", 2500, 400, 16000, true},
    {"Output gain", "output_gain", "", 1, 0, 1.995262315f, false},
    {"Bypass", "bypass", "", 0, 0, 1, false},
    {"Ping-pong", "ping_pong", "", 0, 0, 1, false},
}};

inline float sanitizePreview(std::uint32_t index, float value) noexcept
{
    const auto &spec = previewSpecs[index];
    if (!std::isfinite(value))
        return spec.initial;
    value = std::clamp(value, spec.minimum, spec.maximum);
    return index == Bypass || index == PingPong ? (value >= 0.5f ? 1.0f : 0.0f) : value;
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
inline constexpr std::array<KnobPosition, 5> previewKnobs{{
    {Time, 155, 220},
    {Feedback, 365, 220},
    {Tone, 105, 365},
    {Mix, 260, 365},
    {Level, 415, 365},
}};

// Coordinates shared by drawing, hit testing and display-independent UI tests.
struct PedalPoint {
    double x, y;
};

inline PedalPoint pedalPoint(double x, double y, double width, double height) noexcept
{
    const double factor = std::min(width / 520.0, height / 680.0);
    return {(x - (width - 520 * factor) * 0.5) / factor,
            (y - (height - 680 * factor) * 0.5) / factor};
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
    return point.x >= 105 && point.x <= 415 && point.y >= 454 && point.y <= 480;
}

} // namespace stormsurge
