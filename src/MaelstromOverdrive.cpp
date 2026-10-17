// SPDX-License-Identifier: GPL-3.0-only
#include "MaelstromOverdrive.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace stormsurge
{
namespace
{
constexpr double maxPreClipGain = 60.0;
constexpr double unity = 1.0;

double flush(double value) noexcept
{
    return std::abs(value) < 1e-30 ? 0 : value;
}
double bounded(double value, double low, double high, double fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
// Tight is a highpass, not a treble cut. Removing loose bass while leaving the
// pick range alone is what a highpass does; a lowpass strong enough to matter at
// 110 Hz would also gut the 1-4 kHz attack that makes a riff audible.
//
// Any highpass strong enough to tighten palm mutes also attenuates a down-tuned
// low fundamental, so the default position has to stay mild. The raised exponent
// keeps most of the knob travel in the audible range: the midpoint lands near
// 85 Hz, where a normal low string loses a few dB rather than disappearing.
double tightCutoff(double tight) noexcept
{
    const double shaped = std::pow(tight, 1.4);
    return 25.0 * std::pow(700.0 / 25.0, shaped);
}
// Bite trades post-clip clarity against hiss-prone fizz. The ceiling is kept
// well below Nyquist even after clamping, because this stage runs before the
// anti-alias filter and must not invent new high-frequency content.
double biteCutoff(double bite, double rate) noexcept
{
    const double cutoff = 4500.0 * std::pow(11000.0 / 4500.0, bite);
    return std::min(cutoff, 0.45 * rate);
}
double onePole(double cutoff, double rate) noexcept
{
    return std::exp(-2 * std::numbers::pi * cutoff / rate);
}
// Drive starts at unity so the default 10% position is close to a clean push,
// then climbs steeply. A straight exponential to the ceiling would already add a
// few dB at 10%, which would quietly change level on insertion. The cube keeps
// the bottom of the knob usable while still reaching genuinely aggressive
// overdrive at the top, which is what tight melodic-death rhythm needs.
double preClipFor(double drive) noexcept
{
    const double shaped = drive * drive * drive;
    return unity + (maxPreClipGain - unity) * shaped;
}
} // namespace

void MaelstromOverdrive::Channel::reset() noexcept
{
    bass1 = bass2 = biteLow = antiAlias = 0;
}
void MaelstromOverdrive::Smooth::set(double next, std::uint32_t frames) noexcept
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
double MaelstromOverdrive::Smooth::tick() noexcept
{
    if (remaining != 0) {
        value += step;
        if (--remaining == 0)
            value = target;
    }
    return value;
}
OverdriveParameters MaelstromOverdrive::sanitize(const OverdriveParameters &p) noexcept
{
    return {bounded(p.drive, 0, 1, 0.10), bounded(p.tight, 0, 1, 0.50), bounded(p.bite, 0, 1, 0.50),
            // The approved ceiling is +12 dB; silence must stay reachable.
            bounded(p.level, 0, 3.9810717055349722, unity), p.enabled};
}

void MaelstromOverdrive::prepare(double sampleRate, const OverdriveParameters &p)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000 || sampleRate > 192000)
        throw std::invalid_argument("MaelstromOverdrive sample rate must be 8000..192000 Hz");
    rate = sampleRate;
    parameters = sanitize(p);
    rampFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.020)));
    engageFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.010)));
    // A high drive stage generates harmonics well past the audible band. Twice
    // oversampling with a gentle decimating lowpass keeps the grit musical
    // without the CPU and latency of a larger factor. Tight/Bite filtering
    // happens at this rate, so its cutoff tracking stays correct.
    oversampling = 2;
    latency = oversampling - 1;
    updateAntiAlias();
    reset();
}
void MaelstromOverdrive::updateAntiAlias() noexcept
{
    // Must sit below the base rate's Nyquist, not the oversampled one. Anything
    // above it would fold back on decimation, which is the whole point of the
    // extra rate. 0.45 leaves a margin for the filter's own transition band.
    antiAliasPole = onePole(0.45 * rate, rate * oversampling);
}
void MaelstromOverdrive::coefficients(bool snap) noexcept
{
    const auto frames = snap ? 0u : rampFrames;
    drive.set(preClipFor(parameters.drive), frames);
    tight.set(tightCutoff(parameters.tight), frames);
    bite.set(biteCutoff(parameters.bite, rate), frames);
    level.set(parameters.level, frames);
    engage.set(parameters.enabled ? 1.0 : 0.0, snap ? 0u : engageFrames);
}
void MaelstromOverdrive::setParameters(const OverdriveParameters &p) noexcept
{
    parameters = sanitize(p);
    coefficients(false);
}
void MaelstromOverdrive::reset() noexcept
{
    for (auto &channel : channels)
        channel.reset();
    coefficients(true);
}

double MaelstromOverdrive::oversampled(double sample, Channel &state, double gain) noexcept
{
    // Interpolation is a two-tap symmetric filter, so the reported latency
    // stays the small fixed integer documented in latencySamples().
    // onePole() returns the z-plane pole, so the smoothing coefficient is its
    // complement. Using the pole itself here would give p*y + p*x instead of
    // p*y + (1-p)*x, which is not a lowpass at all.
    // Cascaded one-pole highpasss: 12 dB/octave, no ringing, no added latency.
    // Each stage forms its own highpass before the next one sees it. Subtracting
    // one doubly-filtered signal from the input instead would put a resonant
    // bump of a couple of dB near the cutoff, so Tight would boost a band rather
    // than remove one.
    const double p = 1 - onePole(tight.value, rate * oversampling);
    state.bass1 += p * (sample - state.bass1);
    const double firstStage = sample - state.bass1;
    state.bass2 += p * (firstStage - state.bass2);
    double value = (firstStage - state.bass2) * gain;
    // tanh saturates smoothly and monotonically, so the stage cannot run away
    // or fold back on itself; a hard clamp here would only add aliasing.
    value = std::tanh(value);
    // Bite after clipping shapes the generated harmonics instead of pre-empting them.
    state.biteLow += (1 - onePole(bite.value, rate * oversampling)) * (value - state.biteLow);
    value = state.biteLow;
    state.antiAlias += (1 - antiAliasPole) * (value - state.antiAlias);
    return state.antiAlias;
}

std::array<double, 2> MaelstromOverdrive::processSample(const std::array<double, 2> &input) noexcept
{
    std::array<double, 2> output{};
    const double engageAmount = engage.tick();
    const double outputLevel = level.tick();
    const double preClip = drive.tick();
    for (std::size_t channel = 0; channel < 2; ++channel) {
        // Fail safe before any state can hold a non-finite value.
        const bool valid = std::isfinite(input[channel]);
        const double dry = valid ? input[channel] : 0.0;
        auto &state = channels[channel];
        if (!valid)
            state.reset();
        double held = dry;
        double wet = dry;
        for (std::uint32_t step = 0; step < oversampling; ++step) {
            const double fraction = (step + 1.0) / oversampling;
            const double up = held + (dry - held) * fraction;
            wet = oversampled(up, state, preClip);
            held = dry;
        }
        double value = wet * outputLevel;
        if (!std::isfinite(value)) {
            // Recover instead of emitting noise that would poison the amp model.
            state.reset();
            value = 0;
        }
        output[channel] = flush(engageAmount >= 1.0 ? value : dry + (value - dry) * engageAmount);
    }
    return output;
}

std::array<double, 2> MaelstromOverdrive::bypassSample(const std::array<double, 2> &input) noexcept
{
    // Unity dry, identical policy for non-finite input. This is the path a host
    // hard bypass takes; processSample() reaches the same signal via its fade.
    return {std::isfinite(input[0]) ? input[0] : 0.0, std::isfinite(input[1]) ? input[1] : 0.0};
}

} // namespace stormsurge
