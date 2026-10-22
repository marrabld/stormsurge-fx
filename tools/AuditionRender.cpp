// SPDX-License-Identifier: GPL-3.0-only
#include "AuditionRender.hpp"

#include "Riptide.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace stormsurge
{

namespace
{

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::invalid_argument(message);
}

} // namespace

wav::Audio renderAudition(const wav::Audio &input, const AuditionSettings &settings,
                          std::uint32_t blockFrames)
{
    require(input.channels == 1 || input.channels == 2,
            "Riptide audition supports mono or stereo WAV files only");
    require(std::isfinite(settings.shiftSemitones) && settings.shiftSemitones >= -12.0 &&
                settings.shiftSemitones <= 0.0,
            "Shift must be between -12 and 0 semitones");
    require(std::isfinite(settings.blend) && settings.blend >= 0.0 && settings.blend <= 1.0,
            "Blend must be between 0 and 1");
    require(blockFrames > 0, "Block size must be greater than zero");

    const std::size_t frames = static_cast<std::size_t>(input.frames());
    std::vector<float> planar[2];
    for (std::uint16_t channel = 0; channel < input.channels; ++channel)
        planar[channel].resize(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < input.channels; ++channel)
            planar[channel][frame] = input.samples[frame * input.channels + channel];
    }

    Riptide effect;
    effect.prepare(input.sampleRate, input.channels, {settings.shiftSemitones, settings.blend});

    wav::Audio output = input;
    output.samples.assign(input.samples.size(), 0.0f);
    for (std::size_t offset = 0; offset < frames; offset += blockFrames) {
        const auto count =
            static_cast<std::uint32_t>(std::min<std::size_t>(blockFrames, frames - offset));
        const float *inputs[] = {planar[0].data() + offset, planar[1].data() + offset};
        float *outputs[] = {planar[0].data() + offset, planar[1].data() + offset};
        effect.process(inputs, outputs, count);
        for (std::uint32_t frame = 0; frame < count; ++frame) {
            for (std::uint16_t channel = 0; channel < input.channels; ++channel) {
                output.samples[(offset + frame) * input.channels + channel] =
                    planar[channel][offset + frame];
            }
        }
    }
    return output;
}

} // namespace stormsurge
