// SPDX-License-Identifier: GPL-3.0-only
#include "DarkDelay.hpp"
#include "PreviewParameters.hpp"
#include "DistrhoPluginUtils.hpp"
#include "src/DistrhoPluginInternal.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

static bool standalone = false;
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
    return standalone ? "JACK/Standalone" : "VST3";
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

static void enable(PluginExporter &plugin)
{
    if (standalone)
        plugin.setParameterValue(MonitorRequest, 1);
}

static void prepare(PluginExporter &plugin)
{
    plugin.deactivate();
    plugin.activate();
    enable(plugin);
}

static void mappings()
{
    for (std::uint32_t i = 0; i < EffectParameterCount; ++i) {
        const auto &spec = previewSpecs[i];
        require(denormalizedPreview(i, 0) == spec.minimum, "Knob minimum wrong");
        require(std::abs(denormalizedPreview(i, 1) - spec.maximum) < 0.002f, "Knob maximum wrong");
        require(std::abs(denormalizedPreview(i, normalizedPreview(i, spec.initial)) -
                         spec.initial) < 0.002f,
                "Knob round trip wrong");
    }
    require(sanitizePreview(Bypass, 0.9f) == 1, "Boolean mapping wrong");
    for (double scale : {0.75, 1.0, 1.5}) {
        for (const auto &knob : previewKnobs) {
            const auto point =
                pedalPoint(knob.x * scale + 40, knob.y * scale, 520 * scale + 80, 680 * scale);
            require(knobAt(point) == static_cast<int>(knob.parameter),
                    "Scaled knob hit test wrong");
        }
    }
    require(knobAt({260, 540}) == -1, "Footswitch overlaps a knob");
    require(pingPongButtonAt({260, 467}) && !pingPongButtonAt({260, 540}),
            "Mode switch hit test wrong");
    require(previewHostIndex(PingPong, false) == 6 && previewHostIndex(PingPong, true) == 8 &&
                MonitorRequest == 6 && MonitorActive == 7,
            "Published host indices moved");
    for (bool app : {false, true}) {
        for (std::uint32_t i = 0; i < EffectParameterCount; ++i)
            require(previewEffectIndex(previewHostIndex(i, app), app) == static_cast<int>(i),
                    "Host/effect mapping not invertible");
    }
    require(previewEffectIndex(6, true) == -1 && previewEffectIndex(7, true) == -1,
            "Monitor slots overlap effect parameters");
}

static void integration(PluginExporter &plugin, bool pingPong)
{
    plugin.setParameterValue(Time, 20);
    plugin.setParameterValue(previewHostIndex(PingPong, standalone), pingPong ? 1.0f : 0.0f);
    const std::size_t count = 8192;
    auto input = audio(count + 1);
    input[0][0] = 1.0f;
    input[1][5] = -0.5f;
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        plugin.setSampleRate(rate, true);
        for (std::uint32_t frames : {1u, 2u, 63u, 64u, 127u, 512u, 2048u}) {
            plugin.setBufferSize(frames, true);
            for (bool inPlace : {false, true}) {
                prepare(plugin);
                auto output = audio(count + 1, 99);
                for (std::size_t offset = 0; offset < count;) {
                    auto size =
                        static_cast<std::uint32_t>(std::min<std::size_t>(frames, count - offset));
                    run(plugin, input, output, offset, size, inPlace);
                    offset += size;
                }
                require(output[0][count] == 99 && output[1][count] == 99, "Wrote past block end");
                DarkDelay reference;
                DelayParameters p;
                p.timeMs = 20;
                p.pingPong = pingPong;
                reference.prepare(rate, p);
                auto expected = audio(count + 1, 99);
                const float *inputs[] = {input[0].data(), input[1].data()};
                float *outputs[] = {expected[0].data(), expected[1].data()};
                reference.process(inputs, outputs, count);
                require(output == expected, "Plugin differs from delay core");
                const auto firstEcho = static_cast<std::size_t>(rate * 0.020);
                require(output[0][firstEcho] > 0, "Plugin produced no echo");
                if (pingPong) {
                    require(output[1][firstEcho + 5] == 0,
                            "First ping-pong echo was not left-only");
                    require(output[1][firstEcho * 2] > 0,
                            "Second ping-pong echo did not reach right");
                }
            }
        }
    }
    auto output = audio(1, 99);
    run(plugin, input, output, 0, 0);
    require(output[0][0] == 99 && output[1][0] == 99, "Zero-frame callback wrote output");
    trackAllocations = true;
    plugin.run(nullptr, nullptr, 0);
    trackAllocations = false;
}

static void parametersAndRecall(PluginExporter &plugin)
{
    const std::array<float, EffectParameterCount> settings{800, 52, 37, 1800, 0.6f, 1, 1};
    const std::array<const char *, 6> legacySymbols{"time_ms", "feedback",    "mix",
                                                    "tone_hz", "output_gain", "bypass"};
    for (std::uint32_t i = 0; i < legacySymbols.size(); ++i)
        require(plugin.getParameterSymbol(i) == legacySymbols[i], "Legacy parameter moved");
    if (standalone)
        require(plugin.getParameterSymbol(6) == "monitor_request" &&
                    plugin.getParameterSymbol(7) == "monitor_active",
                "Legacy monitor indices moved");
    for (std::uint32_t i = 0; i < EffectParameterCount; ++i) {
        const auto host = previewHostIndex(i, standalone);
        require(plugin.getParameterSymbol(host) == previewSpecs[i].symbol, "Parameter ID drift");
        require(plugin.getParameterHints(host) & kParameterIsAutomatable,
                "Parameter not automatable");
        trackAllocations = true;
        plugin.setParameterValue(host, settings[i]);
        trackAllocations = false;
        require(plugin.getParameterValue(host) == settings[i], "Parameter value did not persist");
    }
    std::array<float, EffectParameterCount> saved{};
    for (std::uint32_t i = 0; i < EffectParameterCount; ++i)
        saved[i] = plugin.getParameterValue(previewHostIndex(i, standalone));
    PluginExporter reopened(nullptr, nullptr, nullptr, nullptr);
    reopened.setState("undertow_preview_schema", "1");
    for (std::uint32_t i = 0; i < EffectParameterCount; ++i)
        reopened.setParameterValue(previewHostIndex(i, standalone), saved[i]);
    reopened.activate();
    for (std::uint32_t i = 0; i < EffectParameterCount; ++i)
        require(reopened.getParameterValue(previewHostIndex(i, standalone)) == settings[i],
                "Reconstructed settings differ");
    if (standalone)
        require(reopened.getParameterValue(MonitorActive) == 0, "Recall enabled monitoring");
    reopened.deactivate();
    // Old projects contain only the original six effect parameters and schema 1.
    PluginExporter legacy(nullptr, nullptr, nullptr, nullptr);
    legacy.setState("undertow_preview_schema", "1");
    for (std::uint32_t i = 0; i < legacySymbols.size(); ++i)
        legacy.setParameterValue(i, settings[i]);
    legacy.activate();
    require(legacy.getParameterValue(previewHostIndex(PingPong, standalone)) == 0,
            "Old project settings did not retain Normal mode");
    legacy.deactivate();
    for (std::uint32_t i = 0; i < EffectParameterCount; ++i) {
        const auto host = previewHostIndex(i, standalone);
        plugin.setParameterValue(host, std::numeric_limits<float>::quiet_NaN());
        require(plugin.getParameterValue(host) == previewSpecs[i].initial,
                "Invalid value not sanitized");
        plugin.setParameterValue(host, -10000);
        require(plugin.getParameterValue(host) == previewSpecs[i].minimum, "Minimum not enforced");
        plugin.setParameterValue(host, 100000);
        require(plugin.getParameterValue(host) == previewSpecs[i].maximum, "Maximum not enforced");
        plugin.setParameterValue(host, previewSpecs[i].initial);
    }
}

static void bypassAndAutomation(PluginExporter &plugin)
{
    plugin.setSampleRate(48000, true);
    plugin.setParameterValue(Time, 100);
    prepare(plugin);
    auto input = audio(48000);
    auto output = audio(48000);
    input[0][0] = 1;
    run(plugin, input, output, 0, 2400);
    plugin.setParameterValue(Bypass, 1);
    input[0][12000] = 1;
    run(plugin, input, output, 2400, 45600);
    require(output[0][4800] > 0, "Effect bypass cut tail");
    require(std::abs(output[0][12000] - 1) < 1e-6f, "Bypass dry gain wrong");
    // Automate all inputs through the exporter while processing a smooth signal.
    for (std::size_t i = 0; i < input[0].size(); ++i)
        input[0][i] = static_cast<float>(0.1 * std::sin(i * 0.01));
    for (std::size_t offset = 0; offset < input[0].size(); offset += 80) {
        for (std::uint32_t i = 0; i < EffectParameterCount; ++i) {
            trackAllocations = true;
            plugin.setParameterValue(previewHostIndex(i, standalone),
                                     offset % 160 == 0 ? previewSpecs[i].minimum
                                                       : previewSpecs[i].maximum);
            trackAllocations = false;
        }
        run(plugin, input, output, offset, 80);
    }
    for (float sample : output[0])
        require(std::isfinite(sample), "Automation produced nonfinite audio");
}

static void monitorSafety(PluginExporter &plugin, bool pingPong)
{
    auto input = audio(48000, 0.1f);
    auto output = audio(48000, 99);
    plugin.deactivate();
    plugin.activate();
    plugin.setParameterValue(previewHostIndex(PingPong, standalone), pingPong ? 1.0f : 0.0f);
    require(plugin.getParameterValue(MonitorActive) == 0, "Changing mode enabled monitoring");
    run(plugin, input, output, 0, 48000);
    require(output == audio(48000), "Changing mode leaked muted audio");
    plugin.setParameterValue(Level, 1);
    plugin.setParameterValue(Mix, 20);
    plugin.setParameterValue(Bypass, 0);
    plugin.setParameterValue(Time, 20);
    plugin.setParameterValue(previewHostIndex(PingPong, standalone), pingPong ? 1.0f : 0.0f);
    prepare(plugin);
    run(plugin, input, output, 0, 48000); // load the delay with a sustained signal
    plugin.setParameterValue(MonitorRequest, 0);
    plugin.setParameterValue(Bypass, 1);
    run(plugin, input, output, 0, 48000, true);
    require(output == audio(48000), "Monitor mute leaked dry or tail during bypass");
    plugin.setParameterValue(MonitorRequest, 1);
    require(plugin.getParameterValue(MonitorActive) == 1, "Monitor did not enable");
    require(plugin.getParameterValue(MonitorRequest) == 0, "Monitor request would save enabled");
    plugin.setState("undertow_preview_schema", "1");
    plugin.deactivate();
    trackAllocations = true;
    plugin.activate();
    trackAllocations = false;
    require(plugin.getParameterValue(MonitorActive) == 0, "Restart restored monitor consent");
    plugin.setParameterValue(MonitorRequest, std::numeric_limits<float>::quiet_NaN());
    require(plugin.getParameterValue(MonitorActive) == 0, "NaN unmuted monitoring");
    plugin.setParameterValue(MonitorRequest, 1);
    // A quick mute/unmute between callbacks must still clear buffered audio.
    plugin.setParameterValue(Bypass, 0);
    run(plugin, input, output, 0, 48000);
    plugin.setParameterValue(MonitorRequest, 0);
    plugin.setParameterValue(MonitorRequest, 1);
    auto silence = audio(48000);
    run(plugin, silence, output, 0, 48000);
    require(output == silence, "Monitor re-enable exposed an old tail");
    PluginExporter other(nullptr, nullptr, nullptr, nullptr);
    other.activate();
    require(other.getParameterValue(MonitorActive) == 0, "Instance inherited monitor consent");
    other.deactivate();
}

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Expected vst3 or standalone");
        require(std::strcmp(argv[1], "vst3") == 0 || std::strcmp(argv[1], "standalone") == 0,
                "Unknown mode");
        standalone = std::strcmp(argv[1], "standalone") == 0;
        d_nextBufferSize = 512;
        d_nextSampleRate = 48000;
        PluginExporter plugin(nullptr, nullptr, nullptr, nullptr);
        require(plugin.getParameterCount() ==
                    (standalone ? StandaloneParameterCount : EffectParameterCount),
                "Parameter count wrong");
        require(plugin.getStateKey(0) == "undertow_preview_schema", "Schema marker missing");
        for (std::uint32_t i = 0; i < EffectParameterCount; ++i)
            require(plugin.getParameterValue(previewHostIndex(i, standalone)) ==
                        previewSpecs[i].initial,
                    "Default wrong");
        plugin.activate();
        mappings();
        integration(plugin, false);
        integration(plugin, true);
        parametersAndRecall(plugin);
        bypassAndAutomation(plugin);
        if (standalone) {
            monitorSafety(plugin, false);
            monitorSafety(plugin, true);
        }
        plugin.setSampleRate(384000, true);
        enable(plugin);
        auto input = audio(16, 1), output = audio(16, 99);
        run(plugin, input, output, 0, 16);
        require(output == audio(16), "Unsupported sample rate did not fail muted");
        plugin.setSampleRate(48000, true);
        enable(plugin);
        run(plugin, input, output, 0, 16);
        require(output[0][0] != 0, "Supported sample rate did not recover");
        plugin.deactivate();
        require(allocations == 0, "Processing or control update allocated");
        std::printf("PASS: preview %s, echo/core comparison, buffers/rates, parameters/recall, "
                    "automation/bypass, monitor safety and allocation checks\n",
                    argv[1]);
        return 0;
    } catch (const std::exception &error) {
        trackAllocations = false;
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
