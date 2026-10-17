// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ParameterSpec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stormsurge
{

// Maelstrom's own parameter space. Deliberately unrelated to Undertow's: these
// IDs, symbols and order are new for the UtM0 identity and must never be
// renumbered to match the preview plugin.
enum MaelstromParameter : std::uint32_t {
    Drive = 0,
    Tight,
    Bite,
    MaelstromLevel,
    MaelstromBypass,
    MaelstromGrit,
    MaelstromParameterCount
};

// +12 dB is the approved output ceiling. The exact float matters: it is the same
// literal the DSP clamps to, so a host automation lane that writes the maximum
// cannot be silently rejected.
inline constexpr float MaelstromMaxLevel = 3.9810717055349722f;

inline constexpr std::array<ParameterSpec, MaelstromParameterCount> maelstromSpecs{{
    {"Drive", "drive", "%", 10, 0, 100, false},
    {"Tight", "tight", "%", 50, 0, 100, false},
    {"Bite", "bite", "%", 50, 0, 100, false},
    {"Level", "level", "", 1, 0, MaelstromMaxLevel, false},
    {"Bypass", "bypass", "", 0, 0, 1, false},
    {"Grit", "grit", "", 0, 0, 1, false},
}};

inline float sanitizeMaelstrom(std::uint32_t index, float value) noexcept
{
    const auto &spec = maelstromSpecs[index];
    if (!std::isfinite(value))
        return spec.initial;
    value = std::clamp(value, spec.minimum, spec.maximum);
    return (index == MaelstromBypass || index == MaelstromGrit) ? (value >= 0.5f ? 1.0f : 0.0f)
                                                                : value;
}

inline constexpr double MaelstromModeY = 280;

inline float normalizedMaelstrom(std::uint32_t index, float value) noexcept
{
    const auto &spec = maelstromSpecs[index];
    value = sanitizeMaelstrom(index, value);
    return spec.logarithmic ? std::log(value / spec.minimum) / std::log(spec.maximum / spec.minimum)
                            : (value - spec.minimum) / (spec.maximum - spec.minimum);
}

inline float denormalizedMaelstrom(std::uint32_t index, float position) noexcept
{
    const auto &spec = maelstromSpecs[index];
    position = std::isfinite(position) ? std::clamp(position, 0.0f, 1.0f) : 0.0f;
    return sanitizeMaelstrom(
        index, spec.logarithmic ? spec.minimum * std::pow(spec.maximum / spec.minimum, position)
                                : spec.minimum + position * (spec.maximum - spec.minimum));
}

// Layout matches the mockup approved in issue #26. Shared by drawing, hit
// testing and the GUI harness so a scaled window cannot drift from hit areas.
inline constexpr double MaelstromWidth = 520;
inline constexpr double MaelstromHeight = 680;
inline constexpr double MaelstromFootY = 520;
inline constexpr double MaelstromLedY = 447;

struct MaelstromKnobPosition {
    std::uint32_t parameter;
    float x, y;
    const char *label;
};
inline constexpr std::array<MaelstromKnobPosition, 4> maelstromKnobs{{
    {Drive, 155, 205, "DRIVE"},
    {Tight, 365, 205, "TIGHT"},
    {Bite, 155, 355, "BITE"},
    {MaelstromLevel, 365, 355, "LEVEL"},
}};

struct MaelstromPoint {
    double x, y;
};

inline bool maelstromModeAt(MaelstromPoint point) noexcept
{
    return std::abs(point.x - MaelstromWidth * 0.5) <= 75 &&
           std::abs(point.y - MaelstromModeY) <= 24;
}

inline MaelstromPoint maelstromPoint(double x, double y, double width, double height) noexcept
{
    const double factor = std::min(width / MaelstromWidth, height / MaelstromHeight);
    return {(x - (width - MaelstromWidth * factor) * 0.5) / factor,
            (y - (height - MaelstromHeight * factor) * 0.5) / factor};
}

inline int maelstromKnobAt(MaelstromPoint point) noexcept
{
    for (const auto &knob : maelstromKnobs)
        if (std::hypot(point.x - knob.x, point.y - knob.y) <= 40)
            return static_cast<int>(knob.parameter);
    return -1;
}

inline bool maelstromFootAt(MaelstromPoint point) noexcept
{
    return std::hypot(point.x - MaelstromWidth * 0.5, point.y - MaelstromFootY) <= 40;
}

} // namespace stormsurge
