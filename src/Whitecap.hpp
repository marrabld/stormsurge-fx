// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace stormsurge
{

enum class WhitecapMode : std::uint8_t { Chorus, Vibrato };

struct WhitecapParameters {
    double rateHz = 0.5;
    double depthMs = 0.5;
    double mix = 0.20;
    double widthDegrees = 20.0;
    WhitecapMode mode = WhitecapMode::Chorus;
    bool bypass = false;
};

// Host-independent stereo modulation core. prepare() allocates off the audio callback.
// One audio-thread owner; prepare(), reset(), setParameters() and process() must not
// run concurrently. The 10 ms centre delay is intrinsic to the modulated signal.
class Whitecap
{
  public:
    void prepare(double sampleRate, const WhitecapParameters &parameters = {});
    void reset() noexcept;
    void setParameters(const WhitecapParameters &parameters) noexcept;
    // Planar stereo, separate or corresponding in-place buffers, including zero frames.
    // Invalid input samples are treated as silence. No allocation occurs here.
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

    WhitecapParameters sanitize(const WhitecapParameters &parameters) const noexcept;
    double read(std::size_t channel, double delaySamples) const noexcept;

    WhitecapParameters parameters;
    std::array<std::vector<double>, 2> buffers;
    std::array<Smoother, 4> controls;
    Smoother modeBlend, bypassBlend;
    std::size_t write = 0;
    double sampleRate = 48000.0;
    double phase = 0.0;
    std::uint32_t smoothFrames = 960;
};

} // namespace stormsurge
