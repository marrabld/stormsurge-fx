// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace stormsurge
{

struct SquallParameters {
    bool heavy = false;
    bool bypass = false;
};

// Host-independent vocal effect. prepare() allocates off the audio callback.
// One audio-thread owner; prepare(), reset(), setParameters() and process() must
// not run concurrently. Process accepts planar mono or stereo buffers.
class Squall
{
  public:
    void prepare(double sampleRate, const SquallParameters &parameters = {});
    void reset() noexcept;
    void setParameters(const SquallParameters &parameters) noexcept;
    // Mono or stereo, separate or corresponding in-place buffers. Buffers must be
    // valid for the requested channel count. Invalid samples are treated as silence.
    void process(const float *const *inputs, float *const *outputs, std::uint32_t frames,
                 std::uint32_t channels) noexcept;

  private:
    struct DelayLine {
        std::vector<double> data;
        std::size_t position = 0;
        void prepare(std::size_t frames);
        void reset() noexcept;
        double read() const noexcept;
        void write(double value) noexcept;
    };

    struct Smooth {
        double value = 0, target = 0, step = 0;
        std::uint32_t remaining = 0;
        void snap(double next) noexcept;
        void set(double next, std::uint32_t frames) noexcept;
        double tick() noexcept;
    };

    static SquallParameters sanitize(const SquallParameters &parameters) noexcept;
    double heavyVoice(std::size_t channel, double input) noexcept;
    std::array<double, 2> reverberate(const std::array<double, 2> &input) noexcept;

    static constexpr std::size_t CombCount = 4;
    static constexpr std::size_t DiffuserCount = 2;
    std::array<std::array<DelayLine, CombCount>, 2> combs;
    std::array<std::array<DelayLine, DiffuserCount>, 2> diffusers;
    std::array<std::array<double, CombCount>, 2> combDamped{};
    std::array<std::array<double, 3>, 2> voiceFilters{};
    std::array<std::array<double, CombCount>, 2> combFeedback{};
    SquallParameters parameters;
    Smooth modeBlend, bypassBlend;
    double rate = 48000.0;
    double dampingPole = 0.0;
    double highpassPole = 0.0;
    double lowMidLowPole = 0.0;
    double lowMidHighPole = 0.0;
    double reverbMix = 0.12;
    std::uint32_t rampFrames = 960;
};

} // namespace stormsurge
