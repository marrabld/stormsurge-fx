// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace stormsurge
{

struct NoteDivision {
    const char *label;
    double beats;
};
inline constexpr std::array<NoteDivision, 9> noteDivisions{{
    {"1/4", 1},
    {"1/4 D", 1.5},
    {"1/4 T", 2.0 / 3},
    {"1/8", 0.5},
    {"1/8 D", 0.75},
    {"1/8 T", 1.0 / 3},
    {"1/16", 0.25},
    {"1/16 D", 0.375},
    {"1/16 T", 1.0 / 6},
}};

enum class TempoOrigin { Fallback = 0, Host = 1, Held = 2, Internal = 3 };
inline const char *tempoOriginName(int origin) noexcept
{
    switch (static_cast<TempoOrigin>(origin)) {
    case TempoOrigin::Host:
        return "HOST";
    case TempoOrigin::Held:
        return "HELD";
    case TempoOrigin::Internal:
        return "INTERNAL";
    default:
        return "FALLBACK";
    }
}

struct TempoTiming {
    double bpm = 120;
    double timeMs = 400;
    TempoOrigin origin = TempoOrigin::Fallback;
    bool limited = false;
};

// Audio-owner state. Transport playing/stopped is deliberately not an input.
class TempoSync
{
  public:
    TempoTiming resolve(bool standalone, bool valid, double hostBpm, double internalBpm, bool sync,
                        double freeMs, int division) noexcept
    {
        TempoTiming result;
        internalBpm = std::isfinite(internalBpm) ? std::clamp(internalBpm, 20.0, 300.0) : 120;
        if (standalone) {
            result.bpm = internalBpm;
            result.origin = TempoOrigin::Internal;
        } else if (valid && std::isfinite(hostBpm) && hostBpm >= 1 && hostBpm <= 1000) {
            result.bpm = lastHost = hostBpm;
            result.origin = TempoOrigin::Host;
        } else if (lastHost > 0) {
            result.bpm = lastHost;
            result.origin = TempoOrigin::Held;
        } else {
            result.bpm = internalBpm;
            result.origin = TempoOrigin::Fallback;
        }
        if (sync) {
            const auto index = static_cast<std::size_t>(std::clamp(division, 0, 8));
            const double requested = 60000.0 / result.bpm * noteDivisions[index].beats;
            result.timeMs = std::clamp(requested, 20.0, 8000.0);
            result.limited = result.timeMs != requested;
        } else {
            result.timeMs = std::isfinite(freeMs) ? std::clamp(freeMs, 20.0, 2000.0) : 400;
        }
        return result;
    }

  private:
    double lastHost = 0;
};

} // namespace stormsurge
