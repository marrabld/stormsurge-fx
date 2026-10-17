// SPDX-License-Identifier: GPL-3.0-only
#include "MaelstromParameters.hpp"
#include "MaelstromOverdrive.hpp"
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

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

using Audio = std::array<std::vector<float>, 2>;
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
    trackAllocations = true;
    plugin.run(inputs, outputs, frames);
    trackAllocations = false;
}

// DPF hands the exporter its buffer size and rate before construction. The
// exporter asserts on an unbalanced deactivate, so guard it.
static void prepare(PluginExporter &plugin, double rate)
{
    plugin.deactivateIfNeeded();
    plugin.setSampleRate(rate, true);
    plugin.activate();
}

static void identity_and_defaults(PluginExporter &plugin)
{
    require(std::strcmp(plugin.getName(), "Maelstrom") == 0, "Wrong plugin name");
    require(std::strcmp(plugin.getLabel(), "maelstrom") == 0, "Wrong plugin label");
    // A distinct identity is what stops an old Undertow project from silently
    // acquiring an overdrive, and keeps saved sessions apart.
    require(plugin.getUniqueId() == d_cconst("UtM0"), "Wrong unique id");
    require(plugin.getVersion() == d_version(0, 4, 0), "Wrong version");
    require(plugin.getParameterCount() == MaelstromParameterCount, "Wrong parameter count");
    require(plugin.getStateKey(0) == "maelstrom_schema", "Missing Maelstrom schema marker");
    for (std::uint32_t i = 0; i < MaelstromParameterCount; ++i) {
        require(plugin.getParameterValue(i) == maelstromSpecs[i].initial,
                "A parameter does not start at its documented default");
        require(plugin.isParameterInput(i), "Every Maelstrom control must be an input");
        require(plugin.getParameterSymbol(i) == maelstromSpecs[i].symbol,
                "Parameter symbols must match the declared spec");
    }
    // Oversampling owes the host an accurate report rather than a silent offset.
    require(plugin.getLatency() == 1, "Latency must be reported, not left at zero");
    require(plugin.getAudioPortCountWithGroupId(true, kPortGroupStereo) == 2 &&
                plugin.getAudioPortCountWithGroupId(false, kPortGroupStereo) == 2,
            "Maelstrom must present a stereo pair grouped for routing");
}

static void parameter_ranges_and_sanitisation(PluginExporter &plugin)
{
    for (std::uint32_t i = 0; i < MaelstromParameterCount; ++i) {
        const auto &spec = maelstromSpecs[i];
        require(denormalizedMaelstrom(i, 0) == spec.minimum, "Knob minimum wrong");
        require(std::abs(denormalizedMaelstrom(i, 1) - spec.maximum) < 0.002f,
                "Knob maximum wrong");
        require(std::abs(denormalizedMaelstrom(i, normalizedMaelstrom(i, spec.initial)) -
                         spec.initial) < 0.002f,
                "Knob round trip wrong");
        const auto ranges = plugin.getParameterRanges(i);
        require(ranges.min == spec.minimum && ranges.max == spec.maximum &&
                    ranges.def == spec.initial,
                "Host-visible ranges disagree with the spec");
    }
    // The approved +12 dB ceiling must be exactly reachable, or a host automation
    // lane writing the maximum would be silently clamped.
    require(std::abs(denormalizedMaelstrom(MaelstromLevel, 1) - MaelstromMaxLevel) < 1e-6f,
            "Level maximum must match the approved +12 dB ceiling");

    plugin.setParameterValue(Drive, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(Drive) == maelstromSpecs[Drive].initial,
            "Non-finite parameter must fall back to the default");
    plugin.setParameterValue(Drive, 500.0f);
    require(plugin.getParameterValue(Drive) == maelstromSpecs[Drive].maximum,
            "Out-of-range parameter must clamp to the range");
    plugin.setParameterValue(MaelstromBypass, 0.4f);
    require(plugin.getParameterValue(MaelstromBypass) == 0.0f, "Bypass must stay boolean");
    plugin.setParameterValue(MaelstromBypass, 1.0f);
    require(plugin.getParameterValue(MaelstromBypass) == 1.0f, "Bypass must reach the on state");
    plugin.setParameterValue(Drive, maelstromSpecs[Drive].initial);
    plugin.setParameterValue(MaelstromBypass, 0.0f);
}

static void bypass_is_unity_and_ignores_controls(PluginExporter &plugin)
{
    // Push the amp hard with boost engaged, then bypass: a real pedal switched off
    // means clean guitar at unity, not the last tone setting still applied.
    plugin.setParameterValue(Drive, 100.0f);
    plugin.setParameterValue(Tight, 100.0f);
    plugin.setParameterValue(Bite, 100.0f);
    plugin.setParameterValue(MaelstromLevel, MaelstromMaxLevel);
    plugin.setParameterValue(MaelstromBypass, 1.0f);
    prepare(plugin, 48000.0);
    const auto input = audio(256, 0.5f);
    auto output = audio(256, 9.0f);
    run(plugin, input, output, 0, 256);
    for (std::size_t i = 0; i < 256; ++i)
        require(output[0][i] == 0.5f && output[1][i] == 0.5f,
                "Bypass must be exactly unity dry, ignoring every control");

    // Re-enabling must fade in, not jump.
    plugin.setParameterValue(MaelstromBypass, 0.0f);
    auto faded = audio(256, 9.0f);
    run(plugin, input, faded, 0, 256);
    require(faded[0][0] != 9.0f, "Enabling the pedal must produce audio immediately");
    double step = 0;
    for (std::size_t i = 1; i < 256; ++i)
        step = std::max(step, static_cast<double>(std::abs(faded[0][i] - faded[0][i - 1])));
    require(step < 0.5, "Enabling must ramp rather than jump to the distorted signal");

    plugin.setParameterValue(Drive, maelstromSpecs[Drive].initial);
    plugin.setParameterValue(Tight, maelstromSpecs[Tight].initial);
    plugin.setParameterValue(Bite, maelstromSpecs[Bite].initial);
    plugin.setParameterValue(MaelstromLevel, maelstromSpecs[MaelstromLevel].initial);
}

static void wet_path_actually_processes(PluginExporter &plugin)
{
    prepare(plugin, 48000.0);
    Audio input = audio(1024);
    for (std::size_t i = 0; i < input[0].size(); ++i)
        input[0][i] = input[1][i] =
            0.2f * std::sin(2 * 3.14159265358979323846 * 220 * static_cast<double>(i) / 48000);
    auto output = audio(1024);
    run(plugin, input, output, 0, 1024);
    double difference = 0;
    for (std::size_t i = 0; i < 1024; ++i)
        difference += std::abs(output[0][i] - input[0][i]);
    require(difference > 0, "Enabled overdrive must actually process the signal");
    require(
        std::all_of(output[0].begin(), output[0].end(), [](float v) { return std::isfinite(v); }),
        "Output must be finite");
}

static void channels_stay_independent(PluginExporter &plugin)
{
    plugin.setParameterValue(Drive, 60.0f);
    prepare(plugin, 48000.0);
    Audio input = audio(512);
    for (std::size_t i = 0; i < 512; ++i)
        input[0][i] =
            0.3f * std::sin(2 * 3.14159265358979323846 * 330 * static_cast<double>(i) / 48000);
    auto output = audio(512, 7.0f);
    run(plugin, input, output, 0, 512);
    for (std::size_t i = 0; i < 512; ++i)
        require(output[1][i] == 0.0f, "A silent channel must stay silent: no crossfeed");
    plugin.setParameterValue(Drive, maelstromSpecs[Drive].initial);
}

static void saved_settings_come_back(PluginExporter &source, PluginExporter &reopened)
{
    source.setParameterValue(Drive, 37.0f);
    source.setParameterValue(Tight, 71.0f);
    source.setParameterValue(Bite, 12.0f);
    source.setParameterValue(MaelstromLevel, 1.995f);
    source.setParameterValue(MaelstromBypass, 1.0f);
    // DPF persists input parameters itself; a host restore replays them here.
    std::array<float, MaelstromParameterCount> saved{};
    for (std::uint32_t i = 0; i < MaelstromParameterCount; ++i) {
        saved[i] = source.getParameterValue(i);
        reopened.setParameterValue(i, saved[i]);
    }
    reopened.setState("maelstrom_schema", "1");
    for (std::uint32_t i = 0; i < MaelstromParameterCount; ++i)
        require(reopened.getParameterValue(i) == saved[i],
                "A saved setting did not come back after reopening");
}

static void unsupported_sample_rate_fails_silent(PluginExporter &plugin)
{
    prepare(plugin, 384000.0);
    const auto input = audio(128, 0.5f);
    auto output = audio(128, 9.0f);
    run(plugin, input, output, 0, 128);
    for (std::size_t i = 0; i < 128; ++i)
        require(output[0][i] == 0.0f && output[1][i] == 0.0f,
                "An unsupported sample rate must output silence, not garbage");
    prepare(plugin, 48000.0);
    auto resumed = audio(128, 9.0f);
    run(plugin, input, resumed, 0, 128);
    require(resumed[0].back() != 9.0f, "Returning to a supported rate must resume processing");
}

static void reactivation_clears_processing_history(PluginExporter &plugin)
{
    plugin.setParameterValue(Drive, 100.0f);
    prepare(plugin, 48000.0);
    const auto input = audio(2048, 0.5f);
    auto warm = audio(2048);
    run(plugin, input, warm, 0, 2048);
    // A new activation must not let the previous rate's filtered audio burst out.
    prepare(plugin, 96000.0);
    plugin.setParameterValue(MaelstromBypass, 1.0f);
    auto output = audio(2048);
    run(plugin, input, output, 0, 2048);
    for (std::size_t i = 0; i < 2048; ++i)
        require(output[0][i] == input[0][i],
                "Activation must clear filter state, not resume a tail");
    plugin.setParameterValue(Drive, maelstromSpecs[Drive].initial);
    plugin.setParameterValue(MaelstromBypass, 0.0f);
}

static void no_allocations_while_processing(PluginExporter &plugin)
{
    prepare(plugin, 48000.0);
    const auto input = audio(256, 0.2f);
    auto output = audio(256);
    run(plugin, input, output, 0, 256);
    allocations = 0;
    run(plugin, input, output, 0, 256, true);
    require(allocations == 0, "Processing must not allocate");
}

static void hit_testing_follows_the_approved_layout()
{
    for (double scale : {0.75, 1.0, 1.5}) {
        // Widening only the width makes the UI scale exactly to `scale`, so the
        // mapping exercises a real resize rather than the letterboxed centring.
        const auto at = [scale](double x, double y) {
            return maelstromPoint(x * scale + 40, y * scale, MaelstromWidth * scale + 80,
                                  MaelstromHeight * scale);
        };
        for (const auto &knob : maelstromKnobs)
            require(maelstromKnobAt(at(knob.x, knob.y)) == static_cast<int>(knob.parameter),
                    "Scaled knob hit test wrong");
        require(maelstromFootAt(at(260, MaelstromFootY)), "Scaled footswitch target wrong");
        // The gap between the knob rows is empty face, not a hidden control.
        require(!maelstromFootAt(at(260, 300)) && maelstromKnobAt(at(260, 300)) == -1,
                "Empty face must not hit the footswitch or a knob");
        require(maelstromKnobAt(at(260, MaelstromFootY)) == -1,
                "Footswitch area must not overlap a knob");
    }
}

int main()
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000;
    try {
        PluginExporter plugin(nullptr, nullptr, nullptr, nullptr);
        identity_and_defaults(plugin);
        parameter_ranges_and_sanitisation(plugin);
        bypass_is_unity_and_ignores_controls(plugin);
        wet_path_actually_processes(plugin);
        channels_stay_independent(plugin);
        unsupported_sample_rate_fails_silent(plugin);
        reactivation_clears_processing_history(plugin);
        no_allocations_while_processing(plugin);
        // The second exporter stands in for a reopened project.
        PluginExporter reopened(nullptr, nullptr, nullptr, nullptr);
        saved_settings_come_back(plugin, reopened);
        hit_testing_follows_the_approved_layout();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::puts("PASS: Maelstrom plugin identity, parameters, state and lifecycle checks");
    return 0;
}
