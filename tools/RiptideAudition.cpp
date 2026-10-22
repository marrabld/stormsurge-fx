// SPDX-License-Identifier: GPL-3.0-only
#include "AuditionRender.hpp"
#include "WavFile.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

constexpr std::string_view Usage =
    "Usage: riptide_audition <input.wav> <output.wav> [--shift SEMITONES] [--blend PERCENT]\n"
    "\n"
    "Renders the experimental Riptide pitch-down core over a local mono or stereo WAV.\n"
    "Reads PCM 16/24/32-bit or float32 and writes float32. Nothing is uploaded.\n"
    "\n"
    "  --shift   Pitch shift from 0 to -12 semitones (default 0)\n"
    "  --blend   Wet/dry mix from 0 to 100 percent (default 100)\n"
    "\n"
    "Examples:\n"
    "  riptide_audition take.wav take-drop5.wav --shift -5\n"
    "  riptide_audition take.wav take-octave.wav --shift -12 --blend 30\n"
    "\n"
    "A successful render is not evidence of musical quality or live playing feel.\n";

double parseNumber(std::string_view text, const char *name)
{
    double value = 0.0;
    const char *begin = text.data();
    const char *end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value))
        throw std::invalid_argument(std::string("Could not read a finite number for ") + name +
                                    ": '" + std::string(text) + "'");
    return value;
}

} // namespace

int main(int argc, char **argv)
{
    try {
        std::string inputPath;
        std::string outputPath;
        stormsurge::AuditionSettings settings;

        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--help" || argument == "-h") {
                std::fputs(Usage.data(), stdout);
                return 0;
            }
            if (argument == "--shift" || argument == "--blend") {
                if (index + 1 >= argc)
                    throw std::invalid_argument(argument + " needs a value");
                const double value = parseNumber(argv[++index], argument.c_str());
                if (argument == "--shift")
                    settings.shiftSemitones = value;
                else
                    settings.blend = value / 100.0;
                continue;
            }
            if (!argument.empty() && argument[0] == '-')
                throw std::invalid_argument("Unknown option '" + argument + "'");
            if (inputPath.empty())
                inputPath = argument;
            else if (outputPath.empty())
                outputPath = argument;
            else
                throw std::invalid_argument("Unexpected extra argument '" + argument + "'");
        }

        if (inputPath.empty() || outputPath.empty())
            throw std::invalid_argument("Both an input and an output WAV path are required");

        std::error_code error;
        const auto inputAbsolute = std::filesystem::absolute(inputPath, error);
        const auto outputAbsolute = std::filesystem::absolute(outputPath, error);
        if (!error && inputAbsolute == outputAbsolute)
            throw std::invalid_argument("Input and output paths must differ");

        const stormsurge::wav::Audio input = stormsurge::wav::read(inputPath);
        if (input.samples.empty())
            throw std::invalid_argument("Input WAV contains no audio frames");

        const stormsurge::wav::Audio output = stormsurge::renderAudition(input, settings);
        stormsurge::wav::write(outputPath, output, stormsurge::wav::Format::Float32);

        const double seconds =
            static_cast<double>(output.frames()) / static_cast<double>(output.sampleRate);
        std::printf("Rendered %.2f s, %u Hz, %u channel%s, shift %.1f st, blend %.0f%% -> %s\n",
                    seconds, output.sampleRate, output.channels, output.channels == 1 ? "" : "s",
                    settings.shiftSemitones, settings.blend * 100.0, outputPath.c_str());
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "riptide_audition: %s\n", error.what());
        return 1;
    }
}
