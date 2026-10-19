// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ParameterSpec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stormsurge
{

enum WhitecapParameter : std::uint32_t {
    WhitecapRate = 0,
    WhitecapDepth,
    WhitecapMix,
    WhitecapWidth,
    WhitecapModeParameter,
    WhitecapBypass,
    WhitecapParameterCount
};

inline constexpr std::array<ParameterSpec, WhitecapParameterCount> whitecapSpecs{{
    {"Rate", "rate_hz", "Hz", 0.5f, 0.1f, 5.0f, true},
    {"Depth", "depth_ms", "ms", 0.5f, 0.0f, 2.0f, false},
    {"Mix", "mix", "%", 20.0f, 0.0f, 100.0f, false},
    {"Width", "width_deg", "deg", 20.0f, 0.0f, 90.0f, false},
    {"Mode", "mode", "", 0.0f, 0.0f, 1.0f, false},
    {"Bypass", "bypass", "", 0.0f, 0.0f, 1.0f, false},
}};

inline float sanitizeWhitecap(std::uint32_t index, float value) noexcept
{
    const auto &spec = whitecapSpecs[index];
    if (!std::isfinite(value))
        return spec.initial;
    value = std::clamp(value, spec.minimum, spec.maximum);
    return (index == WhitecapModeParameter || index == WhitecapBypass)
               ? (value >= 0.5f ? 1.0f : 0.0f)
               : value;
}

inline float normalizedWhitecap(std::uint32_t index, float value) noexcept
{
    const auto &spec = whitecapSpecs[index];
    value = sanitizeWhitecap(index, value);
    return spec.logarithmic ? std::log(value / spec.minimum) / std::log(spec.maximum / spec.minimum)
                            : (value - spec.minimum) / (spec.maximum - spec.minimum);
}

inline float denormalizedWhitecap(std::uint32_t index, float position) noexcept
{
    const auto &spec = whitecapSpecs[index];
    position = std::isfinite(position) ? std::clamp(position, 0.0f, 1.0f) : 0.0f;
    return sanitizeWhitecap(
        index, spec.logarithmic ? spec.minimum * std::pow(spec.maximum / spec.minimum, position)
                                : spec.minimum + position * (spec.maximum - spec.minimum));
}

inline constexpr double WhitecapUiWidth = 520;
inline constexpr double WhitecapHeight = 700;
inline constexpr double WhitecapModeY = 522;
inline constexpr double WhitecapFootY = 625;
inline constexpr double WhitecapLedY = 568;

struct WhitecapKnobPosition {
    std::uint32_t parameter;
    float x, y;
    const char *label;
};
inline constexpr std::array<WhitecapKnobPosition, 4> whitecapKnobs{{
    {WhitecapRate, 155, 235, "RATE"},
    {WhitecapDepth, 365, 235, "DEPTH"},
    {WhitecapMix, 155, 395, "MIX"},
    {WhitecapWidth, 365, 395, "WIDTH"},
}};

struct WhitecapPoint {
    double x, y;
};

inline WhitecapPoint whitecapPoint(double x, double y, double width, double height) noexcept
{
    const double factor = std::min(width / WhitecapUiWidth, height / WhitecapHeight);
    return {(x - (width - WhitecapUiWidth * factor) * 0.5) / factor,
            (y - (height - WhitecapHeight * factor) * 0.5) / factor};
}

inline int whitecapKnobAt(WhitecapPoint point) noexcept
{
    for (const auto &knob : whitecapKnobs)
        if (std::hypot(point.x - knob.x, point.y - knob.y) <= 40)
            return static_cast<int>(knob.parameter);
    return -1;
}

inline bool whitecapModeAt(WhitecapPoint point) noexcept
{
    return std::abs(point.x - WhitecapUiWidth * 0.5) <= 75 &&
           std::abs(point.y - WhitecapModeY) <= 30;
}

inline bool whitecapFootAt(WhitecapPoint point) noexcept
{
    return std::hypot(point.x - WhitecapUiWidth * 0.5, point.y - WhitecapFootY) <= 40;
}

} // namespace stormsurge
