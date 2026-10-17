// SPDX-License-Identifier: GPL-3.0-only
#include "DarkReverb.hpp"
#include "DarkDelay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

using namespace stormsurge;
static bool track = false;
static std::size_t allocations = 0;
void *operator new(std::size_t size)
{
    if (track)
        ++allocations;
    if (void *p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    std::free(p);
}
static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
using Audio = std::array<std::vector<double>, 2>;
static Audio impulse(double rate, ReverbParameters p, double seconds)
{
    DarkReverb reverb;
    reverb.prepare(rate, p);
    const auto count = static_cast<std::size_t>(rate * seconds);
    Audio result{std::vector<double>(count), std::vector<double>(count)};
    track = true;
    for (std::size_t i = 0; i < count; ++i) {
        const auto sample = reverb.processSample({i == 0 ? 1.0 : 0.0, 0});
        result[0][i] = sample[0];
        result[1][i] = sample[1];
    }
    track = false;
    return result;
}
static double energy(const Audio &a, std::size_t start, std::size_t end)
{
    double sum = 0;
    for (const auto &channel : a)
        for (auto i = start; i < end; ++i)
            sum += channel[i] * channel[i];
    return sum;
}
static double brightness(const Audio &a)
{
    double difference = 0;
    for (const auto &channel : a)
        for (std::size_t i = 1; i < channel.size(); ++i)
            difference += std::pow(channel[i] - channel[i - 1], 2);
    return difference / energy(a, 0, a[0].size());
}
static std::array<double, 2> suppressWet(void *, const std::array<double, 2> &) noexcept
{
    return {0, 0};
}
int main()
{
    try {
        const std::array<ReverbParameters, 3> corners{{{6, 65, 1}, {1, 100, 1}, {12, 0, 1}}};
        for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
            for (ReverbParameters p : corners) {
                auto response = impulse(rate, p, 1.5);
                double stereoDifference = 0, peak = 0;
                for (std::size_t i = 0; i < response[0].size(); ++i) {
                    stereoDifference += std::abs(response[0][i] - response[1][i]);
                    for (const auto &channel : response) {
                        require(std::isfinite(channel[i]), "Nonfinite impulse response");
                        peak = std::max(peak, std::abs(channel[i]));
                    }
                }
                require(peak > 0 && peak < 2 && stereoDifference > 0.01,
                        "Missing/unstable stereo diffusion");
                require(energy(response, 0, static_cast<std::size_t>(rate * 0.025)) == 0,
                        "Reverb has unexpected immediate feedthrough at full Amount");
                require(energy(response, static_cast<std::size_t>(rate * 0.5), response[0].size()) >
                            1e-12,
                        "Reverb has no lingering tail");
            }
        }
        const auto shortSpace = impulse(8000, {2, 20, 1}, 10);
        const auto longSpace = impulse(8000, {10, 20, 1}, 10);
        require(energy(longSpace, 16000, 32000) > 10 * energy(shortSpace, 16000, 32000),
                "Space did not lengthen decay");
        require(energy(longSpace, 64000, 80000) < energy(longSpace, 4000, 20000) * 0.01,
                "Long tail failed to decay");
        const auto bright = impulse(8000, {6, 0, 1}, 2);
        const auto dark = impulse(8000, {6, 100, 1}, 2);
        require(brightness(dark) < brightness(bright) * 0.8,
                "Darkness did not reduce high-frequency response");

        DarkReverb reverb;
        reverb.prepare(8000);
        track = true;
        for (std::size_t i = 0; i < 8000; ++i) {
            const std::array<double, 2> input{std::sin(i * 0.31), -std::cos(i * 0.21)};
            require(reverb.processSample(input) == input, "Amount zero changed the delay signal");
        }
        reverb.reset();
        track = false;
        reverb.prepare(8000, {12, 0, 1});
        double peak = 0;
        std::array<double, 2> previous{};
        track = true;
        for (std::size_t i = 0; i < 64000; ++i) {
            if (i % 64 == 0)
                reverb.setParameters({i % 128 == 0 ? 1.0 : 12.0, i % 128 == 0 ? 0.0 : 100.0,
                                      i % 128 == 0 ? 0.0 : 1.0});
            const double input = i < 32000 ? 0.25 * std::sin(i * 0.14) : 0;
            const auto out = reverb.processSample({input, -input * 0.7});
            for (std::size_t channel = 0; channel < 2; ++channel) {
                const double sample = out[channel];
                require(std::isfinite(sample), "Control transitions produced nonfinite output");
                peak = std::max(peak, std::abs(sample));
                if (i > 1000 && i < 32000)
                    require(std::abs(sample - previous[channel]) < 0.2,
                            "Control changes produced a discontinuity on smooth input");
            }
            previous = out;
        }
        reverb.reset();
        for (int i = 0; i < 8000; ++i)
            require(reverb.processSample({0, 0}) == std::array<double, 2>{0, 0},
                    "Reset retained a tail");
        track = false;
        require(peak < 8, "Sustained/transition test produced excessive gain");
        reverb.prepare(192000, {6, 65, 1});
        require(reverb.processSample({0, 0}) == std::array<double, 2>{0, 0},
                "Rate change retained state");
        reverb.prepare(8000, {6, 65, 1});
        for (int i = 0; i < 2000; ++i)
            require(reverb.processSample({std::numeric_limits<double>::quiet_NaN(),
                                          std::numeric_limits<double>::infinity()}) ==
                        std::array<double, 2>{0, 0},
                    "Invalid input contaminated state");
        const double nan = std::numeric_limits<double>::quiet_NaN();
        auto invalid = impulse(8000, {nan, nan, nan}, 1);
        require(invalid[0][0] == 1 && invalid[1][0] == 0 && energy(invalid, 1, 8000) == 0,
                "Invalid parameters did not fall back to disabled reverb");
        require(impulse(8000, {99, 200, 2}, 1) == impulse(8000, {12, 100, 1}, 1),
                "Core parameter limits were not enforced");
        bool rejected = false;
        try {
            reverb.prepare(nan);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        require(rejected, "Invalid sample rate accepted");
        require(allocations == 0, "Allocated during processing, control updates or reset");
        DarkDelay delay;
        DelayParameters p;
        p.timeMs = 20;
        p.mix = 1;
        p.feedback = 0.5;
        delay.prepare(8000, p);
        std::vector<float> input(1000), zero(1000), output(1000), right(1000);
        input[0] = 1;
        const float *inputs[] = {input.data(), zero.data()};
        float *outputs[] = {output.data(), right.data()};
        delay.process(inputs, outputs, 240, suppressWet);
        const float *remaining[] = {input.data() + 240, zero.data() + 240};
        float *remainingOut[] = {output.data() + 240, right.data() + 240};
        delay.process(remaining, remainingOut, 760);
        require(output[160] == 0 && output[320] > 0,
                "Wet insert changed delay feedback rather than only the output path");
        std::puts("PASS: reverb diffusion, decay, darkness, disabled identity, reset/rates, "
                  "transitions, invalid values and allocations");
        return 0;
    } catch (const std::exception &error) {
        track = false;
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
