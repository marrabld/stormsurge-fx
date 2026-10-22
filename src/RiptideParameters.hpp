// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ParameterSpec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stormsurge
{

enum RiptidePluginParameter : std::uint32_t {
    RiptideShiftParameter = 0,
    RiptideBlendParameter,
    RiptideBypassParameter,
    RiptidePluginParameterCount
};

inline constexpr std::array<ParameterSpec, RiptidePluginParameterCount> riptideSpecs{{
    {"Shift", "shift", "st", 0.0f, -12.0f, 0.0f, false},
    {"Blend", "blend", "%", 100.0f, 0.0f, 100.0f, false},
    {"Bypass", "bypass", "", 0.0f, 0.0f, 1.0f, false},
}};

// Shift is quantised to whole semitones so a host automation curve and the readout
// agree; a fractional setting would read as an unintended detune. Blend stays
// continuous. An invalid value falls back to that parameter's own safe default
// rather than to whichever parameter happened to initialise first.
inline float sanitizeRiptideParameter(std::uint32_t index, float value) noexcept
{
    if (index >= RiptidePluginParameterCount)
        return 0.0f;
    const auto &spec = riptideSpecs[index];
    if (!std::isfinite(value))
        return spec.initial;
    value = std::clamp(value, spec.minimum, spec.maximum);
    if (index == RiptideShiftParameter)
        return std::round(value);
    if (index == RiptideBypassParameter)
        return value >= 0.5f ? 1.0f : 0.0f;
    return value;
}

inline float normalizedRiptide(std::uint32_t index, float value) noexcept
{
    const auto &spec = riptideSpecs[index];
    value = sanitizeRiptideParameter(index, value);
    return (value - spec.minimum) / (spec.maximum - spec.minimum);
}

inline float denormalizedRiptide(std::uint32_t index, float position) noexcept
{
    const auto &spec = riptideSpecs[index];
    position = std::isfinite(position) ? std::clamp(position, 0.0f, 1.0f) : 0.0f;
    return sanitizeRiptideParameter(index, spec.minimum + position * (spec.maximum - spec.minimum));
}

// The prepared Riptide enclosure is 1040x1416, so 500x680 follows its aspect.
inline constexpr double RiptideUiWidth = 500;
inline constexpr double RiptideUiHeight = 680;
inline constexpr double RiptideKnobY = 320;
inline constexpr double RiptideLedY = 468;
inline constexpr double RiptideFootY = 552;
inline constexpr double RiptideKnobRadius = 44;
inline constexpr double riptideShiftX = 155;
inline constexpr double riptideBlendX = 345;

struct RiptideUiPoint {
    double x, y;
};

inline RiptideUiPoint riptideUiPoint(double x, double y, double width, double height) noexcept
{
    const double factor = std::min(width / RiptideUiWidth, height / RiptideUiHeight);
    return {(x - (width - RiptideUiWidth * factor) * 0.5) / factor,
            (y - (height - RiptideUiHeight * factor) * 0.5) / factor};
}

inline bool riptideKnobAt(RiptideUiPoint point, std::uint32_t index) noexcept
{
    if (index >= RiptidePluginParameterCount)
        return false;
    const double x = index == RiptideShiftParameter   ? riptideShiftX
                     : index == RiptideBlendParameter ? riptideBlendX
                                                      : -1.0;
    return x >= 0.0 && std::hypot(point.x - x, point.y - RiptideKnobY) <= RiptideKnobRadius;
}

inline bool riptideFootAt(RiptideUiPoint point) noexcept
{
    return std::hypot(point.x - RiptideUiWidth * 0.5, point.y - RiptideFootY) <= 45;
}

} // namespace stormsurge
