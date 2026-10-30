// SPDX-License-Identifier: GPL-3.0-only
#include "FloodGateParameters.hpp"
#include "DistrhoPluginUtils.hpp"
#include "src/DistrhoPluginInternal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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

START_NAMESPACE_DISTRHO
const char *getPluginFormatName() noexcept
{
    return "VST3";
}
END_NAMESPACE_DISTRHO

using namespace DISTRHO;
using namespace stormsurge;
using Audio = std::array<std::vector<float>, 2>;

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static Audio audio(std::size_t frames, float value = 0.0f)
{
    return {std::vector<float>(frames, value), std::vector<float>(frames, value)};
}

static void run(PluginExporter &plugin, const Audio &input, Audio &output, std::size_t offset,
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
    plugin.run(inputs, outputs, frames);
    trackAllocations = false;
}

static void prepare(PluginExporter &plugin, double rate)
{
    plugin.deactivateIfNeeded();
    plugin.setSampleRate(rate, true);
    plugin.activate();
}

static void identityAndDefaults(PluginExporter &plugin)
{
    require(std::strcmp(plugin.getName(), "FloodGate") == 0, "Wrong plugin name");
    require(std::strcmp(plugin.getLabel(), "floodgate") == 0, "Wrong plugin label");
    require(plugin.getUniqueId() == d_cconst("UtE0"), "Wrong unique id");
    require(plugin.getVersion() == d_version(0, 1, 0), "Wrong plugin version");
    require(plugin.getParameterCount() == FloodGateParameterCount, "Wrong parameter count");
    require(plugin.getStateKey(0) == "floodgate_schema", "Missing FloodGate schema marker");
    require(plugin.getParameterValue(FloodGateBypass) == 0.0f, "FloodGate must start enabled");
    for (std::uint32_t i = 0; i < FloodGateParameterCount; ++i) {
        require(plugin.isParameterInput(i), "Every FloodGate parameter must be an input");
        require(plugin.getParameterSymbol(i) == floodGateSpecs[i].symbol,
                "Parameter symbol disagrees with FloodGate schema");
        const auto range = plugin.getParameterRanges(i);
        require(range.min == floodGateSpecs[i].minimum && range.max == floodGateSpecs[i].maximum &&
                    range.def == floodGateSpecs[i].initial,
                "FloodGate parameter range/default is wrong");
        require(plugin.getParameterValue(i) == floodGateSpecs[i].initial,
                "FloodGate must start at its schema defaults");
    }
    require(plugin.getAudioPortCountWithGroupId(true, kPortGroupStereo) == 2 &&
                plugin.getAudioPortCountWithGroupId(false, kPortGroupStereo) == 2,
            "FloodGate must expose stereo ports");
}

static void parameterSanitizing(PluginExporter &plugin)
{
    plugin.setParameterValue(FloodGateThreshold, 10.0f);
    require(plugin.getParameterValue(FloodGateThreshold) == -20.0f,
            "Threshold must clamp to its maximum");
    plugin.setParameterValue(FloodGateRelease, 1.0f);
    require(plugin.getParameterValue(FloodGateRelease) == 10.0f,
            "Release must clamp to its minimum");
    plugin.setParameterValue(FloodGateRange, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(FloodGateRange) == floodGateSpecs[FloodGateRange].initial,
            "Invalid range must fall back to its default");
    plugin.setParameterValue(FloodGateBypass, 0.7f);
    require(plugin.getParameterValue(FloodGateBypass) == 1.0f, "Bypass must be boolean");
    plugin.setParameterValue(FloodGateBypass, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(FloodGateBypass) == 0.0f,
            "Invalid bypass must fail to enabled, not bypassed");
    for (std::uint32_t i = 0; i < FloodGateParameterCount; ++i)
        plugin.setParameterValue(i, floodGateSpecs[i].initial);
}

static float peak(const std::vector<float> &samples, std::size_t from)
{
    float result = 0.0f;
    for (std::size_t i = from; i < samples.size(); ++i)
        result = std::max(result, std::abs(samples[i]));
    return result;
}

static Audio tone(std::size_t frames, float level)
{
    auto buffer = audio(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float sample = static_cast<float>(
            level * std::sin(2.0 * 3.14159265358979323846 * 220.0 * static_cast<double>(i) /
                             48000.0));
        buffer[0][i] = buffer[1][i] = sample;
    }
    return buffer;
}

static void audioAndBypass(PluginExporter &plugin)
{
    constexpr std::uint32_t frames = 48000;
    // Release is a time constant, not a deadline, so measure the last 100 ms of a second.
    constexpr std::size_t settled = frames - 4800;
    prepare(plugin, 48000.0);
    const auto loud = tone(frames, 0.3f);
    auto output = audio(frames);
    run(plugin, loud, output, 0, frames);
    require(std::abs(peak(output[0], frames / 2) - peak(loud[0], frames / 2)) < 1.0e-5f,
            "A loud signal must pass the gate at unity");
    require(output[0] == output[1], "Identical stereo inputs must stay identical");
    require(allocations == 0, "FloodGate plugin processing allocated memory");

    // About -70 dBFS, under the -60 dB default threshold: the gate must close to its Range floor.
    const auto hiss = tone(frames, 0.0003f);
    prepare(plugin, 48000.0);
    output = audio(frames);
    run(plugin, hiss, output, 0, frames, true);
    require(peak(output[0], settled) < 0.0003f * 1.0e-3f, "Quiet input was not gated");

    // Bypassing a running, closed gate must fade to exact dry rather than jump open.
    plugin.setParameterValue(FloodGateBypass, 1.0f);
    output = audio(frames);
    run(plugin, hiss, output, 0, 64);
    require(peak(output[0], 0) < 0.0003f * 0.5f, "Bypass must ramp, not cut the gate open");
    run(plugin, hiss, output, 64, frames - 64);
    require(std::equal(output[0].begin() + frames / 2, output[0].end(),
                       hiss[0].begin() + frames / 2) &&
                std::equal(output[1].begin() + frames / 2, output[1].end(),
                           hiss[1].begin() + frames / 2),
            "Settled bypass must return the original stereo signal");
    require(allocations == 0, "Bypass processing allocated memory");
    plugin.setParameterValue(FloodGateBypass, 0.0f);
}

static void settingsPersist(PluginExporter &source, PluginExporter &reopened)
{
    source.setParameterValue(FloodGateThreshold, -42.0f);
    source.setParameterValue(FloodGateRelease, 230.0f);
    source.setParameterValue(FloodGateRange, -30.0f);
    source.setParameterValue(FloodGateBypass, 1.0f);
    std::array<float, FloodGateParameterCount> saved{};
    for (std::uint32_t i = 0; i < FloodGateParameterCount; ++i) {
        saved[i] = source.getParameterValue(i);
        reopened.setParameterValue(i, saved[i]);
    }
    reopened.setState("floodgate_schema", "1");
    for (std::uint32_t i = 0; i < FloodGateParameterCount; ++i)
        require(reopened.getParameterValue(i) == saved[i],
                "FloodGate settings did not survive a simulated reopen");
}

static void unsupportedRateFailsSilent(PluginExporter &plugin)
{
    prepare(plugin, 384000.0);
    auto input = audio(128, 0.25f);
    auto output = audio(128, 9.0f);
    run(plugin, input, output, 0, 128);
    require(std::all_of(output[0].begin(), output[0].end(),
                        [](float sample) { return sample == 0.0f; }),
            "Unsupported sample rate must fail silent");
}

int main(int argc, char **argv)
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000;
    try {
        PluginExporter plugin(nullptr, nullptr, nullptr, nullptr);
        PluginExporter reopened(nullptr, nullptr, nullptr, nullptr);
        identityAndDefaults(plugin);
        parameterSanitizing(plugin);
        audioAndBypass(plugin);
        settingsPersist(plugin, reopened);
        unsupportedRateFailsSilent(plugin);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    if (argc > 1 && std::strcmp(argv[1], "vst3") != 0) {
        std::fprintf(stderr, "Usage: floodgate_plugin_tests [vst3]\n");
        return 1;
    }
    std::puts("PASS: FloodGate VST3 identity, parameters, state and processing");
    return 0;
}
