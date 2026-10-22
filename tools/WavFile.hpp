// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace stormsurge::wav
{

struct Audio {
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    // Interleaved samples in the range -1..1 for integer PCM sources. Float files
    // keep their original values, including anything above full scale.
    std::vector<float> samples;

    std::uint64_t frames() const;
};

enum class Format {
    Float32,
    Pcm16,
    Pcm24,
    Pcm32,
};

// Reads PCM 16/24/32-bit or IEEE float 32-bit RIFF/WAVE, including the extensible
// variant some DAWs write. Throws std::runtime_error with a plain description for
// malformed files, unsupported formats and non-finite float samples.
Audio read(const std::string &path);

// Writes a RIFF/WAVE file. PCM formats refuse out-of-range samples instead of
// clipping them; the audition tool writes Float32 so rendered peaks stay intact.
void write(const std::string &path, const Audio &audio, Format format);

} // namespace stormsurge::wav
