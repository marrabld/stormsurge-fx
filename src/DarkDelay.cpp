// SPDX-License-Identifier: GPL-3.0-only
#include "DarkDelay.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace stormsurge
{

namespace
{
double bounded(double value, double low, double high, double fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

double flushTiny(double value) noexcept
{
    // Prevent subnormal feedback state without depending on the host's FP mode.
    return std::abs(value) < 1.0e-30 ? 0.0 : value;
}
} // namespace

void DarkDelay::Smoother::snap(double next) noexcept
{
    value = target = next;
    remaining = 0;
    step = 0.0;
}

void DarkDelay::Smoother::set(double next, std::uint32_t frames) noexcept
{
    if (next == target)
        return;
    target = next;
    remaining = frames;
    step = (target - value) / frames;
}

double DarkDelay::Smoother::tick() noexcept
{
    if (remaining != 0) {
        value += step;
        if (--remaining == 0)
            value = target;
    }
    return value;
}

DelayParameters DarkDelay::sanitize(const DelayParameters &p) noexcept
{
    DelayParameters result = p;
    result.timeMs = bounded(p.timeMs, 20.0, 2000.0, 400.0);
    result.feedback = bounded(p.feedback, 0.0, 0.90, 0.35);
    result.mix = bounded(p.mix, 0.0, 1.0, 0.20);
    result.toneHz = bounded(p.toneHz, 400.0, 16000.0, 2500.0);
    result.outputGain = bounded(p.outputGain, 0.0, std::pow(10.0, 6.0 / 20.0), 1.0);
    return result;
}

void DarkDelay::prepare(double sampleRate, const DelayParameters &p)
{
    // Limit memory requests and make the supported range explicit.
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0)
        throw std::invalid_argument("DarkDelay sample rate must be 8000..192000 Hz");
    rate = sampleRate;
    const auto size = static_cast<std::size_t>(std::ceil(rate * 2.0)) + 2;
    for (auto &buffer : buffers)
        buffer.assign(size, 0.0);
    smoothFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.020)));
    fadeFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.050)));
    parameters = sanitize(p);
    reset();
}

void DarkDelay::reset() noexcept
{
    for (auto &buffer : buffers)
        std::fill(buffer.begin(), buffer.end(), 0.0);
    filtered.fill(0.0);
    write = 0;
    fadeRemaining = 0;
    oldDelay = newDelay = requestedDelay = rate * parameters.timeMs / 1000.0;
    feedback.snap(parameters.feedback);
    mix.snap(parameters.mix);
    gain.snap(parameters.outputGain);
    injection.snap(parameters.bypass ? 0.0 : 1.0);
    pole.snap(std::exp(-2.0 * std::numbers::pi * std::min(parameters.toneHz, rate * 0.45) / rate));
}

void DarkDelay::setParameters(const DelayParameters &p) noexcept
{
    parameters = sanitize(p);
    feedback.set(parameters.feedback, smoothFrames);
    mix.set(parameters.mix, smoothFrames);
    gain.set(parameters.outputGain, smoothFrames);
    injection.set(parameters.bypass ? 0.0 : 1.0, smoothFrames);
    pole.set(std::exp(-2.0 * std::numbers::pi * std::min(parameters.toneHz, rate * 0.45) / rate),
             smoothFrames);
    requestedDelay = rate * parameters.timeMs / 1000.0;
    if (fadeRemaining == 0)
        startTimeChange();
}

void DarkDelay::startTimeChange() noexcept
{
    if (requestedDelay != oldDelay) {
        newDelay = requestedDelay;
        fadeRemaining = fadeFrames;
    }
}

double DarkDelay::read(std::size_t channel, double delay) const noexcept
{
    const auto size = buffers[channel].size();
    double position = static_cast<double>(write) - delay;
    if (position < 0.0)
        position += static_cast<double>(size);
    const auto first = static_cast<std::size_t>(position);
    const auto second = (first + 1) % size;
    const double fraction = position - static_cast<double>(first);
    return buffers[channel][first] * (1.0 - fraction) + buffers[channel][second] * fraction;
}

void DarkDelay::process(const float *const *inputs, float *const *outputs,
                        std::uint32_t frames) noexcept
{
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const double f = feedback.tick();
        const double m = mix.tick();
        const double a = pole.tick();
        const double g = gain.tick();
        const double inputGain = injection.tick();
        const double blend = fadeRemaining == 0
                                 ? 0.0
                                 : static_cast<double>(fadeFrames - fadeRemaining + 1) / fadeFrames;
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const double dry = std::isfinite(inputs[channel][frame]) ? inputs[channel][frame] : 0.0;
            const double delayed = fadeRemaining == 0 ? read(channel, oldDelay)
                                                      : read(channel, oldDelay) * (1.0 - blend) +
                                                            read(channel, newDelay) * blend;
            // Filter every wet repeat, including the first, and recirculate it.
            // Convex interpolation, filtering and crossfades cannot boost loop gain.
            filtered[channel] = flushTiny((1.0 - a) * delayed + a * filtered[channel]);
            buffers[channel][write] = flushTiny(dry * inputGain + f * filtered[channel]);
            // Bypass restores unity dry while preserving the existing wet tail.
            outputs[channel][frame] =
                static_cast<float>((dry * (1.0 - m * inputGain) + filtered[channel] * m) * g);
        }
        write = (write + 1) % buffers[0].size();
        if (fadeRemaining != 0 && --fadeRemaining == 0) {
            oldDelay = newDelay;
            // Finish the current fade before servicing the latest automation value.
            startTimeChange();
        }
    }
}

} // namespace stormsurge
