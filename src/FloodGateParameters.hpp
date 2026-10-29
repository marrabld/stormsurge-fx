// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ParameterSpec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stormsurge
{

// FloodGate's own parameter space. Deliberately unrelated to every
// other pedal's: these IDs, symbols and order are new for the UtE0 identity. Append new
// parameters after the last one; never reorder or renumber saved ones.
enum FloodGateParameter : std::uint32_t {
    FloodGateThreshold = 0,
    FloodGateRelease,
    FloodGateRange,
    FloodGateBypass,

    // Keep last
    FloodGateParameterCount
};

// Proposed starting points, not approved values. Threshold and Range are stored in dB so the host
// shows meaningful units; Release is logarithmic because 10 -> 50 ms matters more than 450 -> 500.
inline constexpr std::array<ParameterSpec, FloodGateParameterCount> floodGateSpecs{{
    {"Threshold", "threshold", "dB", -60, -80, -20, false},
    {"Release", "release", "ms", 80, 10, 500, true},
    {"Range", "range", "dB", -80, -80, -6, false},
    {"Bypass", "bypass", "", 0, 0, 1, false},
}};

inline float sanitizeFloodGate(std::uint32_t index, float value) noexcept
{
    const auto &spec = floodGateSpecs[index];
    if (!std::isfinite(value))
        return spec.initial;
    value = std::clamp(value, spec.minimum, spec.maximum);
    return index == FloodGateBypass ? (value >= 0.5f ? 1.0f : 0.0f) : value;
}

inline float normalizedFloodGate(std::uint32_t index, float value) noexcept
{
    const auto &spec = floodGateSpecs[index];
    value = sanitizeFloodGate(index, value);
    return spec.logarithmic ? std::log(value / spec.minimum) / std::log(spec.maximum / spec.minimum)
                            : (value - spec.minimum) / (spec.maximum - spec.minimum);
}

inline float denormalizedFloodGate(std::uint32_t index, float position) noexcept
{
    const auto &spec = floodGateSpecs[index];
    position = std::isfinite(position) ? std::clamp(position, 0.0f, 1.0f) : 0.0f;
    // Hoisted out of the call so line breaks do not depend on the clang-format version.
    const float value = spec.logarithmic
                            ? spec.minimum * std::pow(spec.maximum / spec.minimum, position)
                            : spec.minimum + position * (spec.maximum - spec.minimum);
    return sanitizeFloodGate(index, value);
}

// Provisional layout in Maelstrom's 520x680 frame so the shared enclosure art fits. No mockup has
// been approved for this pedal (Maelstrom's was #26), so these numbers are placeholders. Shared by
// drawing, hit testing and the GUI harness so a scaled window cannot drift from hit areas.
inline constexpr double FloodGateWidth = 520;
inline constexpr double FloodGateHeight = 680;
inline constexpr double FloodGateFootY = 520;
inline constexpr double FloodGateLedY = 447;

struct FloodGateKnobPosition {
    std::uint32_t parameter;
    float x, y;
    const char *label;
};
inline constexpr std::array<FloodGateKnobPosition, 3> floodGateKnobs{{
    {FloodGateThreshold, 155, 205, "THRESH"},
    {FloodGateRelease, 365, 205, "RELEASE"},
    {FloodGateRange, 260, 355, "RANGE"},
}};

struct FloodGatePoint {
    double x, y;
};

inline FloodGatePoint floodGatePoint(double x, double y, double width, double height) noexcept
{
    const double factor = std::min(width / FloodGateWidth, height / FloodGateHeight);
    return {(x - (width - FloodGateWidth * factor) * 0.5) / factor,
            (y - (height - FloodGateHeight * factor) * 0.5) / factor};
}

inline int floodGateKnobAt(FloodGatePoint point) noexcept
{
    for (const auto &knob : floodGateKnobs)
        if (std::hypot(point.x - knob.x, point.y - knob.y) <= 40)
            return static_cast<int>(knob.parameter);
    return -1;
}

inline bool floodGateFootAt(FloodGatePoint point) noexcept
{
    return std::hypot(point.x - FloodGateWidth * 0.5, point.y - FloodGateFootY) <= 40;
}

} // namespace stormsurge
