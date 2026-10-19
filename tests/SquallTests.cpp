// SPDX-License-Identifier: GPL-3.0-only
#include "Squall.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <numbers>
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

using stormsurge::Squall;
using stormsurge::SquallParameters;

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

using Audio = std::array<std::vector<float>, 2>;

static Audio audio(std::size_t frames)
{
    return {std::vector<float>(frames), std::vector<float>(frames)};
}

static void process(Squall &effect, const Audio &input, Audio &output, std::size_t offset,
                    std::uint32_t frames, std::uint32_t channels, bool inPlace = false)
{
    const float *inputs[] = {input[0].data() + offset, input[1].data() + offset};
    float *outputs[] = {output[0].data() + offset, output[1].data() + offset};
    if (inPlace) {
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            std::copy_n(inputs[channel], frames, outputs[channel]);
            inputs[channel] = outputs[channel];
        }
    }
    allocations = 0;
    trackAllocations = true;
    effect.process(inputs, outputs, frames, channels);
    trackAllocations = false;
    require(allocations == 0, "Processing allocated memory");
}

static void cleanReverbAndReset()
{
    constexpr double rate = 48000.0;
    Squall effect;
    effect.prepare(rate);
    auto input = audio(24000);
    input[0][0] = 1.0f;
    input[1][0] = 1.0f;
    auto output = audio(input[0].size());
    process(effect, input, output, 0, static_cast<std::uint32_t>(input[0].size()), 2);
    require(output[0][0] > 0.85f && output[0][0] < 1.0f,
            "Clean mode should preserve a mostly dry direct voice");
    require(std::any_of(output[0].begin() + 100, output[0].end(),
                        [](float sample) { return std::abs(sample) > 1.0e-5f; }),
            "Clean mode should add a short reverb tail");
    require(std::all_of(output[0].begin(), output[0].end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Reverb output must remain finite");

    effect.reset();
    auto silence = audio(256);
    auto resetOutput = audio(256);
    process(effect, silence, resetOutput, 0, 256, 2);
    require(std::all_of(resetOutput[0].begin(), resetOutput[0].end(),
                        [](float sample) { return sample == 0.0f; }),
            "Reset must clear the reverb tail");
}

static void heavyCharacterAndStereo()
{
    constexpr double rate = 48000.0;
    constexpr std::size_t frames = 24000;
    auto input = audio(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float sample = static_cast<float>(
            0.7 * std::sin(2.0 * std::numbers::pi * 220.0 * static_cast<double>(i) / rate));
        input[0][i] = sample;
        input[1][i] = sample;
    }

    Squall clean;
    clean.prepare(rate);
    auto cleanOutput = audio(frames);
    process(clean, input, cleanOutput, 0, static_cast<std::uint32_t>(frames), 2);

    Squall heavy;
    SquallParameters parameters;
    parameters.heavy = true;
    heavy.prepare(rate, parameters);
    auto heavyOutput = audio(frames);
    process(heavy, input, heavyOutput, 0, static_cast<std::uint32_t>(frames), 2);
    require(cleanOutput[0] != heavyOutput[0], "Heavy mode must color an existing growl");
    require(heavyOutput[0] != heavyOutput[1],
            "Stereo reverb should decorrelate a centered vocal slightly");
    require(std::all_of(heavyOutput[0].begin(), heavyOutput[0].end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Heavy processing must remain finite");

    Squall mono;
    mono.prepare(rate, parameters);
    auto monoInput = audio(4096);
    monoInput[0][0] = 0.5f;
    auto monoOutput = audio(4096);
    process(mono, monoInput, monoOutput, 0, 4096, 1);
    require(std::any_of(monoOutput[0].begin(), monoOutput[0].end(),
                        [](float sample) { return std::abs(sample) > 1.0e-5f; }),
            "Mono processing must preserve the voice");
}

static void smoothedModeAndBypass()
{
    constexpr double rate = 48000.0;
    constexpr std::size_t frames = 8192;
    Squall effect;
    effect.prepare(rate);
    auto input = audio(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float sample = static_cast<float>(
            0.5 * std::sin(2.0 * std::numbers::pi * 180.0 * static_cast<double>(i) / rate));
        input[0][i] = input[1][i] = sample;
    }
    auto output = audio(frames);
    process(effect, input, output, 0, 2048, 2);
    SquallParameters parameters;
    parameters.heavy = true;
    effect.setParameters(parameters);
    process(effect, input, output, 2048, 2048, 2);

    double largestStep = 0.0;
    for (std::size_t i = 1; i < 4096; ++i)
        largestStep =
            std::max(largestStep, std::abs(static_cast<double>(output[0][i] - output[0][i - 1])));
    require(largestStep < 0.15, "Mode change introduced an abrupt output step");

    parameters.bypass = true;
    effect.setParameters(parameters);
    process(effect, input, output, 4096, 2048, 2);
    require(output[0][4096 + 1100] == input[0][4096 + 1100] &&
                output[1][4096 + 1100] == input[1][4096 + 1100],
            "Settled bypass must return the original signal exactly");

    parameters.bypass = false;
    effect.setParameters(parameters);
    process(effect, input, output, 6144, 2048, 2, true);
    require(std::all_of(output[0].begin() + 6144, output[0].end(),
                        [](float sample) { return std::isfinite(sample); }),
            "In-place mode changes must remain finite");
}

static void invalidSamplesAndRates()
{
    Squall effect;
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        effect.prepare(rate);
        auto input = audio(512);
        input[0][0] = input[1][0] = std::numeric_limits<float>::quiet_NaN();
        auto output = audio(512);
        process(effect, input, output, 0, 512, 2);
        require(std::all_of(output[0].begin(), output[0].end(),
                            [](float sample) { return std::isfinite(sample); }),
                "Invalid input sample must not produce nonfinite output");
    }

    bool rejected = false;
    try {
        effect.prepare(384000.0);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected, "Unsupported sample rates must be rejected during prepare");
}

int main()
{
    try {
        cleanReverbAndReset();
        heavyCharacterAndStereo();
        smoothedModeAndBypass();
        invalidSamplesAndRates();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::puts("PASS: Squall DSP core reverb, heavy mode, bypass and lifecycle");
    return 0;
}
