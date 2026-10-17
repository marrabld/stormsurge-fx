// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "PreviewTempoBridge.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace stormsurge
{

struct PreviewAuClock {
    bool valid = false;
    double beat = 0;
    double bpm = 0;
};

// Audio Unit HostCallbackInfo is supplied by the adapter. Keeping this templated
// lets headless tests check missing/failed callbacks without Apple frameworks.
template <class Callbacks>
PreviewAuClock supplyPreviewAuTempo(void *instance, const Callbacks &callbacks) noexcept
{
    PreviewAuClock clock;
    if (callbacks.beatAndTempoProc != nullptr &&
        callbacks.beatAndTempoProc(callbacks.hostUserData, &clock.beat, &clock.bpm) == 0)
        clock.valid = true;
    else
        clock = {};

    // Tempo remains usable without a meaningful song position. DPF casts the
    // beat to int32_t for BBT, so do not pass nonfinite/out-of-range beats there.
    if (!std::isfinite(clock.beat) || clock.beat < std::numeric_limits<std::int32_t>::min() ||
        clock.beat >= std::numeric_limits<std::int32_t>::max())
        clock.beat = 0;
    setPreviewHostTempo(instance, clock.valid, clock.bpm);
    return clock;
}

} // namespace stormsurge
