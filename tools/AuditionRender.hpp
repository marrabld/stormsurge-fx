// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "WavFile.hpp"

#include <cstdint>

namespace stormsurge
{

struct AuditionSettings {
    double shiftSemitones = 0.0;
    double blend = 1.0;
};

// Runs the production Riptide core over a whole file in fixed-size blocks. Throws
// std::invalid_argument for settings or audio the core cannot accept. Output keeps
// the input length, rate and channel count.
wav::Audio renderAudition(const wav::Audio &input, const AuditionSettings &settings,
                          std::uint32_t blockFrames = 4096);

} // namespace stormsurge
