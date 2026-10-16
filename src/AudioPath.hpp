// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <cstdint>

namespace stormsurge
{

// Prepared planar stereo buffers, with either distinct or corresponding in-place
// channels. No gain, clipping, mixing, resampling or delay in this foundation.
inline void passThrough(const float *const *inputs, float *const *outputs, std::uint32_t frames,
                        bool enabled) noexcept
{
    for (std::uint32_t channel = 0; channel < 2; ++channel) {
        if (!enabled)
            std::fill_n(outputs[channel], frames, 0.0f);
        else if (inputs[channel] != outputs[channel])
            std::copy_n(inputs[channel], frames, outputs[channel]);
    }
}

} // namespace stormsurge
