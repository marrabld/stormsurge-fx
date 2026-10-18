// SPDX-License-Identifier: GPL-3.0-only
#include "Whitecap.hpp"

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

void Whitecap::Smoother::snap(double next) noexcept
{
    value = target = next;
    step = 0.0;
    remaining = 0;
}

void Whitecap::Smoother::set(double next, std::uint32_t frames) noexcept
{
    if (next == target)
        return;
    target = next;
    remaining = frames;
    step = (target - value) / frames;
}

double Whitecap::Smoother::tick() noexcept
{
    if (remaining != 0) {
        value += step;
        if (--remaining == 0)
            value = target;
    }
    return value;
}

WhitecapParameters Whitecap::sanitize(const WhitecapParameters &p) const noexcept
{
    WhitecapParameters result = p;
    result.rateHz = bounded(p.rateHz, 0.1, 5.0, 0.5);
    result.depthMs = bounded(p.depthMs, 0.0, 2.0, 0.5);
    result.mix = bounded(p.mix, 0.0, 1.0, 0.20);
    result.widthDegrees = bounded(p.widthDegrees, 0.0, 90.0, 20.0);
    if (p.mode != WhitecapMode::Chorus && p.mode != WhitecapMode::Vibrato)
        result.mode = WhitecapMode::Chorus;
    return result;
}

void Whitecap::prepare(double rate, const WhitecapParameters &p)
{
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 192000.0)
        throw std::invalid_argument("Whitecap sample rate must be 8000..192000 Hz");
    sampleRate = rate;
    const auto capacity = static_cast<std::size_t>(std::ceil(sampleRate * 0.012)) + 2;
    for (auto &buffer : buffers)
        buffer.assign(capacity, 0.0);
    smoothFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(sampleRate * 0.020)));
    parameters = sanitize(p);
    reset();
}

void Whitecap::reset() noexcept
{
    for (auto &buffer : buffers)
        std::fill(buffer.begin(), buffer.end(), 0.0);
    write = 0;
    phase = 0.0;
    controls[0].snap(parameters.rateHz);
    controls[1].snap(parameters.depthMs);
    controls[2].snap(parameters.mix);
    controls[3].snap(parameters.widthDegrees);
    modeBlend.snap(parameters.mode == WhitecapMode::Vibrato ? 1.0 : 0.0);
}

void Whitecap::setParameters(const WhitecapParameters &p) noexcept
{
    parameters = sanitize(p);
    controls[0].set(parameters.rateHz, smoothFrames);
    controls[1].set(parameters.depthMs, smoothFrames);
    controls[2].set(parameters.mix, smoothFrames);
    controls[3].set(parameters.widthDegrees, smoothFrames);
    modeBlend.set(parameters.mode == WhitecapMode::Vibrato ? 1.0 : 0.0, smoothFrames);
}

double Whitecap::read(std::size_t channel, double delaySamples) const noexcept
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

void Whitecap::process(const float *const *inputs, float *const *outputs,
                       std::uint32_t frames) noexcept
{
    constexpr double centreDelayMs = 10.0;
    constexpr double fullTurn = 2.0 * std::numbers::pi;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const double rate = controls[0].tick();
        const double depthSamples = controls[1].tick() * sampleRate / 1000.0;
        const double mix = controls[2].tick();
        const double width = controls[3].tick() * std::numbers::pi / 180.0;
        const double vibrato = modeBlend.tick();
        const std::array<double, 2> input{std::isfinite(inputs[0][frame]) ? inputs[0][frame] : 0.0,
                                          std::isfinite(inputs[1][frame]) ? inputs[1][frame] : 0.0};

        for (std::size_t channel = 0; channel < 2; ++channel)
            buffers[channel][write] = flushTiny(input[channel]);

        for (std::size_t channel = 0; channel < 2; ++channel) {
            const double channelPhase = phase + (channel == 0 ? -0.5 : 0.5) * width;
            const double modulation = std::sin(channelPhase) * depthSamples;
            const double delay = centreDelayMs * sampleRate / 1000.0 + modulation;
            const double wet = read(channel, delay);
            const double chorus = input[channel] * (1.0 - mix) + wet * mix;
            const double output = chorus * (1.0 - vibrato) + wet * vibrato;
            outputs[channel][frame] = static_cast<float>(output);
        }

        write = (write + 1) % buffers[0].size();
        phase += fullTurn * rate / sampleRate;
        if (phase >= fullTurn)
            phase -= fullTurn;
    }
}

} // namespace stormsurge
