// SPDX-License-Identifier: GPL-3.0-only
#include "Squall.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace stormsurge
{
namespace
{
constexpr std::array<std::array<double, 4>, 2> combLengthsMs{{
    {29.7, 37.1, 41.1, 43.7},
    {31.3, 39.7, 44.9, 49.3},
}};
constexpr std::array<std::array<double, 2>, 2> diffuserLengthsMs{{
    {5.3, 12.7},
    {6.1, 15.1},
}};
constexpr double diffuserCoefficient = 0.5;
constexpr double reverbDecaySeconds = 0.45;
constexpr double voiceDrive = 2.1;
constexpr double saturationBlend = 0.4;
constexpr double lowMidCut = 0.12;

double flush(double value) noexcept
{
    return std::abs(value) < 1.0e-30 ? 0.0 : value;
}

double pole(double frequency, double sampleRate) noexcept
{
    return std::exp(-2.0 * std::numbers::pi * frequency / sampleRate);
}
} // namespace

void Squall::DelayLine::prepare(std::size_t frames)
{
    data.assign(std::max<std::size_t>(frames, 1), 0.0);
    position = 0;
}

void Squall::DelayLine::reset() noexcept
{
    std::fill(data.begin(), data.end(), 0.0);
    position = 0;
}

double Squall::DelayLine::read() const noexcept
{
    return data[position];
}

void Squall::DelayLine::write(double value) noexcept
{
    data[position] = flush(value);
    if (++position == data.size())
        position = 0;
}

void Squall::Smooth::snap(double next) noexcept
{
    value = target = next;
    step = 0;
    remaining = 0;
}

void Squall::Smooth::set(double next, std::uint32_t frames) noexcept
{
    if (target == next)
        return;
    target = next;
    remaining = frames;
    step = (target - value) / static_cast<double>(frames);
}

double Squall::Smooth::tick() noexcept
{
    if (remaining != 0) {
        value += step;
        if (--remaining == 0)
            value = target;
    }
    return value;
}

SquallParameters Squall::sanitize(const SquallParameters &p) noexcept
{
    return {p.heavy, p.bypass};
}

void Squall::prepare(double sampleRate, const SquallParameters &p)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0)
        throw std::invalid_argument("Squall sample rate must be 8000..192000 Hz");

    rate = sampleRate;
    parameters = sanitize(p);
    rampFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.020)));
    dampingPole = pole(6500.0, rate);
    highpassPole = pole(85.0, rate);
    lowMidLowPole = pole(180.0, rate);
    lowMidHighPole = pole(420.0, rate);

    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t i = 0; i < CombCount; ++i) {
            const auto frames =
                static_cast<std::size_t>(std::lround(rate * combLengthsMs[channel][i] / 1000.0));
            combs[channel][i].prepare(frames);
            const double seconds = static_cast<double>(frames) / rate;
            combFeedback[channel][i] = std::pow(0.001, seconds / reverbDecaySeconds);
        }
        for (std::size_t i = 0; i < DiffuserCount; ++i) {
            const auto frames = static_cast<std::size_t>(
                std::lround(rate * diffuserLengthsMs[channel][i] / 1000.0));
            diffusers[channel][i].prepare(frames);
        }
    }
    reset();
}

void Squall::setParameters(const SquallParameters &p) noexcept
{
    parameters = sanitize(p);
    modeBlend.set(parameters.heavy ? 1.0 : 0.0, rampFrames);
    bypassBlend.set(parameters.bypass ? 1.0 : 0.0, rampFrames);
}

void Squall::reset() noexcept
{
    for (auto &channel : combs)
        for (auto &line : channel)
            line.reset();
    for (auto &channel : diffusers)
        for (auto &line : channel)
            line.reset();
    for (auto &channel : combDamped)
        channel.fill(0.0);
    for (auto &channel : voiceFilters)
        channel.fill(0.0);
    modeBlend.snap(parameters.heavy ? 1.0 : 0.0);
    bypassBlend.snap(parameters.bypass ? 1.0 : 0.0);
}

double Squall::heavyVoice(std::size_t channel, double input) noexcept
{
    auto &state = voiceFilters[channel];
    state[0] = flush((1.0 - highpassPole) * input + highpassPole * state[0]);
    const double highPassed = input - state[0];
    state[1] = flush((1.0 - lowMidLowPole) * highPassed + lowMidLowPole * state[1]);
    state[2] = flush((1.0 - lowMidHighPole) * highPassed + lowMidHighPole * state[2]);
    const double reduced = highPassed - lowMidCut * (state[2] - state[1]);
    const double saturated = std::tanh(voiceDrive * reduced) / voiceDrive;
    return reduced * (1.0 - saturationBlend) + saturated * saturationBlend;
}

std::array<double, 2> Squall::reverberate(const std::array<double, 2> &input) noexcept
{
    std::array<double, 2> output{};
    for (std::size_t channel = 0; channel < 2; ++channel) {
        double diffused = input[channel];
        for (auto &line : diffusers[channel]) {
            const double delayed = line.read();
            const double next = delayed - diffuserCoefficient * diffused;
            line.write(diffused + diffuserCoefficient * next);
            diffused = next;
        }

        double sum = 0.0;
        for (std::size_t i = 0; i < CombCount; ++i) {
            auto &line = combs[channel][i];
            const double delayed = line.read();
            auto &damped = combDamped[channel][i];
            damped = flush((1.0 - dampingPole) * delayed + dampingPole * damped);
            sum += delayed;
            line.write(diffused * 0.25 + combFeedback[channel][i] * damped);
        }
        output[channel] = sum * 0.5;
    }
    return output;
}

void Squall::process(const float *const *inputs, float *const *outputs, std::uint32_t frames,
                     std::uint32_t channels) noexcept
{
    if (frames == 0 || (channels != 1 && channels != 2))
        return;

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        std::array<double, 2> dry{};
        std::array<double, 2> selected{};
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            const float sample = inputs[channel][frame];
            dry[channel] = std::isfinite(sample) ? static_cast<double>(sample) : 0.0;
            selected[channel] = dry[channel];
        }

        const double heavy = modeBlend.tick();
        const double bypass = bypassBlend.tick();
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            const double colored = heavyVoice(channel, dry[channel]);
            selected[channel] = dry[channel] + heavy * (colored - dry[channel]);
        }

        const auto wet = reverberate(selected);
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            const double effected =
                selected[channel] * (1.0 - reverbMix) + wet[channel] * reverbMix;
            outputs[channel][frame] =
                static_cast<float>(effected * (1.0 - bypass) + dry[channel] * bypass);
        }
    }
}

} // namespace stormsurge
