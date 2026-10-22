// SPDX-License-Identifier: GPL-3.0-only
#include "AuditionRender.hpp"
#include "WavFile.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using stormsurge::AuditionSettings;
using stormsurge::wav::Audio;
using stormsurge::wav::Format;

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void near(double actual, double expected, double tolerance, const char *message)
{
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

struct TempFile {
    std::filesystem::path path;
    TempFile()
    {
        static std::mt19937_64 generator(std::random_device{}());
        path = std::filesystem::temp_directory_path() /
               ("stormsurge-audition-" + std::to_string(generator()) + ".wav");
    }
    ~TempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

Audio sine(std::uint32_t sampleRate, std::uint16_t channels, std::size_t frames, double frequency,
           double amplitude)
{
    Audio audio;
    audio.sampleRate = sampleRate;
    audio.channels = channels;
    audio.samples.resize(frames * channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto value =
            static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * frequency *
                                                    static_cast<double>(frame) / sampleRate));
        for (std::uint16_t channel = 0; channel < channels; ++channel)
            audio.samples[frame * channels + channel] = value;
    }
    return audio;
}

// Magnitude of one Goertzel bin. The first check in the test compares a known
// sine's bin against its expected amplitude, because a helper that quietly reads
// the wrong samples would otherwise still produce plausible ratios.
double goertzel(const std::vector<float> &samples, double frequency, double sampleRate)
{
    const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const double coefficient = 2.0 * std::cos(omega);
    double previous = 0.0;
    double older = 0.0;
    for (float sample : samples) {
        const double current = sample + coefficient * previous - older;
        older = previous;
        previous = current;
    }
    return std::sqrt(previous * previous + older * older - coefficient * previous * older);
}

void testFloatRoundTrip()
{
    TempFile file;
    Audio audio;
    audio.sampleRate = 48000;
    audio.channels = 2;
    audio.samples = {0.0f, 0.25f, -0.5f, 1.5f, -2.0f, 0.0f};
    stormsurge::wav::write(file.path.string(), audio, Format::Float32);

    const Audio read = stormsurge::wav::read(file.path.string());
    require(read.sampleRate == audio.sampleRate && read.channels == audio.channels,
            "Float WAV kept the wrong rate or channel count");
    require(read.samples == audio.samples,
            "Float WAV did not round-trip samples above and below full scale exactly");
}

void testPcmRoundTrip()
{
    Audio audio = sine(48000, 2, 96, 440.0, 0.4);
    for (Format format : {Format::Pcm16, Format::Pcm24, Format::Pcm32}) {
        TempFile file;
        stormsurge::wav::write(file.path.string(), audio, format);
        const Audio read = stormsurge::wav::read(file.path.string());
        require(read.samples.size() == audio.samples.size(), "PCM WAV changed the frame count");
        const double tolerance = format == Format::Pcm16   ? 1.0 / 32000.0
                                 : format == Format::Pcm24 ? 1.0 / 8000000.0
                                                           : 1.0 / 2000000000.0;
        for (std::size_t index = 0; index < audio.samples.size(); ++index)
            near(read.samples[index], audio.samples[index], tolerance,
                 "PCM WAV round-trip lost more precision than the format allows");
    }
}

void testMalformedInput()
{
    TempFile file;
    {
        std::FILE *stream = std::fopen(file.path.string().c_str(), "wb");
        require(stream != nullptr, "Could not create a malformed test file");
        const char junk[] = "not a wave file";
        std::fwrite(junk, 1, sizeof(junk), stream);
        std::fclose(stream);
    }
    bool rejected = false;
    try {
        stormsurge::wav::read(file.path.string());
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    require(rejected, "A non-WAV file was accepted");
}

void testRenderIdentity()
{
    const Audio input = sine(48000, 2, 4800, 220.0, 0.5);
    for (AuditionSettings settings : {AuditionSettings{0.0, 0.0}, AuditionSettings{0.0, 1.0}}) {
        const Audio output = stormsurge::renderAudition(input, settings);
        require(output.samples.size() == input.samples.size(),
                "Identity render changed the length");
        require(output.samples == input.samples,
                "Zero shift with either blend extreme did not reproduce the dry input exactly");
    }
}

void testShiftedFrequency()
{
    constexpr double sampleRate = 48000.0;
    constexpr double inputFrequency = 220.0;
    const Audio input = sine(static_cast<std::uint32_t>(sampleRate), 1, 48000, inputFrequency, 0.5);
    const Audio output = stormsurge::renderAudition(input, {-5.0, 1.0});
    require(output.frames() == input.frames(), "Shifted render changed the frame count");

    const double expected = inputFrequency * std::pow(2.0, -5.0 / 12.0);
    const double selfCheck = goertzel(input.samples, inputFrequency, sampleRate);
    near(selfCheck, 0.5 * 48000.0 / 2.0, 0.02 * 0.5 * 48000.0 / 2.0,
         "Spectral helper failed its own known-amplitude check");

    const double shifted = goertzel(output.samples, expected, sampleRate);
    const double below = goertzel(output.samples, expected - 30.0, sampleRate);
    const double above = goertzel(output.samples, expected + 30.0, sampleRate);
    require(shifted > 4.0 * std::max(below, above),
            "Shifted output did not concentrate energy near the expected -5 semitone pitch");
    require(goertzel(output.samples, inputFrequency, sampleRate) < shifted * 0.5,
            "Fully blended output still contained most of the unshifted fundamental");
}

void testRejectsUnsupportedAudio()
{
    Audio threeChannels = sine(48000, 2, 10, 220.0, 0.5);
    threeChannels.channels = 3;
    threeChannels.samples.resize(30);
    bool rejected = false;
    try {
        stormsurge::renderAudition(threeChannels, {0.0, 1.0});
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected, "Three-channel audio was accepted");

    const Audio input = sine(48000, 1, 10, 220.0, 0.5);
    for (AuditionSettings settings :
         {AuditionSettings{1.0, 1.0}, AuditionSettings{-5.0, 1.5}, AuditionSettings{-5.0, -0.1}}) {
        rejected = false;
        try {
            stormsurge::renderAudition(input, settings);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        require(rejected, "An out-of-range shift or blend was accepted");
    }
}

} // namespace

int main()
{
    try {
        testFloatRoundTrip();
        testPcmRoundTrip();
        testMalformedInput();
        testRenderIdentity();
        testShiftedFrequency();
        testRejectsUnsupportedAudio();
        std::puts("Riptide audition checks passed");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
