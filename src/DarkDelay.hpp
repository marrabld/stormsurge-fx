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
    bool pingPong = false;
};

// Normal independent-stereo and ping-pong core shared by plugin and standalone.
// One audio-thread owner. prepare() may allocate and must run off the callback;
// setParameters(), reset() and process() must not run concurrently.
class DarkDelay
{
  public:
    using WetInsert = std::array<double, 2> (*)(void *, const std::array<double, 2> &) noexcept;
    void prepare(double sampleRate, const DelayParameters &parameters = {},
                 double maximumDelayMs = 2000.0);
    void reset() noexcept;
    void setParameters(const DelayParameters &parameters) noexcept;
    // Planar stereo, separate or corresponding in-place buffers, including zero frames.
    // prepare() is required first; invalid input samples are treated as silence.
    // Optional wet-only insert runs outside feedback, before the final Mix/Level.
    void process(const float *const *inputs, float *const *outputs, std::uint32_t frames,
                 WetInsert insert = nullptr, void *context = nullptr) noexcept;

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
    DelayParameters sanitize(const DelayParameters &parameters) const noexcept;
    void startTimeChange() noexcept;
    double read(std::size_t channel, double delay) const noexcept;

    DelayParameters parameters;
    std::array<std::vector<double>, 2> buffers;
    std::array<double, 2> filtered{};
    std::size_t write = 0;
    double rate = 48000.0;
    double maximumMs = 2000.0;
    double oldDelay = 19200.0;
    double newDelay = 19200.0;
    double requestedDelay = 19200.0;
    std::uint32_t fadeFrames = 2400;
    std::uint32_t fadeRemaining = 0;
    std::uint32_t smoothFrames = 960;
    Smoother feedback, mix, pole, gain, injection, routing;
};

} // namespace stormsurge
