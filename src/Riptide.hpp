// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace stormsurge
{

struct RiptideParameters {
    double shiftSemitones = 0.0;
    double blend = 1.0;
};

// Host-independent downward pitch-shifting prototype. prepare() allocates off the
// audio callback. One audio-thread owner; lifecycle and processing calls must not
// run concurrently.
class Riptide
{
  public:
    void prepare(double sampleRate, std::uint32_t channels = 2,
                 const RiptideParameters &parameters = {});
    void reset() noexcept;
    void setParameters(const RiptideParameters &parameters) noexcept;

    // Planar mono or stereo, separate or corresponding in-place buffers. Invalid
    // samples become silence. No allocation occurs here.
    void process(const float *const *inputs, float *const *outputs, std::uint32_t frames) noexcept;

    std::uint32_t minimumReadDelaySamples() const noexcept { return minimumDelaySamples; }
    std::uint32_t maximumReadDelaySamples() const noexcept { return maximumDelaySamples; }
    std::uint32_t channelCount() const noexcept { return channels; }

  private:
    struct Smoother {
        double value = 0.0;
        double target = 0.0;
        double step = 0.0;
        std::uint32_t remaining = 0;
        void snap(double next) noexcept;
        void set(double next, std::uint32_t frames) noexcept;
        double tick() noexcept;
    };

    RiptideParameters sanitize(const RiptideParameters &parameters) const noexcept;
    double read(std::uint32_t channel, double delaySamples) const noexcept;

    std::array<std::vector<double>, 2> buffers;
    RiptideParameters parameters;
    Smoother pitchRatio, blend;
    std::uint32_t channels = 2;
    std::uint32_t minimumDelaySamples = 0;
    std::uint32_t grainSpanSamples = 0;
    std::uint32_t maximumDelaySamples = 0;
    std::uint32_t smoothingFrames = 1;
    std::size_t write = 0;
    double sampleRate = 48000.0;
    double phase = 0.0;
};

} // namespace stormsurge
