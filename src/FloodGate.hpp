// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace stormsurge
{

// Values are in the units the host shows (dB, ms), matching floodGateSpecs in
// FloodGateParameters.hpp. Defaults are proposed starting points, not approved values.
struct GateParameters {
    double thresholdDb = -60.0;
    double releaseMs = 80.0;
    // Gain while closed. Finite on purpose: a true -inf floor lets the tail go denormal.
    double rangeDb = -80.0;
    bool enabled = true;
};

// Host-independent noise gate. One audio owner, prepare() off the processing callback before
// use; it throws std::invalid_argument for an unusable sample rate. Unlike Maelstrom the channels
// are NOT independent: the detector follows the louder of the two so a closing gate cannot pull the
// stereo image. Identical inputs still give identical outputs. No lookahead, so a pick attack is
// softened by the attack time instead of delayed.
class FloodGate
{
  public:
    void prepare(double sampleRate, const GateParameters &parameters = {});
    void setParameters(const GateParameters &parameters) noexcept;
    void reset() noexcept;

    // Enabled output: gated, crossfaded from dry by the engage ramp so toggling never clicks.
    // The detector keeps running while disabled, so enabling mid-note is already in step.
    std::array<double, 2> processSample(const std::array<double, 2> &input) noexcept;
    // Unity dry, ignoring every control.
    std::array<double, 2> bypassSample(const std::array<double, 2> &input) noexcept;

    // Zero means no added latency. Kept as a function so the integration layer reports every
    // pedal the same way if lookahead is ever added.
    std::uint32_t latencySamples() const noexcept { return 0; }

  private:
    struct Smooth {
        double value = 0, target = 0, step = 0;
        std::uint32_t remaining = 0;
        void set(double next, std::uint32_t frames) noexcept;
        double tick() noexcept;
    };

    static GateParameters sanitize(const GateParameters &p) noexcept;
    void clearState() noexcept;
    void coefficients(bool snap) noexcept;

    Smooth threshold, closedGain, engage;
    GateParameters parameters;
    double rate = 48000;
    double attackPole = 0, releasePole = 0, detectorPole = 0;
    double envelope = 0, gain = 1;
    std::uint32_t holdFrames = 960, holdRemaining = 0;
    std::uint32_t rampFrames = 960, engageFrames = 480;
    bool isOpen = true;
};

} // namespace stormsurge
