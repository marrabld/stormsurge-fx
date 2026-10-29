// SPDX-License-Identifier: GPL-3.0-only
#include "FloodGate.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace stormsurge
{
namespace
{
// These mirror floodGateSpecs in FloodParameters.hpp. A test keeps the two in step so the DSP never
// rejects a value the host can legally write.
constexpr double minThresholdDb = -80, maxThresholdDb = -20;
constexpr double minReleaseMs = 10, maxReleaseMs = 500;
constexpr double minRangeDb = -80, maxRangeDb = -6;

// Fast enough to keep most of a pick attack, slow enough that opening cannot click.
constexpr double attackMs = 1.0;
constexpr double detectorDecayMs = 5.0;
// Bridges the dips between pick attack and sustain so the gate does not flutter.
constexpr double holdSeconds = 0.020;
// Closes 6 dB below the opening level, so a decaying note does not chatter at the threshold.
constexpr double closeRatio = 0.5;

// The envelope decays geometrically, so digital silence would walk it into denormals within
// seconds. Flushing it, and the output, keeps the per-sample cost flat.
double flush(double value) noexcept
{
    return std::abs(value) < 1e-30 ? 0 : value;
}
double bounded(double value, double low, double high, double fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
double fromDecibels(double db) noexcept
{
    return std::pow(10.0, db / 20.0);
}
// z-plane pole for a time constant, so equal times mean equal speed at any sample rate. As with
// onePole() in Maelstrom this is the pole itself; the smoothing coefficient is its complement.
double decay(double ms, double rate) noexcept
{
    return std::exp(-1.0 / (ms * 0.001 * rate));
}
} // namespace

void FloodGate::Smooth::set(double next, std::uint32_t frames) noexcept
{
    if (frames == 0) {
        value = target = next;
        remaining = 0;
        step = 0;
    } else if (target != next) {
        target = next;
        remaining = frames;
        step = (next - value) / frames;
    }
}
double FloodGate::Smooth::tick() noexcept
{
    if (remaining != 0) {
        value += step;
        if (--remaining == 0)
            value = target;
    }
    return value;
}

GateParameters FloodGate::sanitize(const GateParameters &p) noexcept
{
    const GateParameters defaults;
    return {bounded(p.thresholdDb, minThresholdDb, maxThresholdDb, defaults.thresholdDb),
            bounded(p.releaseMs, minReleaseMs, maxReleaseMs, defaults.releaseMs),
            bounded(p.rangeDb, minRangeDb, maxRangeDb, defaults.rangeDb), p.enabled};
}

void FloodGate::prepare(double sampleRate, const GateParameters &p)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000 || sampleRate > 192000)
        throw std::invalid_argument("FloodGate sample rate must be 8000..192000 Hz");
    rate = sampleRate;
    parameters = sanitize(p);
    rampFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.020)));
    engageFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.010)));
    holdFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * holdSeconds)));
    attackPole = decay(attackMs, rate);
    detectorPole = decay(detectorDecayMs, rate);
    reset();
}
void FloodGate::coefficients(bool snap) noexcept
{
    const auto frames = snap ? 0u : rampFrames;
    threshold.set(fromDecibels(parameters.thresholdDb), frames);
    closedGain.set(fromDecibels(parameters.rangeDb), frames);
    engage.set(parameters.enabled ? 1.0 : 0.0, snap ? 0u : engageFrames);
    // Release is applied directly: it only changes how fast a continuous gain state moves, so a
    // jump in the coefficient cannot produce a jump in the output.
    releasePole = decay(parameters.releaseMs, rate);
}
void FloodGate::setParameters(const GateParameters &p) noexcept
{
    parameters = sanitize(p);
    coefficients(false);
}
void FloodGate::clearState() noexcept
{
    envelope = 0;
    gain = 1;
    holdRemaining = 0;
    // Start open: a reactivated gate must never swallow the first note.
    isOpen = true;
}
void FloodGate::reset() noexcept
{
    clearState();
    coefficients(true);
}

std::array<double, 2> FloodGate::processSample(const std::array<double, 2> &input) noexcept
{
    const double openLevel = threshold.tick();
    const double closedLevel = closedGain.tick();
    const double engageAmount = engage.tick();

    // Fail safe before any state can hold a non-finite value.
    const std::array<double, 2> dry{std::isfinite(input[0]) ? input[0] : 0.0,
                                    std::isfinite(input[1]) ? input[1] : 0.0};
    if (!std::isfinite(input[0]) || !std::isfinite(input[1]))
        clearState();

    // Instant attack, slow decay: a peak follower that cannot miss a transient.
    envelope = flush(std::max({std::abs(dry[0]), std::abs(dry[1]), envelope * detectorPole}));

    if (envelope > openLevel) {
        isOpen = true;
        holdRemaining = holdFrames;
    } else if (isOpen) {
        if (envelope >= openLevel * closeRatio)
            holdRemaining = holdFrames;
        else if (holdRemaining > 0)
            --holdRemaining;
        else
            isOpen = false;
    }

    const double goal = isOpen ? 1.0 : closedLevel;
    gain = goal + (goal > gain ? attackPole : releasePole) * (gain - goal);

    std::array<double, 2> output{};
    for (std::size_t channel = 0; channel < 2; ++channel) {
        const double value = dry[channel] * gain;
        output[channel] = flush(
            engageAmount >= 1.0 ? value : dry[channel] + (value - dry[channel]) * engageAmount);
    }
    return output;
}

std::array<double, 2> FloodGate::bypassSample(const std::array<double, 2> &input) noexcept
{
    // Unity dry with the same non-finite policy as processSample(), so a hard bypass can never
    // pass a NaN on to whatever follows.
    return {std::isfinite(input[0]) ? input[0] : 0.0, std::isfinite(input[1]) ? input[1] : 0.0};
}

} // namespace stormsurge
