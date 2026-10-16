// SPDX-License-Identifier: GPL-3.0-only
#include "DistrhoPluginUtils.hpp"
#include "src/DistrhoPluginInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

static bool standalone = false;
static bool trackAllocations = false;
static std::size_t callbackAllocations = 0;

void *operator new(std::size_t size)
{
    if (trackAllocations)
        ++callbackAllocations;
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

START_NAMESPACE_DISTRHO
// Headless host harness, not a pretend Bitwig or VST3 validator. It exercises the
// actual processor through DPF's exporter, without loading UI or audio devices.
const char *getPluginFormatName() noexcept
{
    return standalone ? "JACK/Standalone" : "VST3";
}
END_NAMESPACE_DISTRHO

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Expected vst3 or standalone mode");
        require(std::strcmp(argv[1], "standalone") == 0 || std::strcmp(argv[1], "vst3") == 0,
                "Unknown harness mode");
        standalone = std::strcmp(argv[1], "standalone") == 0;
        using namespace DISTRHO;
        d_nextBufferSize = 512;
        d_nextSampleRate = 48000;
        PluginExporter processor(nullptr, nullptr, nullptr, nullptr);
        require(processor.getParameterCount() == (standalone ? 2u : 0u), "Unexpected parameters");
        processor.activate();

        for (const double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
            processor.setSampleRate(rate, true);
            for (const uint32_t frames : {0u, 1u, 2u, 63u, 64u, 127u, 512u, 2048u}) {
                processor.setBufferSize(std::max(1u, frames), true);
                const std::size_t size = frames + 1;
                std::vector<float> left(size), right(size), outLeft(size, 99), outRight(size, 99);
                for (std::size_t i = 0; i < size; ++i) {
                    left[i] = static_cast<float>(std::sin(i * 0.137) * 2.5);
                    right[i] = static_cast<float>(std::cos(i * 0.291) * -3.0);
                }
                const float *inputs[] = {left.data(), right.data()};
                float *outputs[] = {outLeft.data(), outRight.data()};
                if (standalone) {
                    // Initial activation and reconfiguration require monitor consent.
                    require(processor.getParameterValue(0) == 0, "Monitor should start muted");
                    processor.run(inputs, outputs, frames);
                    require(std::all_of(outLeft.begin(), outLeft.begin() + frames,
                                        [](float value) { return value == 0; }),
                            "Muted left leaked input");
                    require(std::all_of(outRight.begin(), outRight.begin() + frames,
                                        [](float value) { return value == 0; }),
                            "Muted right leaked input");
                    processor.setParameterValue(0, 1);
                }
                trackAllocations = true;
                processor.run(inputs, outputs, frames);
                trackAllocations = false;
                require(std::memcmp(left.data(), outLeft.data(), frames * sizeof(float)) == 0,
                        "Left audio changed");
                require(std::memcmp(right.data(), outRight.data(), frames * sizeof(float)) == 0,
                        "Right audio changed");
                require(outLeft[frames] == 99 && outRight[frames] == 99, "Wrote past block end");

                float *inPlace[] = {left.data(), right.data()};
                processor.run(inputs, inPlace, frames);
                require(std::memcmp(left.data(), outLeft.data(), frames * sizeof(float)) == 0,
                        "In-place left changed");
                require(std::memcmp(right.data(), outRight.data(), frames * sizeof(float)) == 0,
                        "In-place right changed");
                if (standalone) {
                    processor.setParameterValue(0, std::numeric_limits<float>::quiet_NaN());
                    require(processor.getParameterValue(0) == 0, "Invalid monitor value unmuted");
                    processor.setParameterValue(0, 1);
                    processor.deactivate();
                    processor.activate();
                    require(processor.getParameterValue(0) == 0, "Restart restored monitoring");
                    processor.run(inputs, inPlace, frames);
                    require(std::all_of(left.begin(), left.begin() + frames,
                                        [](float value) { return value == 0; }),
                            "In-place mute leaked audio");
                }
            }
        }
        require(callbackAllocations == 0, "Processor allocated in audio callback");
        processor.deactivate();
        std::printf("PASS: %s, five rates, eight block sizes, separate/in-place buffers, unchanged "
                    "over-range audio, allocation check\n",
                    argv[1]);
        return 0;
    } catch (const std::exception &error) {
        trackAllocations = false;
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
