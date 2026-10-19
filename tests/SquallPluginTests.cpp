// SPDX-License-Identifier: GPL-3.0-only
#include "SquallParameters.hpp"
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
    require(std::strcmp(plugin.getName(), "Squall") == 0, "Wrong plugin name");
    require(std::strcmp(plugin.getLabel(), "squall") == 0, "Wrong plugin label");
    require(plugin.getUniqueId() == d_cconst("UtS0"), "Wrong unique id");
    require(plugin.getVersion() == d_version(0, 1, 0), "Wrong plugin version");
    require(plugin.getParameterCount() == SquallPluginParameterCount, "Wrong parameter count");
    require(plugin.getStateKey(0) == "squall_schema", "Missing Squall schema marker");
    require(plugin.getParameterValue(SquallModeParameter) == 0.0f &&
                plugin.getParameterValue(SquallBypassParameter) == 0.0f,
            "Squall must start Clean and enabled");
    for (std::uint32_t i = 0; i < SquallPluginParameterCount; ++i) {
        require(plugin.isParameterInput(i), "Every Squall parameter must be an input");
        require(plugin.getParameterSymbol(i) == squallSpecs[i].symbol,
                "Parameter symbol disagrees with Squall schema");
        const auto range = plugin.getParameterRanges(i);
        require(range.min == 0.0f && range.max == 1.0f && range.def == 0.0f,
                "Squall parameter range/default is wrong");
    }
    require(plugin.getAudioPortCountWithGroupId(true, kPortGroupStereo) == 2 &&
                plugin.getAudioPortCountWithGroupId(false, kPortGroupStereo) == 2,
            "Squall must expose stereo ports for recorded-vocal tracks");
}

static void parameterSanitizing(PluginExporter &plugin)
{
    plugin.setParameterValue(SquallModeParameter, 0.6f);
    require(plugin.getParameterValue(SquallModeParameter) == 1.0f,
            "Heavy mode must be a discrete choice");
    plugin.setParameterValue(SquallModeParameter, 0.4f);
    require(plugin.getParameterValue(SquallModeParameter) == 0.0f,
            "Clean mode must be a discrete choice");
    plugin.setParameterValue(SquallBypassParameter, 0.7f);
    require(plugin.getParameterValue(SquallBypassParameter) == 1.0f, "Bypass must be boolean");
    plugin.setParameterValue(SquallBypassParameter, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(SquallBypassParameter) ==
                squallSpecs[SquallBypassParameter].initial,
            "Invalid parameter must fall back to its safe default");
}

static void audioAndBypass(PluginExporter &plugin)
{
    prepare(plugin, 48000.0);
    auto input = audio(2048);
    input[0][0] = input[1][0] = 0.5f;
    for (std::size_t i = 1; i < input[0].size(); ++i) {
        const float sample =
            static_cast<float>(0.4 * std::sin(2.0 * 3.14159265358979323846 * 180.0 *
                                              static_cast<double>(i) / 48000.0));
        input[0][i] = input[1][i] = sample;
    }
    auto output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    require(std::abs(output[0][0] - 0.44f) < 1.0e-6f,
            "Clean default should keep a mostly dry direct vocal");
    require(std::all_of(output[0].begin(), output[0].end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Clean plugin output must remain finite");
    require(allocations == 0, "Squall plugin processing allocated memory");
    const Audio cleanOutput = output;

    plugin.setParameterValue(SquallBypassParameter, 1.0f);
    prepare(plugin, 48000.0);
    output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()), true);
    require(output[0] == input[0] && output[1] == input[1],
            "Settled plugin bypass must return the original stereo signal");
    require(allocations == 0, "Bypass processing allocated memory");

    plugin.setParameterValue(SquallBypassParameter, 0.0f);
    plugin.setParameterValue(SquallModeParameter, 1.0f);
    prepare(plugin, 48000.0);
    output = audio(input[0].size());
    run(plugin, input, output, 0, static_cast<std::uint32_t>(input[0].size()));
    require(output[0] != cleanOutput[0], "Heavy mode parameter did not reach the DSP core");
    require(allocations == 0, "Heavy-mode processing allocated memory");
}

static void settingsPersist(PluginExporter &source, PluginExporter &reopened)
{
    source.setParameterValue(SquallModeParameter, 1.0f);
    source.setParameterValue(SquallBypassParameter, 1.0f);
    std::array<float, SquallPluginParameterCount> saved{};
    for (std::uint32_t i = 0; i < SquallPluginParameterCount; ++i) {
        saved[i] = source.getParameterValue(i);
        reopened.setParameterValue(i, saved[i]);
    }
    reopened.setState("squall_schema", "1");
    for (std::uint32_t i = 0; i < SquallPluginParameterCount; ++i)
        require(reopened.getParameterValue(i) == saved[i],
                "Squall settings did not survive a simulated reopen");
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
        std::fprintf(stderr, "Usage: squall_plugin_tests [vst3]\n");
        return 1;
    }
    std::puts("PASS: Squall VST3 identity, parameters, state and processing");
    return 0;
}
