// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace stormsurge
{

// Percentages are knob positions, not the fraction of bass or distortion applied.
// See docs/maelstrom.md for the approved contract behind these defaults.
struct OverdriveParameters {
    double drive = 0.10;
    double tight = 0.50;
    double bite = 0.50;
    // Linear amplitude. 1.0 is unity; 3.98107 is the approved +12 dB ceiling.
    double level = 1.0;
    bool enabled = true;
    bool grit = false;
};

// Host-independent tightening overdrive for feeding an amp modeller. One audio
// owner, prepare() off the processing callback before use. Channels are
// processed independently with no crossfeed, so identical inputs stay identical.
class MaelstromOverdrive
{
  public:
    void prepare(double sampleRate, const OverdriveParameters &parameters = {});
    void setParameters(const OverdriveParameters &parameters) noexcept;
    void reset() noexcept;

    // Enabled output, wet through the drive/tight/bite chain plus output level.
    std::array<double, 2> processSample(const std::array<double, 2> &input) noexcept;
    // Unity dry, ignoring every control. The integration layer keeps this
    // aligned with the enabled path, so bypass can never add level or click.
    std::array<double, 2> bypassSample(const std::array<double, 2> &input) noexcept;

    // Processing latency in samples. Oversampling filters are linear phase, so
    // this is the oversampling factor's group delay. Zero means no added latency.
    std::uint32_t latencySamples() const noexcept { return latency; }

  private:
    struct Channel {
        // Two cascaded one-pole highpasss, so Tight removes loose bass without
        // touching the attack range and without ringing or added latency.
        double bass1 = 0, bass2 = 0;
        // Bite rolls off the harmonics the clip stage just generated.
        double biteLow = 0;
        // One-pole anti-alias lowpass for the oversampled stream.
        double antiAlias = 0;
        void reset() noexcept;
    };
    struct Smooth {
        double value = 0, target = 0, step = 0;
        std::uint32_t remaining = 0;
        void set(double next, std::uint32_t frames) noexcept;
        double tick() noexcept;
    };

    static OverdriveParameters sanitize(const OverdriveParameters &p) noexcept;
    void coefficients(bool snap) noexcept;
    // Oversampling is prepared into fixed storage; no allocation happens here.
    double oversampled(double sample, Channel &state, double preClip, double gritAmount) noexcept;
    void updateAntiAlias() noexcept;

    std::array<Channel, 2> channels{};
    Smooth drive, tight, bite, level, engage, grit;
    OverdriveParameters parameters;
    double rate = 48000, antiAliasPole = 0;
    std::uint32_t rampFrames = 960, engageFrames = 480;
    std::uint32_t oversampling = 1, latency = 0;
};

} // namespace stormsurge
