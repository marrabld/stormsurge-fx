// SPDX-License-Identifier: GPL-3.0-only
#include "DarkReverb.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace stormsurge
{
namespace
{
double flush(double value) noexcept
{
    return std::abs(value) < 1e-30 ? 0 : value;
}
double bounded(double value, double low, double high, double fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
constexpr std::array<double, 8> lengths{29.7, 37.1, 41.1, 43.7, 53.3, 61.7, 71.9, 79.3};
constexpr double normalization = 0.3535533905932738; // 1/sqrt(8)
constexpr std::array<double, 8> inputRight{1, -1, 1, -1, 1, -1, 1, -1};
constexpr std::array<double, 8> outputLeft{1, 1, -1, -1, 1, 1, -1, -1};
constexpr std::array<double, 8> outputRight{1, 1, 1, 1, -1, -1, -1, -1};
} // namespace

void DarkReverb::Line::prepare(std::size_t frames)
{
    data.assign(frames | 1u, 0.0);
    position = 0;
}
void DarkReverb::Line::reset() noexcept
{
    std::fill(data.begin(), data.end(), 0.0);
    position = 0;
}
double DarkReverb::Line::read() const noexcept
{
    return data[position];
}
void DarkReverb::Line::write(double value) noexcept
{
    data[position] = flush(value);
    if (++position == data.size())
        position = 0;
}
void DarkReverb::Smooth::set(double next, std::uint32_t frames) noexcept
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
double DarkReverb::Smooth::tick() noexcept
{
    if (remaining != 0) {
        value += step;
        if (--remaining == 0)
            value = target;
    }
    return value;
}
ReverbParameters DarkReverb::sanitize(const ReverbParameters &p) noexcept
{
    return {bounded(p.spaceSeconds, 1, 12, 6), bounded(p.darkness, 0, 100, 65),
            bounded(p.amount, 0, 1, 0)};
}
void DarkReverb::prepare(double sampleRate, const ReverbParameters &p)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000 || sampleRate > 192000)
        throw std::invalid_argument("DarkReverb sample rate must be 8000..192000 Hz");
    rate = sampleRate;
    parameters = sanitize(p);
    rampFrames = std::max(1u, static_cast<std::uint32_t>(std::lround(rate * 0.020)));
    highpassPole = std::exp(-2 * std::numbers::pi * 150 / rate);
    for (std::size_t i = 0; i < lines.size(); ++i)
        lines[i].prepare(static_cast<std::size_t>(std::lround(rate * lengths[i] / 1000)));
    constexpr double diffusion[2][2]{{5.3, 12.7}, {6.1, 15.1}};
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t i = 0; i < 2; ++i)
            diffusers[channel][i].prepare(
                static_cast<std::size_t>(std::lround(rate * diffusion[channel][i] / 1000)));
    reset();
}
void DarkReverb::coefficients(bool snap) noexcept
{
    const auto frames = snap ? 0 : rampFrames;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const double seconds = static_cast<double>(lines[i].data.size()) / rate;
        gains[i].set(std::exp(std::log(0.001) * seconds / parameters.spaceSeconds), frames);
    }
    const double cutoff =
        std::min(12000.0 * std::pow(800.0 / 12000, parameters.darkness / 100), rate * 0.45);
    pole.set(std::exp(-2 * std::numbers::pi * cutoff / rate), frames);
    amount.set(parameters.amount, frames);
}
void DarkReverb::setParameters(const ReverbParameters &p) noexcept
{
    parameters = sanitize(p);
    coefficients(false);
}
void DarkReverb::reset() noexcept
{
    for (auto &line : lines)
        line.reset();
    for (auto &channel : diffusers)
        for (auto &line : channel)
            line.reset();
    damped.fill(0);
    lowInput.fill(0);
    coefficients(true);
}
std::array<double, 2> DarkReverb::processSample(const std::array<double, 2> &input) noexcept
{
    std::array<double, 2> clean{}, diffused{}, reverberated{};
    for (std::size_t channel = 0; channel < 2; ++channel) {
        clean[channel] = std::isfinite(input[channel]) ? input[channel] : 0;
        lowInput[channel] =
            flush((1 - highpassPole) * clean[channel] + highpassPole * lowInput[channel]);
        double value = clean[channel] - lowInput[channel];
        for (auto &line : diffusers[channel]) {
            const double output = line.read() - 0.5 * value;
            line.write(value + 0.5 * output);
            value = output;
        }
        diffused[channel] = value;
    }
    const double a = pole.tick();
    double sum = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        damped[i] = flush((1 - a) * lines[i].read() + a * damped[i]);
        sum += damped[i];
        reverberated[0] += normalization * outputLeft[i] * damped[i];
        reverberated[1] += normalization * outputRight[i] * damped[i];
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        // Householder reflection: orthogonal mixing before per-line loss.
        // Input columns and output rows are normalized and mutually orthogonal.
        const double scattered = damped[i] - 0.25 * sum;
        const double incoming = normalization * (diffused[0] + inputRight[i] * diffused[1]);
        lines[i].write(incoming + gains[i].tick() * scattered);
    }
    const double blend = amount.tick();
    if (blend == 0)
        return clean; // exact delay compatibility, not an approximate zero blend
    for (std::size_t channel = 0; channel < 2; ++channel)
        clean[channel] = clean[channel] * (1 - blend) + reverberated[channel] * blend;
    return clean;
}

} // namespace stormsurge
