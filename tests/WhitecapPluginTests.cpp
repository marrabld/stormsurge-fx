// SPDX-License-Identifier: GPL-3.0-only
#include "WhitecapParameters.hpp"
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

static Audio audio(std::size_t size, float value = 0)
{
    return {std::vector<float>(size, value), std::vector<float>(size, value)};
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
    require(std::strcmp(plugin.getName(), "Whitecap") == 0, "Wrong plugin name");
    require(std::strcmp(plugin.getLabel(), "whitecap") == 0, "Wrong plugin label");
    require(plugin.getUniqueId() == d_cconst("UtW0"), "Wrong unique id");
    require(plugin.getVersion() == d_version(0, 1, 0), "Wrong plugin version");
    require(plugin.getParameterCount() == WhitecapParameterCount, "Wrong parameter count");
    require(plugin.getStateKey(0) == "whitecap_schema", "Missing Whitecap schema marker");
    for (std::uint32_t i = 0; i < WhitecapParameterCount; ++i) {
        require(plugin.isParameterInput(i), "Every Whitecap parameter must be an input");
        require(plugin.getParameterValue(i) == whitecapSpecs[i].initial,
                "Parameter does not start at its documented default");
        require(plugin.getParameterSymbol(i) == whitecapSpecs[i].symbol,
                "Parameter symbol disagrees with its schema");
    }
    require(plugin.getAudioPortCountWithGroupId(true, kPortGroupStereo) == 2 &&
                plugin.getAudioPortCountWithGroupId(false, kPortGroupStereo) == 2,
            "Whitecap must present a stereo pair");
}

static void rangesAndSanitizing(PluginExporter &plugin)
{
    for (std::uint32_t i = 0; i < WhitecapParameterCount; ++i) {
        const auto &spec = whitecapSpecs[i];
        require(denormalizedWhitecap(i, 0.0f) == spec.minimum, "Knob minimum wrong");
        require(std::abs(denormalizedWhitecap(i, 1.0f) - spec.maximum) < 0.002f,
                "Knob maximum wrong");
        require(std::abs(denormalizedWhitecap(i, normalizedWhitecap(i, spec.initial)) -
                         spec.initial) < 0.002f,
                "Knob normalization round trip wrong");
        const auto ranges = plugin.getParameterRanges(i);
        require(ranges.min == spec.minimum && ranges.max == spec.maximum &&
                    ranges.def == spec.initial,
                "Host ranges disagree with the parameter spec");
    }
    plugin.setParameterValue(WhitecapRate, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(WhitecapRate) == whitecapSpecs[WhitecapRate].initial,
            "Nonfinite controls must fall back to defaults");
    plugin.setParameterValue(WhitecapDepth, 10.0f);
    require(plugin.getParameterValue(WhitecapDepth) == whitecapSpecs[WhitecapDepth].maximum,
            "Out-of-range values must clamp");
    plugin.setParameterValue(WhitecapModeParameter, 0.4f);
    require(plugin.getParameterValue(WhitecapModeParameter) == 0.0f,
            "Mode must be a discrete choice");
    plugin.setParameterValue(WhitecapModeParameter, 0.7f);
    require(plugin.getParameterValue(WhitecapModeParameter) == 1.0f,
            "Vibrato mode must be selectable");
    plugin.setParameterValue(WhitecapBypass, 0.7f);
    require(plugin.getParameterValue(WhitecapBypass) == 1.0f, "Bypass must be boolean");
    for (std::uint32_t i = 0; i < WhitecapParameterCount; ++i)
        plugin.setParameterValue(i, whitecapSpecs[i].initial);
}

static void modesAndBypass(PluginExporter &plugin)
{
    prepare(plugin, 48000.0);
    auto input = audio(1024);
    for (std::size_t i = 0; i < input[0].size(); ++i) {
        const float value = static_cast<float>(
            0.5 * std::sin(2 * 3.14159265358979323846 * 220 * static_cast<double>(i) / 48000));
        input[0][i] = input[1][i] = value;
    }
    input[0][0] = input[1][0] = 0.5f;
    auto output = audio(1024);
    run(plugin, input, output, 0, 1024);
    require(output[0][0] == input[0][0] * 0.8f,
            "Default Chorus must use the documented 20 percent wet mix");
    require(std::all_of(output[0].begin(), output[0].end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Enabled output must remain finite");

    plugin.setParameterValue(WhitecapBypass, 1.0f);
    prepare(plugin, 48000.0);
    output = audio(1024);
    run(plugin, input, output, 0, 1024);
    require(output[0] == input[0] && output[1] == input[1],
            "Bypass must return the unprocessed stereo input");

    plugin.setParameterValue(WhitecapBypass, 0.0f);
    plugin.setParameterValue(WhitecapModeParameter, 1.0f);
    plugin.setParameterValue(WhitecapDepth, 0.0f);
    prepare(plugin, 48000.0);
    auto impulse = audio(481);
    impulse[0][0] = impulse[1][0] = 1.0f;
    output = audio(impulse[0].size());
    run(plugin, impulse, output, 0, static_cast<std::uint32_t>(impulse[0].size()));
    require(output[0][0] == 0.0f && output[0][479] == 0.0f && output[0][480] == 1.0f,
            "Vibrato must be fully wet at its documented centre delay");
    plugin.setParameterValue(WhitecapModeParameter, 0.0f);
}

static void lifecycleAndNoCallbackAllocation(PluginExporter &plugin)
{
    prepare(plugin, 384000.0);
    auto input = audio(128, 0.25f);
    auto output = audio(128, 9.0f);
    run(plugin, input, output, 0, 128);
    require(std::all_of(output[0].begin(), output[0].end(), [](float x) { return x == 0.0f; }),
            "Unsupported sample rate must fail silent");
    plugin.setParameterValue(WhitecapBypass, 1.0f);
    prepare(plugin, 48000.0);
    output = audio(128, 9.0f);
    run(plugin, input, output, 0, 128, true);
    require(allocations == 0, "Plugin processing allocated memory");
    require(output[0] == input[0] && output[1] == input[1],
            "Returning to a supported rate must resume in bypass");
    plugin.setParameterValue(WhitecapBypass, 0.0f);
}

static void scaledHitTesting()
{
    for (double scale : {0.75, 1.0, 1.5}) {
        const auto at = [scale](double x, double y) {
            return whitecapPoint(x * scale + 40, y * scale, WhitecapUiWidth * scale + 80,
                                 WhitecapHeight * scale);
        };
        for (const auto &knob : whitecapKnobs)
            require(whitecapKnobAt(at(knob.x, knob.y)) == static_cast<int>(knob.parameter),
                    "Scaled Whitecap knob hit test wrong");
        require(whitecapModeAt(at(WhitecapUiWidth * 0.5, WhitecapModeY)),
                "Scaled Whitecap mode target wrong");
        require(whitecapFootAt(at(WhitecapUiWidth * 0.5, WhitecapFootY)),
                "Scaled Whitecap footswitch target wrong");
        require(!whitecapFootAt(at(260, 330)) && whitecapKnobAt(at(260, 330)) == -1 &&
                    !whitecapModeAt(at(260, 330)),
                "Whitecap face must not hit an unrelated control");
    }
}

int main(int argc, char **argv)
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000;
    try {
        PluginExporter plugin(nullptr, nullptr, nullptr, nullptr);
        identityAndDefaults(plugin);
        rangesAndSanitizing(plugin);
        modesAndBypass(plugin);
        lifecycleAndNoCallbackAllocation(plugin);
        scaledHitTesting();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    if (argc > 1 && std::strcmp(argv[1], "vst3") != 0) {
        std::fprintf(stderr, "Usage: whitecap_plugin_tests [vst3]\n");
        return 1;
    }
    std::puts("PASS: Whitecap VST3 identity, controls, modes, bypass and lifecycle");
    return 0;
}
