// SPDX-License-Identifier: GPL-3.0-only
#include "Riptide.hpp"

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

using stormsurge::Riptide;
using stormsurge::RiptideParameters;
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

static Audio audio(std::size_t frames)
{
    return {std::vector<float>(frames), std::vector<float>(frames)};
}

static void process(Riptide &effect, const Audio &input, Audio &output, std::size_t offset,
                    std::uint32_t frames, std::uint32_t channels = 2, bool inPlace = false)
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
    effect.process(inputs, outputs, frames);
    trackAllocations = false;
    require(allocations == 0, "Processing allocated memory");
}

static void render(Riptide &effect, const Audio &input, Audio &output, std::uint32_t channels = 2)
{
    constexpr std::array<std::uint32_t, 4> blocks{137, 512, 73, 1024};
    std::size_t offset = 0;
    std::size_t blockIndex = 0;
    while (offset < input[0].size()) {
        const auto frames = static_cast<std::uint32_t>(
            std::min<std::size_t>(blocks[blockIndex++ % blocks.size()], input[0].size() - offset));
        process(effect, input, output, offset, frames, channels);
        offset += frames;
    }
}

static double amplitudeAt(const std::vector<float> &samples, std::size_t start, std::size_t frames,
                          double frequency, double sampleRate)
{
    double real = 0.0;
    double imaginary = 0.0;
    const double phaseStep = 2.0 * std::numbers::pi * frequency / sampleRate;
    for (std::size_t i = 0; i < frames; ++i) {
        const double phase = phaseStep * static_cast<double>(i);
        real += samples[start + i] * std::cos(phase);
        imaginary -= samples[start + i] * std::sin(phase);
    }
    return 2.0 * std::hypot(real, imaginary) / static_cast<double>(frames);
}

static void spectralHelperSelfCheck()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t frames = 48000;
    constexpr double frequency = 220.0;
    constexpr double amplitude = 0.37;
    std::vector<float> known(frames);
    for (std::size_t i = 0; i < frames; ++i)
        known[i] = static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * frequency *
                                                           static_cast<double>(i) / sampleRate));
    near(amplitudeAt(known, 0, frames, frequency, sampleRate), amplitude, 1.0e-5,
         "Spectral helper failed its known-amplitude self-check");
}

static Audio tones(std::size_t frames, double sampleRate, const std::array<double, 3> &frequencies)
{
    auto result = audio(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double time = static_cast<double>(i) / sampleRate;
        for (const double frequency : frequencies) {
            const float sample =
                static_cast<float>(0.2 * std::sin(2.0 * std::numbers::pi * frequency * time));
            result[0][i] += sample;
            result[1][i] += sample;
        }
    }
    return result;
}

static void passThroughAndBlend()
{
    Riptide effect;
    effect.prepare(48000.0);
    require(effect.channelCount() == 2, "Default Riptide layout must be stereo");
    require(effect.minimumReadDelaySamples() == 48 && effect.maximumReadDelaySamples() == 1968,
            "Riptide prototype read-delay range is wrong");

    auto input = audio(4097);
    for (std::size_t i = 0; i < input[0].size(); ++i) {
        input[0][i] = static_cast<float>(0.4 * std::sin(0.07 * static_cast<double>(i)));
        input[1][i] = static_cast<float>(0.3 * std::cos(0.11 * static_cast<double>(i)));
    }
    input[0][7] = std::numeric_limits<float>::quiet_NaN();
    input[1][11] = std::numeric_limits<float>::infinity();

    auto output = audio(input[0].size());
    render(effect, input, output);
    for (std::size_t i = 0; i < input[0].size(); ++i) {
        const float left = std::isfinite(input[0][i]) ? input[0][i] : 0.0f;
        const float right = std::isfinite(input[1][i]) ? input[1][i] : 0.0f;
        near(output[0][i], left, 0.0, "Zero shift must preserve the left input exactly");
        near(output[1][i], right, 0.0, "Zero shift must preserve the right input exactly");
    }

    RiptideParameters p;
    p.shiftSemitones = -12.0;
    p.blend = 0.0;
    effect.prepare(48000.0, 2, p);
    output = audio(input[0].size());
    render(effect, input, output);
    for (std::size_t i = 0; i < input[0].size(); ++i) {
        const float left = std::isfinite(input[0][i]) ? input[0][i] : 0.0f;
        const float right = std::isfinite(input[1][i]) ? input[1][i] : 0.0f;
        near(output[0][i], left, 0.0, "Zero blend must preserve the left input exactly");
        near(output[1][i], right, 0.0, "Zero blend must preserve the right input exactly");
    }

    auto inPlace = audio(256);
    for (std::size_t i = 0; i < inPlace[0].size(); ++i)
        inPlace[0][i] = inPlace[1][i] = static_cast<float>(std::sin(0.1 * i));
    process(effect, inPlace, inPlace, 0, 256, 2, true);
    require(std::all_of(inPlace[0].begin(), inPlace[0].end(),
                        [](float value) { return std::isfinite(value); }),
            "In-place processing emitted non-finite output");
}

static void downwardPitchAndPowerChord()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t frames = 96000;
    constexpr std::size_t analysisStart = 2000;
    constexpr std::size_t analysisFrames = 90000;
    spectralHelperSelfCheck();

    const std::array<double, 3> inputFrequencies{82.4069, 110.0, 146.8324};
    const auto input = tones(frames, sampleRate, inputFrequencies);
    RiptideParameters p;
    p.shiftSemitones = -12.0;
    p.blend = 1.0;
    Riptide effect;
    effect.prepare(sampleRate, 2, p);
    auto output = audio(frames);
    render(effect, input, output);

    for (const double frequency : inputFrequencies) {
        const double expected = frequency * 0.5;
        double shiftedAmplitude = 0.0;
        double shiftedPeakFrequency = expected;
        for (int bin = -100; bin <= 100; ++bin) {
            const double testFrequency = expected + bin * 0.25;
            const double candidate =
                amplitudeAt(output[0], analysisStart, analysisFrames, testFrequency, sampleRate);
            if (candidate > shiftedAmplitude) {
                shiftedAmplitude = candidate;
                shiftedPeakFrequency = testFrequency;
            }
        }
        const double originalAmplitude =
            amplitudeAt(output[0], analysisStart, analysisFrames, frequency, sampleRate);
        if (!(shiftedAmplitude > 0.10))
            std::fprintf(stderr, "frequency %.3f shifted peak %.3f amplitude %.6f original %.6f\n",
                         frequency, shiftedPeakFrequency, shiftedAmplitude, originalAmplitude);
        require(shiftedAmplitude > 0.10,
                "Octave-down power chord did not retain a measurable shifted component");
        require(shiftedAmplitude > originalAmplitude * 1.4,
                "Octave-down power chord retained too much unshifted pitch");
    }

    const double monoInputFrequency = 220.0;
    const auto singleNote =
        tones(frames, sampleRate,
              {monoInputFrequency, monoInputFrequency * 1.37, monoInputFrequency * 1.73});
    p.shiftSemitones = -5.0;
    p.blend = 1.0;
    effect.prepare(sampleRate, 1, p);
    output = audio(frames);
    render(effect, singleNote, output, 1);
    const double target = monoInputFrequency * std::pow(2.0, -5.0 / 12.0);
    double targetAmplitude = 0.0;
    for (int bin = -100; bin <= 100; ++bin)
        targetAmplitude =
            std::max(targetAmplitude, amplitudeAt(output[0], analysisStart, analysisFrames,
                                                  target + bin * 0.25, sampleRate));
    const double sourceAmplitude =
        amplitudeAt(output[0], analysisStart, analysisFrames, monoInputFrequency, sampleRate);
    require(targetAmplitude > 0.10 && targetAmplitude > sourceAmplitude * 1.4,
            "Five-semitone shift did not create the expected lower note");
}

static void parameterAndLifecycleHandling()
{
    Riptide effect;
    RiptideParameters p;
    p.shiftSemitones = -48.0;
    p.blend = 2.0;
    effect.prepare(44100.0, 1, p);
    require(effect.channelCount() == 1, "Mono preparation did not retain the mono layout");
    require(effect.minimumReadDelaySamples() == 44 && effect.maximumReadDelaySamples() == 1808,
            "Read-delay range did not scale with sample rate");

    p.shiftSemitones = std::numeric_limits<double>::quiet_NaN();
    p.blend = std::numeric_limits<double>::infinity();
    effect.setParameters(p);
    auto input = audio(1024);
    std::fill(input[0].begin(), input[0].end(), 0.25f);
    auto output = audio(input[0].size());
    render(effect, input, output, 1);
    require(std::all_of(output[0].begin(), output[0].end(),
                        [](float value) { return std::isfinite(value); }),
            "Invalid parameter updates contaminated processing");

    effect.reset();
    std::fill(input[0].begin(), input[0].end(), 0.0f);
    output = audio(input[0].size());
    render(effect, input, output, 1);
    require(
        std::all_of(output[0].begin(), output[0].end(), [](float value) { return value == 0.0f; }),
        "Reset did not clear the pitch-shifter history");

    for (const double sampleRate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        effect.prepare(sampleRate, 1);
        require(effect.maximumReadDelaySamples() ==
                    static_cast<std::uint32_t>(std::lround(sampleRate * 0.041)),
                "Maximum read delay is inconsistent across sample rates");
    }

    bool rejectedRate = false;
    try {
        effect.prepare(7000.0);
    } catch (const std::invalid_argument &) {
        rejectedRate = true;
    }
    require(rejectedRate, "Unsupported sample rate was not rejected");

    bool rejectedChannels = false;
    try {
        effect.prepare(48000.0, 3);
    } catch (const std::invalid_argument &) {
        rejectedChannels = true;
    }
    require(rejectedChannels, "Unsupported channel count was not rejected");
}

int main()
{
    try {
        passThroughAndBlend();
        downwardPitchAndPowerChord();
        parameterAndLifecycleHandling();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::puts("PASS: Riptide DSP prototype pitch, blend, lifecycle and callback checks");
    return 0;
}
