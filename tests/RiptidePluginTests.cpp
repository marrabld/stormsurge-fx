// SPDX-License-Identifier: GPL-3.0-only
#include "RiptideParameters.hpp"
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
    require(std::strcmp(plugin.getName(), "Riptide") == 0, "Wrong plugin name");
    require(std::strcmp(plugin.getLabel(), "riptide") == 0, "Wrong plugin label");
    require(plugin.getUniqueId() == d_cconst("UtR0"), "Wrong unique id");
    require(plugin.getVersion() == d_version(0, 1, 0), "Wrong plugin version");
    require(plugin.getParameterCount() == RiptidePluginParameterCount, "Wrong parameter count");
    require(plugin.getStateKey(0) == "riptide_schema", "Missing Riptide schema marker");
    require(plugin.getParameterValue(RiptideShiftParameter) == 0.0f,
            "Riptide must start at original pitch");
    require(plugin.getParameterValue(RiptideBlendParameter) == 100.0f,
            "Riptide must start fully blended");
    require(plugin.getParameterValue(RiptideBypassParameter) == 0.0f, "Riptide must start enabled");
    for (std::uint32_t i = 0; i < RiptidePluginParameterCount; ++i) {
        require(plugin.isParameterInput(i), "Every Riptide parameter must be an input");
        require(plugin.getParameterSymbol(i) == riptideSpecs[i].symbol,
                "Parameter symbol disagrees with Riptide schema");
        const auto range = plugin.getParameterRanges(i);
        require(range.min == riptideSpecs[i].minimum && range.max == riptideSpecs[i].maximum &&
                    range.def == riptideSpecs[i].initial,
                "Riptide parameter range/default is wrong");
    }
    require(plugin.getAudioPortCountWithGroupId(true, kPortGroupStereo) == 2 &&
                plugin.getAudioPortCountWithGroupId(false, kPortGroupStereo) == 2,
            "Riptide must expose stereo ports");
}

static void parameterSanitizing(PluginExporter &plugin)
{
    plugin.setParameterValue(RiptideShiftParameter, -5.4f);
    require(plugin.getParameterValue(RiptideShiftParameter) == -5.0f,
            "Shift must settle on a whole downward semitone");
    plugin.setParameterValue(RiptideShiftParameter, -5.6f);
    require(plugin.getParameterValue(RiptideShiftParameter) == -6.0f,
            "Negative shift must round away from zero");
    plugin.setParameterValue(RiptideShiftParameter, -99.0f);
    require(plugin.getParameterValue(RiptideShiftParameter) == -12.0f,
            "Shift must stay inside the prototype range");
    plugin.setParameterValue(RiptideShiftParameter, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(RiptideShiftParameter) ==
                riptideSpecs[RiptideShiftParameter].initial,
            "Invalid shift must fall back to original pitch");
    plugin.setParameterValue(RiptideBlendParameter, 150.0f);
    require(plugin.getParameterValue(RiptideBlendParameter) == 100.0f,
            "Blend must clamp to fully shifted");
    plugin.setParameterValue(RiptideBlendParameter, -1.0f);
    require(plugin.getParameterValue(RiptideBlendParameter) == 0.0f, "Blend must clamp to dry");
    plugin.setParameterValue(RiptideBlendParameter, std::numeric_limits<float>::infinity());
    require(plugin.getParameterValue(RiptideBlendParameter) ==
                riptideSpecs[RiptideBlendParameter].initial,
            "Invalid blend must fall back to its safe default");
    plugin.setParameterValue(RiptideBypassParameter, 0.7f);
    require(plugin.getParameterValue(RiptideBypassParameter) == 1.0f, "Bypass must be boolean");
    plugin.setParameterValue(RiptideBypassParameter, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(RiptideBypassParameter) ==
                riptideSpecs[RiptideBypassParameter].initial,
            "Invalid bypass must fail to enabled, not bypassed");
}

static Audio tone(std::size_t frames, double frequency = 180.0)
{
    auto buffer = audio(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float sample =
            static_cast<float>(0.4 * std::sin(2.0 * 3.14159265358979323846 * frequency *
                                              static_cast<double>(i) / 48000.0));
        buffer[0][i] = buffer[1][i] = sample;
    }
    return buffer;
}

static void audioAndBypass(PluginExporter &plugin)
{
    prepare(plugin, 48000.0);
    const auto input = tone(4096);
    auto output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    require(output[0] == input[0] && output[1] == input[1],
            "Default Riptide state must be an exact unbounded identity");
    require(allocations == 0, "Riptide plugin processing allocated memory");

    plugin.setParameterValue(RiptideShiftParameter, -12.0f);
    prepare(plugin, 48000.0);
    output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    require(output[0] != input[0], "Shift parameter did not reach the DSP core");
    require(std::all_of(output[0].begin(), output[0].end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Shifted plugin output must remain finite");
    require(allocations == 0, "Shifted processing allocated memory");
    const Audio shiftedOutput = output;

    plugin.setParameterValue(RiptideBlendParameter, 0.0f);
    prepare(plugin, 48000.0);
    output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    require(output[0] == input[0] && output[1] == input[1],
            "Blend zero must return the original signal exactly");

    plugin.setParameterValue(RiptideBlendParameter, 100.0f);
    plugin.setParameterValue(RiptideBypassParameter, 1.0f);
    prepare(plugin, 48000.0);
    output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()), true);
    require(output[0] == input[0] && output[1] == input[1],
            "Settled plugin bypass must return the original stereo signal");
    require(allocations == 0, "Bypass processing allocated memory");

    // Toggling bypass on a running instance must fade, not switch. The tail of a
    // 20 ms ramp is dry even though the instance was never re-prepared.
    plugin.setParameterValue(RiptideBypassParameter, 0.0f);
    prepare(plugin, 48000.0);
    output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    require(output[0] != input[0], "Riptide did not shift before the live bypass test");
    plugin.setParameterValue(RiptideBypassParameter, 1.0f);
    auto fading = audio(input[0].size());
    run(plugin, input, fading, 0, static_cast<std::uint32_t>(input[0].size()));
    require(std::abs(fading[0].back() - input[0].back()) < 1.0e-6f,
            "Live bypass must settle on the dry signal");
    require(std::abs(shiftedOutput[0].back() - input[0].back()) > 1.0e-3f,
            "The shifted signal was not measurably different before bypass");
}

static void settingsPersist(PluginExporter &source, PluginExporter &reopened)
{
    source.setParameterValue(RiptideShiftParameter, -7.0f);
    source.setParameterValue(RiptideBlendParameter, 40.0f);
    source.setParameterValue(RiptideBypassParameter, 1.0f);
    std::array<float, RiptidePluginParameterCount> saved{};
    for (std::uint32_t i = 0; i < RiptidePluginParameterCount; ++i) {
        saved[i] = source.getParameterValue(i);
        reopened.setParameterValue(i, saved[i]);
    }
    reopened.setState("riptide_schema", "1");
    for (std::uint32_t i = 0; i < RiptidePluginParameterCount; ++i)
        require(reopened.getParameterValue(i) == saved[i],
                "Riptide settings did not survive a simulated reopen");
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
        std::fprintf(stderr, "Usage: riptide_plugin_tests [vst3]\n");
        return 1;
    }
    std::puts("PASS: Riptide VST3 identity, parameters, state and processing");
    return 0;
}
