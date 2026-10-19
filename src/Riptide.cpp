// SPDX-License-Identifier: GPL-3.0-only
#include "Riptide.hpp"

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
    return std::abs(value) < 1.0e-30 ? 0.0 : value;
}
} // namespace

void Riptide::Smoother::snap(double next) noexcept
{
    value = target = next;
    step = 0.0;
    remaining = 0;
}

void Riptide::Smoother::set(double next, std::uint32_t frames) noexcept
{
    if (next == target)
        return;
    target = next;
    remaining = frames;
    step = (target - value) / frames;
}

double Riptide::Smoother::tick() noexcept
{
    if (remaining != 0) {
        value += step;
        if (--remaining == 0)
            value = target;
    }
    return value;
}

RiptideParameters Riptide::sanitize(const RiptideParameters &p) const noexcept
{
    return {bounded(p.shiftSemitones, -12.0, 0.0, 0.0), bounded(p.blend, 0.0, 1.0, 0.0)};
}

void Riptide::prepare(double rate, std::uint32_t channelCount, const RiptideParameters &p)
{
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 192000.0)
        throw std::invalid_argument("Riptide sample rate must be 8000..192000 Hz");
    if (channelCount == 0 || channelCount > 2)
        throw std::invalid_argument("Riptide supports mono or stereo audio");

    sampleRate = rate;
    channels = channelCount;
    minimumDelaySamples = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.001)));
    // A 40 ms grain span keeps the two overlapping read heads farther apart in
    // phase at guitar fundamentals than a shorter window would.
    grainSpanSamples = std::max(2u, static_cast<std::uint32_t>(std::lround(rate * 0.040)));
    maximumDelaySamples = minimumDelaySamples + grainSpanSamples;
    const auto capacity = static_cast<std::size_t>(maximumDelaySamples) + 2;
    for (auto &buffer : buffers)
        buffer.assign(capacity, 0.0);
    smoothingFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.020)));
    parameters = sanitize(p);
    reset();
}

void Riptide::reset() noexcept
{
    for (auto &buffer : buffers)
        std::fill(buffer.begin(), buffer.end(), 0.0);
    write = 0;
    phase = 0.0;
    pitchRatio.snap(std::pow(2.0, parameters.shiftSemitones / 12.0));
    blend.snap(parameters.blend);
}

void Riptide::setParameters(const RiptideParameters &p) noexcept
{
    parameters = sanitize(p);
    pitchRatio.set(std::pow(2.0, parameters.shiftSemitones / 12.0), smoothingFrames);
    blend.set(parameters.blend, smoothingFrames);
}

double Riptide::read(std::uint32_t channel, double delaySamples) const noexcept
{
    const auto size = buffers[channel].size();
    double position = static_cast<double>(write) - delaySamples;
    if (position < 0.0)
        position += static_cast<double>(size);
    const auto first = static_cast<std::size_t>(position);
    const auto second = (first + 1) % size;
    const double fraction = position - static_cast<double>(first);
    return buffers[channel][first] * (1.0 - fraction) + buffers[channel][second] * fraction;
}

void Riptide::process(const float *const *inputs, float *const *outputs,
                      std::uint32_t frames) noexcept
{
    constexpr double fullTurn = 2.0 * std::numbers::pi;
    const double span = static_cast<double>(grainSpanSamples);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const double ratio = pitchRatio.tick();
        const double mix = blend.tick();
        std::array<double, 2> input{};
        for (std::uint32_t channel = 0; channel < channels; ++channel)
            input[channel] = std::isfinite(inputs[channel][frame]) ? inputs[channel][frame] : 0.0;

        for (std::uint32_t channel = 0; channel < channels; ++channel)
            buffers[channel][write] = flushTiny(input[channel]);

        const double phaseA = phase;
        double phaseB = phase + 0.5;
        if (phaseB >= 1.0)
            phaseB -= 1.0;
        const double weightA = 0.5 - 0.5 * std::cos(fullTurn * phaseA);
        const double weightB = 1.0 - weightA;
        const double pitchStrength = std::clamp((1.0 - ratio) / 0.001, 0.0, 1.0);

        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            const double delayA = minimumDelaySamples + phaseA * span;
            const double delayB = minimumDelaySamples + phaseB * span;
            const double pitched =
                read(channel, delayA) * weightA + read(channel, delayB) * weightB;
            const double shifted = input[channel] * (1.0 - pitchStrength) + pitched * pitchStrength;
            outputs[channel][frame] =
                static_cast<float>(input[channel] * (1.0 - mix) + shifted * mix);
        }

        write = (write + 1) % buffers[0].size();
        phase += (1.0 - ratio) / span;
        if (phase >= 1.0)
            phase -= std::floor(phase);
    }
}

} // namespace stormsurge
