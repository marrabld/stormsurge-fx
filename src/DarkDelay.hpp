// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace stormsurge
{

struct DelayParameters {
    double timeMs = 400.0;
    double feedback = 0.35;
    double mix = 0.20;
    double toneHz = 2500.0;
    // Linear amplitude: zero is silence, one is 0 dB, maximum is +6 dB.
    double outputGain = 1.0;
    bool bypass = false;
};

// Independent stereo core, not yet connected to the Foundation plugin.
// One audio-thread owner. prepare() may allocate and must run off the callback;
// setParameters(), reset() and process() must not run concurrently.
class DarkDelay
{
  public:
    void prepare(double sampleRate, const DelayParameters &parameters = {});
    void reset() noexcept;
    void setParameters(const DelayParameters &parameters) noexcept;
    // Planar stereo, separate or corresponding in-place buffers, including zero frames.
    // prepare() is required first; invalid input samples are treated as silence.
    void process(const float *const *inputs, float *const *outputs, std::uint32_t frames) noexcept;

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
    static DelayParameters sanitize(const DelayParameters &parameters) noexcept;
    void startTimeChange() noexcept;
    double read(std::size_t channel, double delay) const noexcept;

    DelayParameters parameters;
    std::array<std::vector<double>, 2> buffers;
    std::array<double, 2> filtered{};
    std::size_t write = 0;
    double rate = 48000.0;
    double oldDelay = 19200.0;
    double newDelay = 19200.0;
    double requestedDelay = 19200.0;
    std::uint32_t fadeFrames = 2400;
    std::uint32_t fadeRemaining = 0;
    std::uint32_t smoothFrames = 960;
    Smoother feedback, mix, pole, gain, injection;
};

} // namespace stormsurge
