// SPDX-License-Identifier: GPL-3.0-only
#include "Whitecap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <new>
#include <stdexcept>
#include <vector>

static bool trackAllocations = false;
static std::size_t allocations = 0;

void *operator new(std::size_t size)
{
    if (trackAllocations)
        ++allocations;
    if (void *pointer = std::malloc(size == 0 ? 1 : size))
        return pointer;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void *pointer) noexcept
{
    std::free(pointer);
}
void operator delete[](void *pointer) noexcept
{
    std::free(pointer);
}
void operator delete(void *pointer, std::size_t) noexcept
{
    std::free(pointer);
}
void operator delete[](void *pointer, std::size_t) noexcept
{
    std::free(pointer);
}

using stormsurge::Whitecap;
using stormsurge::WhitecapMode;
using stormsurge::WhitecapParameters;
using Audio = std::array<std::vector<float>, 2>;

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void near(double actual, double expected, double tolerance, const char *message)
{
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

static Audio signal(std::size_t frames)
{
    return {std::vector<float>(frames), std::vector<float>(frames)};
}

static void process(Whitecap &effect, const Audio &input, Audio &output, std::size_t offset,
                    std::uint32_t frames, bool inPlace = false)
{
    const float *inputs[] = {input[0].data() + offset, input[1].data() + offset};
    float *outputs[] = {output[0].data() + offset, output[1].data() + offset};
    if (inPlace) {
        std::copy_n(inputs[0], frames, outputs[0]);
        std::copy_n(inputs[1], frames, outputs[1]);
        inputs[0] = outputs[0];
        inputs[1] = outputs[1];
    }
    allocations = 0;
    trackAllocations = true;
    effect.process(inputs, outputs, frames);
    trackAllocations = false;
    require(allocations == 0, "Processing allocated memory");
}

static void centerDelayAndModes()
{
    constexpr double rate = 48000.0;
    constexpr std::size_t delay = 480;
    WhitecapParameters p;
    p.depthMs = 0.0;
    p.widthDegrees = 0.0;
    p.mix = 0.2;
    Whitecap effect;
    effect.prepare(rate, p);
    auto input = signal(delay + 4);
    input[0][0] = 1.0f;
    input[1][0] = 1.0f;
    auto output = signal(input[0].size());
    process(effect, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    near(output[0][0], 0.8, 1.0e-7, "Chorus dry component wrong");
    near(output[0][delay - 1], 0.0, 0.0, "Chorus arrived before centre delay");
    near(output[0][delay], 0.2, 1.0e-7, "Chorus wet component wrong");
    require(output[0] == output[1], "Zero width did not keep channels coherent");

    p.mode = WhitecapMode::Vibrato;
    effect.prepare(rate, p);
    output = signal(input[0].size());
    process(effect, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    near(output[0][0], 0.0, 0.0, "Vibrato leaked dry input");
    near(output[0][delay], 1.0, 1.0e-7, "Vibrato did not return the fully wet signal");

    for (double sampleRate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        const auto expected = static_cast<std::size_t>(std::lround(sampleRate * 0.010));
        p.depthMs = 0.0;
        effect.prepare(sampleRate, p);
        auto rateInput = signal(expected + 1);
        rateInput[0][0] = 1.0f;
        rateInput[1][0] = 1.0f;
        auto rateOutput = signal(rateInput[0].size());
        process(effect, rateInput, rateOutput, 0, static_cast<std::uint32_t>(rateInput[0].size()));
        near(rateOutput[0][expected], 1.0, 1.0e-7, "Centre delay changed with sample rate");
    }

    p.bypass = true;
    effect.prepare(rate, p);
    auto bypassOutput = signal(input[0].size());
    process(effect, input, bypassOutput, 0, static_cast<std::uint32_t>(input[0].size()));
    require(bypassOutput[0] == input[0] && bypassOutput[1] == input[1],
            "Bypass must return the unprocessed input");
}

static void stereoWidthAndModulation()
{
    constexpr double rate = 48000.0;
    constexpr std::size_t frames = 48000;
    WhitecapParameters p;
    p.mode = WhitecapMode::Vibrato;
    p.rateHz = 5.0;
    p.depthMs = 2.0;
    p.widthDegrees = 0.0;
    Whitecap effect;
    effect.prepare(rate, p);
    auto input = signal(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float sample =
            static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 220.0 * i / rate));
        input[0][i] = sample;
        input[1][i] = sample;
    }
    auto output = signal(frames);
    process(effect, input, output, 0, frames);
    require(output[0] == output[1], "Centered modulation differed between channels");
    const auto centered = output[0];

    p.widthDegrees = 90.0;
    effect.reset();
    effect.setParameters(p);
    process(effect, input, output, 0, frames);
    double difference = 0.0;
    for (std::size_t i = frames / 2; i < frames; ++i) {
        require(std::isfinite(output[0][i]) && std::isfinite(output[1][i]),
                "Modulated output became nonfinite");
        difference += std::abs(output[0][i] - output[1][i]);
    }
    require(difference > 10.0, "Width did not change the left/right movement");
    double monoEnergy = 0.0;
    for (std::size_t i = frames / 2; i < frames; ++i)
        monoEnergy += std::abs((output[0][i] + output[1][i]) * 0.5);
    require(monoEnergy > 1000.0, "Representative wide setting vanished in mono sum");
    double centerDifference = 0.0;
    for (std::size_t i = frames / 2; i < frames; ++i)
        centerDifference += std::abs(centered[i] - output[0][i]);
    require(centerDifference > 10.0, "Depth modulation had no measurable effect");
}

static void smoothingAndReset()
{
    constexpr double rate = 48000.0;
    WhitecapParameters p;
    p.depthMs = 1.0;
    p.mix = 0.25;
    Whitecap effect;
    effect.prepare(rate, p);
    auto input = signal(4096);
    for (std::size_t i = 0; i < input[0].size(); ++i) {
        const float sample =
            static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 220.0 * i / rate));
        input[0][i] = sample;
        input[1][i] = sample;
    }
    auto output = signal(input[0].size());
    process(effect, input, output, 0, 1024);

    p.mode = WhitecapMode::Vibrato;
    p.mix = 1.0;
    p.depthMs = 2.0;
    allocations = 0;
    trackAllocations = true;
    effect.setParameters(p);
    trackAllocations = false;
    require(allocations == 0, "Parameter update allocated memory");
    process(effect, input, output, 1024, 960);
    double largestStep = 0.0;
    for (std::size_t i = 1025; i < 1984; ++i)
        largestStep =
            std::max(largestStep, static_cast<double>(std::abs(output[0][i] - output[0][i - 1])));
    require(largestStep < 0.1, "Mode/parameter changes were not smoothed");

    allocations = 0;
    trackAllocations = true;
    effect.reset();
    trackAllocations = false;
    require(allocations == 0, "Reset allocated memory");
    output = signal(input[0].size());
    process(effect, input, output, 0, 1024);
    near(output[0][0], input[0][0] * (1.0 - p.mix), 1.0e-7,
         "Reset did not clear delayed signal state");
}

static void invalidParametersAndBuffers()
{
    WhitecapParameters p;
    p.rateHz = std::numeric_limits<double>::quiet_NaN();
    p.depthMs = std::numeric_limits<double>::infinity();
    p.mix = -10.0;
    p.widthDegrees = 200.0;
    Whitecap effect;
    effect.prepare(8000.0, p);
    auto input = signal(257);
    input[0][0] = std::numeric_limits<float>::quiet_NaN();
    input[1][0] = 1.0f;
    auto output = signal(input[0].size());
    process(effect, input, output, 0, 0);
    process(effect, input, output, 0, static_cast<std::uint32_t>(input[0].size()), true);
    require(
        std::all_of(output[0].begin(), output[0].end(), [](float x) { return std::isfinite(x); }),
        "Invalid input or parameters produced nonfinite output");
    for (auto &channel : input)
        std::fill(channel.begin(), channel.end(), 0.0f);
    effect.setParameters(p);
    process(effect, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    require(
        std::all_of(output[0].begin(), output[0].end(), [](float x) { return std::isfinite(x); }),
        "Nonfinite parameter updates corrupted processing");

    bool rejectedRate = false;
    try {
        effect.prepare(7999.0, {});
    } catch (const std::invalid_argument &) {
        rejectedRate = true;
    }
    require(rejectedRate, "Unsupported sample rate was accepted");
}

int main()
{
    try {
        centerDelayAndModes();
        stereoWidthAndModulation();
        smoothingAndReset();
        invalidParametersAndBuffers();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::puts("PASS: Whitecap DSP timing, modes, width, smoothing, reset and callback allocation");
    return 0;
}
