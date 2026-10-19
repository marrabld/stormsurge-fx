// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ParameterSpec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stormsurge
{

enum SquallPluginParameter : std::uint32_t {
    SquallModeParameter = 0,
    SquallBypassParameter,
    SquallPluginParameterCount
};

enum class SquallVocalMode : std::uint8_t { Clean, Heavy };

inline constexpr std::array<ParameterSpec, SquallPluginParameterCount> squallSpecs{{
    {"Mode", "mode", "", 0.0f, 0.0f, 1.0f, false},
    {"Bypass", "bypass", "", 0.0f, 0.0f, 1.0f, false},
}};

inline float sanitizeSquallParameter(std::uint32_t index, float value) noexcept
{
    const auto &spec = squallSpecs[index];
    if (!std::isfinite(value))
        return spec.initial;
    value = std::clamp(value, spec.minimum, spec.maximum);
    return value >= 0.5f ? 1.0f : 0.0f;
}

inline constexpr double SquallUiWidth = 520;
inline constexpr double SquallUiHeight = 700;
inline constexpr double SquallModeY = 365;
inline constexpr double SquallLedY = 520;
inline constexpr double SquallFootY = 590;

inline constexpr double squallModeLeft = 224;
inline constexpr double squallModeTop = 335;
inline constexpr double squallModeWidth = 72;
inline constexpr double squallModeHeight = 59;

struct SquallUiPoint {
    double x, y;
};

inline SquallUiPoint squallUiPoint(double x, double y, double width, double height) noexcept
{
    const double factor = std::min(width / SquallUiWidth, height / SquallUiHeight);
    return {(x - (width - SquallUiWidth * factor) * 0.5) / factor,
            (y - (height - SquallUiHeight * factor) * 0.5) / factor};
}

inline bool squallModeAt(SquallUiPoint point) noexcept
{
    return std::abs(point.x - SquallUiWidth * 0.5) <= 55 && std::abs(point.y - SquallModeY) <= 38;
}

inline bool squallFootAt(SquallUiPoint point) noexcept
{
    return std::hypot(point.x - SquallUiWidth * 0.5, point.y - SquallFootY) <= 45;
}

} // namespace stormsurge
