// SPDX-License-Identifier: GPL-3.0-only
#include "DarkDelay.hpp"
#include "DarkReverb.hpp"
#include "PreviewParameters.hpp"
#include "PreviewTail.hpp"
#include "PreviewTailBridge.hpp"
#include "PreviewTempoBridge.hpp"
#include "TempoSync.hpp"
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
static constexpr std::array<std::uint32_t, 13> inputIds{
    Time, Feedback, Mix,           Tone,        Level,          Bypass,      PingPong,
    Sync, Division, InternalTempo, ReverbSpace, ReverbDarkness, ReverbAmount};

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
    require(std::count_if(previewSpecs.begin(), previewSpecs.end(),
                          [](const auto &p) { return !p.readOnly; }) == inputIds.size(),
            "Input coverage list is incomplete");
    for (auto i : inputIds) {
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
            const auto point = pedalPoint(knob.x * scale + 40, knob.y * scale, 520 * scale + 80,
                                          PreviewHeight * scale);
            require(knobAt(point) == static_cast<int>(knob.parameter),
                    "Scaled knob hit test wrong");
        }
        auto scaledPoint = [scale](double x, double y) {
            return pedalPoint(x * scale + 40, y * scale, 520 * scale + 80, PreviewHeight * scale);
        };
        require(timingSyncAt(scaledPoint(170, 94)) && timingSyncAt(scaledPoint(350, 126)),
                "Scaled Sync switch target wrong");
        require(timingSelectorAt(scaledPoint(40, 138)) == 0 &&
                    timingSelectorAt(scaledPoint(480, 170)) == 1 &&
                    timingSelectorAt(scaledPoint(260, 194)) == 2,
                "Scaled selector targets wrong");
        for (int menu = 0; menu < 2; ++menu) {
            for (int row = 0; row < 3; ++row) {
                require(timingMenuRowAt(scaledPoint(menu == 0 ? 145 : 375, 174 + row * 32), menu) ==
                            row,
                        "Scaled popup row target wrong");
            }
        }
        require(timingTempoAt(scaledPoint(260, 246)), "Scaled tempo popup target wrong");
    }
    require(timingSelectorAt({260, 154}) == -1 && timingMenuRowAt({260, 200}, 0) == -1 &&
                timingMenuRowAt({145, 270}, 0) == -1 && timingMenuRowAt({260, 246}, 2) == -1,
            "Timing gaps or tempo popup selected a note");
    require(knobAt({260, PreviewFootY}) == -1, "Footswitch overlaps a knob");
    require(pingPongButtonAt({260, PreviewModeTop + 13}) && !pingPongButtonAt({260, PreviewFootY}),
            "Mode switch hit test wrong");
    require(previewHostIndex(PingPong, false) == 6 && previewHostIndex(PingPong, true) == 8 &&
                MonitorRequest == 6 && MonitorActive == 7,
            "Published host indices moved");
    require(Sync == 7 && Division == 8 && InternalTempo == 9 && EffectiveTempo == 10 &&
                TempoSource == 11 && EffectiveTime == 12 && SyncLimited == 13,
            "Published sync/status indices moved");
    require(ReverbSpace == 14 && ReverbDarkness == 15 && ReverbAmount == 16,
            "Reverb parameters did not append after timing readouts");
    for (bool app : {false, true}) {
        for (auto i : inputIds)
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
                for (std::size_t channel = 0; channel < 2; ++channel)
                    require(std::memcmp(output[channel].data(), expected[channel].data(),
                                        output[channel].size() * sizeof(float)) == 0,
                            "Zero reverb Amount is not bit-identical to the original delay");
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
    const std::array<float, PreviewParameterCount> settings{800, 52, 37, 1800, 0.6f, 1, 1,  1, 4,
                                                            90,  0,  0,  0,    0,    9, 80, 45};
    const std::array<const char *, 6> legacySymbols{"time_ms", "feedback",    "mix",
                                                    "tone_hz", "output_gain", "bypass"};
    for (std::uint32_t i = 0; i < legacySymbols.size(); ++i)
        require(plugin.getParameterSymbol(i) == legacySymbols[i], "Legacy parameter moved");
    if (standalone)
        require(plugin.getParameterSymbol(6) == "monitor_request" &&
                    plugin.getParameterSymbol(7) == "monitor_active",
                "Legacy monitor indices moved");
    for (auto i : inputIds) {
        const auto host = previewHostIndex(i, standalone);
        require(plugin.getParameterSymbol(host) == previewSpecs[i].symbol, "Parameter ID drift");
        require(plugin.getParameterHints(host) & kParameterIsAutomatable,
                "Parameter not automatable");
        trackAllocations = true;
        plugin.setParameterValue(host, settings[i]);
        trackAllocations = false;
        require(plugin.getParameterValue(host) == settings[i], "Parameter value did not persist");
    }
    std::array<float, PreviewParameterCount> saved{};
    for (auto i : inputIds)
        saved[i] = plugin.getParameterValue(previewHostIndex(i, standalone));
    PluginExporter reopened(nullptr, nullptr, nullptr, nullptr);
    reopened.setState("undertow_preview_schema", "1");
    for (auto i : inputIds)
        reopened.setParameterValue(previewHostIndex(i, standalone), saved[i]);
    reopened.activate();
    for (auto i : inputIds)
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
    require(legacy.getParameterValue(previewHostIndex(Sync, standalone)) == 0,
            "Old project settings enabled sync");
    require(legacy.getParameterValue(previewHostIndex(ReverbAmount, standalone)) == 0,
            "Old project settings enabled reverb");
    legacy.setParameterValue(previewHostIndex(PingPong, standalone), 1);
    require(legacy.getParameterValue(previewHostIndex(PingPong, standalone)) == 1 &&
                legacy.getParameterValue(previewHostIndex(Sync, standalone)) == 0,
            "Old Ping-pong settings did not retain free timing");
    legacy.deactivate();
    for (auto i : inputIds) {
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
        for (auto i : inputIds) {
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

static void monitorSafety(PluginExporter &plugin, bool pingPong, bool sync)
{
    auto input = audio(48000, 0.1f);
    auto output = audio(48000, 99);
    plugin.deactivate();
    plugin.activate();
    plugin.setParameterValue(previewHostIndex(Sync, standalone), sync ? 1.0f : 0.0f);
    plugin.setParameterValue(previewHostIndex(PingPong, standalone), pingPong ? 1.0f : 0.0f);
    require(plugin.getParameterValue(MonitorActive) == 0, "Changing mode enabled monitoring");
    run(plugin, input, output, 0, 48000);
    require(output == audio(48000), "Changing mode leaked muted audio");
    plugin.setParameterValue(Level, 1);
    plugin.setParameterValue(Mix, 20);
    plugin.setParameterValue(Bypass, 0);
    plugin.setParameterValue(Time, 20);
    plugin.setParameterValue(previewHostIndex(ReverbAmount, standalone), 100);
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

static void syncIntegration(PluginExporter &plugin)
{
    // Same field/flag extraction template as the generated VST3 adapter uses.
    struct Context {
        std::uint32_t state;
        double bpm;
    };
    constexpr std::uint32_t validTempo = 1, playing = 2;
    auto set = [&](std::uint32_t id, float value) {
        plugin.setParameterValue(previewHostIndex(id, standalone), value);
    };
    auto get = [&](std::uint32_t id) {
        return plugin.getParameterValue(previewHostIndex(id, standalone));
    };
    for (std::uint32_t id = 0; id < PreviewParameterCount; ++id) {
        if (!previewSpecs[id].readOnly)
            continue;
        const auto host = previewHostIndex(id, standalone);
        require(plugin.getParameterHints(host) & kParameterIsOutput, "Timing status not read-only");
        const float before = get(id);
        plugin.setParameterValue(host, 42);
        require(get(id) == before, "Host wrote a timing readout");
    }
    set(Time, 777);
    set(ReverbAmount, 0);
    set(Sync, 1);
    set(InternalTempo, 100);
    set(Feedback, 0);
    set(Tone, 2500);
    set(Mix, 100);
    set(Level, 1);
    set(Bypass, 0);
    plugin.setSampleRate(8000, true);
    // No host tempo has been supplied yet. An unrelated context flag is not valid tempo.
    Context context{playing, 999};
    supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
    require(get(EffectiveTempo) == 100 &&
                get(TempoSource) ==
                    static_cast<float>(standalone ? TempoOrigin::Internal : TempoOrigin::Fallback),
            "Initial internal/fallback source wrong");
    for (bool ping : {false, true}) {
        set(PingPong, ping ? 1 : 0);
        for (double bpm : {40.0, 90.0, 120.0, 180.0}) {
            set(InternalTempo, static_cast<float>(bpm));
            context = {validTempo | playing, bpm};
            for (std::size_t division = 0; division < noteDivisions.size(); ++division) {
                set(Division, static_cast<float>(division));
                supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
                prepare(plugin);
                const double ms = 60000.0 / bpm * noteDivisions[division].beats;
                const auto count = static_cast<std::uint32_t>(std::ceil(ms * 8.0)) * 3 + 128;
                auto input = audio(count), output = audio(count), expected = audio(count);
                input[0][0] = input[1][0] = 1;
                run(plugin, input, output, 0, count);
                DarkDelay reference;
                DelayParameters p;
                p.timeMs = ms;
                p.feedback = 0;
                p.mix = 1;
                p.pingPong = ping;
                reference.prepare(8000, p, 8000);
                const float *inputs[] = {input[0].data(), input[1].data()};
                float *outputs[] = {expected[0].data(), expected[1].data()};
                reference.process(inputs, outputs, count);
                require(output == expected, "Synced processor differs from reference delay");
                require(std::abs(get(EffectiveTime) - ms) < 0.001, "Wrong synced target time");
                require(get(SyncLimited) == 0, "Ordinary note unexpectedly limited");
                const auto echo = static_cast<std::size_t>(std::floor(ms * 8.0));
                require(output[0][echo] > 0, "No synced echo at expected onset");
                // The same valid tempo must remain valid when transport stops.
                context.state = validTempo;
                trackAllocations = true;
                supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
                trackAllocations = false;
                require(std::abs(get(EffectiveTime) - ms) < 0.001,
                        "Stopping transport changed synced delay");
            }
        }
    }
    // Changing tempo while stopped must work, not switch to milliseconds.
    set(Division, 0);
    set(InternalTempo, 90);
    context = {validTempo, 90};
    supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
    require(std::abs(get(EffectiveTime) - 60000.0 / 90) < 0.001, "Stopped tempo change ignored");
    context.state = playing; // lose tempo validity; do not invent 120
    context.bpm = 120;
    supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
    require(get(EffectiveTempo) == 90 &&
                get(TempoSource) ==
                    static_cast<float>(standalone ? TempoOrigin::Internal : TempoOrigin::Held),
            "Missing tempo did not hold last valid BPM");
    supplyPreviewTempo<Context>(plugin.getInstancePointer(), nullptr, validTempo);
    require(get(EffectiveTempo) == 90, "Missing context changed tempo");
    {
        PluginExporter other(nullptr, nullptr, nullptr, nullptr);
        require(other.getParameterValue(previewHostIndex(EffectiveTempo, standalone)) == 120,
                "Another instance inherited tempo history");
        Context otherContext{validTempo, 144};
        supplyPreviewTempo(other.getInstancePointer(), &otherContext, validTempo);
        require(get(EffectiveTempo) == 90, "Tempo bridge updated the wrong instance");
    }
    context = {validTempo, std::numeric_limits<double>::quiet_NaN()};
    supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
    require(get(EffectiveTempo) == 90, "Invalid tempo contaminated state");
    set(Sync, 0);
    supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
    require(get(EffectiveTime) == 777 && get(Time) == 777, "Free time was not preserved");
    set(Sync, 1);
    set(Division, 1); // dotted quarter
    context = {validTempo, 1};
    supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
    if (!standalone)
        require(get(EffectiveTime) == 8000 && get(SyncLimited) == 1,
                "Long sync target was not visibly limited");
    context = {validTempo, 1000};
    set(Division, 8); // sixteenth triplet: 10 ms, below the 20 ms floor
    supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
    if (!standalone)
        require(get(EffectiveTime) == 20 && get(SyncLimited) == 1,
                "Short sync target was not visibly limited");
    if (!standalone) {
        context = {validTempo, 7.5}; // a quarter note exactly fills eight seconds
        set(Division, 0);
        set(Feedback, 50);
        supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
        prepare(plugin);
        auto impulse = audio(129000), longOutput = audio(129000);
        impulse[0][0] = impulse[1][0] = 1;
        run(plugin, impulse, longOutput, 0, 129000);
        require(get(EffectiveTime) == 8000 && get(SyncLimited) == 0 && longOutput[0][64000] > 0 &&
                    longOutput[1][128000] > 0,
                "Actual processor failed the eight-second ping-pong boundary");
    }

    // Exercise rapid host tempo + division changes through real processing without allocations.
    auto input = audio(16000, 0.1f), output = audio(16000);
    set(InternalTempo, 120);
    set(Feedback, 90);
    prepare(plugin);
    for (std::size_t offset = 0; offset < 16000; offset += 80) {
        context = {validTempo | (offset % 160 == 0 ? playing : 0),
                   offset % 160 == 0 ? 90.0 : 180.0};
        trackAllocations = true;
        set(Division, static_cast<float>((offset / 80) % 9));
        supplyPreviewTempo(plugin.getInstancePointer(), &context, validTempo);
        trackAllocations = false;
        run(plugin, input, output, offset, 80);
    }
    for (float sample : output[0])
        require(std::isfinite(sample) && std::abs(sample) <= 1.001f,
                "Tempo automation exceeded bound");
    set(Sync, 0);
    set(Mix, 20);
    set(Bypass, 1);
}

static std::array<double, 2> reverbInsert(void *context,
                                          const std::array<double, 2> &input) noexcept
{
    return static_cast<DarkReverb *>(context)->processSample(input);
}

static void reverbIntegration(PluginExporter &plugin)
{
    auto set = [&](std::uint32_t id, float value) {
        plugin.setParameterValue(previewHostIndex(id, standalone), value);
    };
    plugin.setSampleRate(8000, true);
    set(Time, 40);
    set(Feedback, 35);
    set(Mix, 40);
    set(Tone, 2500);
    set(Level, 1);
    set(Bypass, 0);
    set(Division, 0);
    set(InternalTempo, 120);
    set(ReverbSpace, 6);
    set(ReverbDarkness, 55);
    set(ReverbAmount, 65);
    struct Context {
        std::uint32_t state;
        double bpm;
    } context{1, 120};
    supplyPreviewTempo(plugin.getInstancePointer(), &context, 1);
    auto input = audio(16000);
    for (std::size_t i = 0; i < 1200; ++i) {
        input[0][i] = static_cast<float>(0.2 * std::sin(i * 0.11));
        input[1][i] = static_cast<float>(0.1 * std::cos(i * 0.07));
    }
    for (bool sync : {false, true}) {
        set(Sync, sync ? 1 : 0);
        for (bool ping : {false, true}) {
            set(PingPong, ping ? 1 : 0);
            for (std::uint32_t size : {63u, 512u}) {
                for (bool inPlace : {false, true}) {
                    prepare(plugin);
                    auto output = audio(16000), expected = audio(16000);
                    for (std::size_t offset = 0; offset < 16000;) {
                        const auto count =
                            static_cast<std::uint32_t>(std::min<std::size_t>(size, 16000 - offset));
                        run(plugin, input, output, offset, count, inPlace);
                        offset += count;
                    }
                    DarkDelay reference;
                    DelayParameters p;
                    p.timeMs = sync ? 500 : 40;
                    p.mix = 0.4;
                    p.pingPong = ping;
                    reference.prepare(8000, p, 8000);
                    DarkReverb verb;
                    verb.prepare(8000, {6, 55, 0.65});
                    const float *inputs[] = {input[0].data(), input[1].data()};
                    float *outputs[] = {expected[0].data(), expected[1].data()};
                    reference.process(inputs, outputs, 16000, reverbInsert, &verb);
                    require(output == expected,
                            "Wet reverb plugin differs from independent chain reference");
                }
            }
        }
    }
    set(Mix, 0);
    set(ReverbAmount, 100);
    prepare(plugin);
    auto dry = audio(16000);
    run(plugin, input, dry, 0, 16000, true);
    require(dry == input, "Reverb changed the original guitar when Mix is dry-only");
    set(Mix, 100);
    set(Sync, 0);
    set(Feedback, 0);
    prepare(plugin);
    auto pulse = audio(32000), output = audio(32000);
    pulse[0][0] = pulse[1][0] = 1;
    run(plugin, pulse, output, 0, 32000);
    require(
        std::all_of(output[0].begin(), output[0].begin() + 559, [](float v) { return v == 0; }) &&
            output[0][559] != 0,
        "Reverb is not strictly after the delayed wet path");

    auto tail = [&](bool newNote) {
        set(Bypass, 0);
        prepare(plugin);
        auto signal = pulse, result = audio(32000);
        if (newNote)
            signal[0][16000] = 1;
        run(plugin, signal, result, 0, 8000);
        set(Bypass, 1);
        run(plugin, signal, result, 8000, 24000);
        return result;
    };
    const auto originalTail = tail(false), bypassedNote = tail(true);
    double lateEnergy = 0;
    for (std::size_t i = 16001; i < 32000; ++i) {
        lateEnergy +=
            originalTail[0][i] * originalTail[0][i] + originalTail[1][i] * originalTail[1][i];
        require(originalTail[0][i] == bypassedNote[0][i] &&
                    originalTail[1][i] == bypassedNote[1][i],
                "New bypassed note entered the reverb");
    }
    require(lateEnergy > 1e-12, "Bypass cut the long reverb tail");
    require(std::abs(bypassedNote[0][16000] - originalTail[0][16000] - 1) < 1e-6,
            "Effect bypass did not preserve clean new guitar");
}

static void tailReporting(PluginExporter &plugin)
{
    auto set = [&](std::uint32_t id, float value) {
        plugin.setParameterValue(previewHostIndex(id, standalone), value);
    };
    plugin.setSampleRate(48000, true);
    prepare(plugin);
    set(Time, 400);
    set(Feedback, 35);
    set(Mix, 20);
    set(Bypass, 0);
    set(Sync, 0);
    set(ReverbSpace, 6);
    set(ReverbAmount, 0);

    // 0.4 s at 35 % feedback reaches -60 dB after seven repeats.
    require(previewTailSamples(plugin.getInstancePointer()) == 134400,
            "Default VST3 tail did not follow the decay formula");
    set(Feedback, 0);
    require(previewTailSamples(plugin.getInstancePointer()) == 19200,
            "Zero-feedback tail should be one delay interval");

    set(Feedback, 90);
    set(Time, 2000);
    require(previewTailSamples(plugin.getInstancePointer()) == 6336000,
            "Long feedback tail was not reported");

    set(Time, 400);
    set(Feedback, 35);
    set(Mix, 0);
    require(previewTailSamples(plugin.getInstancePointer()) == 0,
            "Fully dry settings must report no wet tail");
    set(Mix, 20);
    set(Bypass, 1);
    require(previewTailSamples(plugin.getInstancePointer()) == 0,
            "Bypass must not report a wet tail");
    set(Bypass, 0);

    set(ReverbSpace, 12);
    set(ReverbAmount, 50);
    require(previewTailSamples(plugin.getInstancePointer()) == 710400,
            "Reverb decay was not added to the delay tail");
    set(ReverbAmount, 0);

    struct Context {
        std::uint32_t state;
        double bpm;
    } context{1, 120};
    set(Sync, 1);
    set(Feedback, 0);
    supplyPreviewTempo(plugin.getInstancePointer(), &context, 1);
    require(previewTailSamples(plugin.getInstancePointer()) == 24000,
            "Sync tail did not use the effective tempo target");

    set(Sync, 0);
    set(Feedback, 35);
    set(Time, 400);
    plugin.setSampleRate(96000, true);
    require(previewTailSamples(plugin.getInstancePointer()) == 268800,
            "Tail samples did not follow the sample rate");
    plugin.setSampleRate(48000, true);

    PreviewTailSettings capped{10000, 0.9, 1, false, 12, 1};
    require(previewTailSeconds(capped) == PreviewTailCapSeconds, "Tail cap was not applied");
    require(previewTailSamples(48000, capped) == 28800000,
            "Capped tail did not convert to samples");
    require(previewTailSamples(0, capped) == 0, "Invalid sample rate produced a tail");
    require(previewTailSeconds({0.4, 0.35, -1, false, 6, 0}) == 0,
            "Negative mix produced a wet tail");
    require(
        previewTailSeconds({std::numeric_limits<double>::quiet_NaN(), 0.35, 0.2, false, 6, 0}) == 0,
        "Invalid delay time produced a tail");
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
                    (standalone ? StandaloneParameterCount : PreviewParameterCount),
                "Parameter count wrong");
        require(plugin.getStateKey(0) == "undertow_preview_schema", "Schema marker missing");
        for (auto i : inputIds)
            require(plugin.getParameterValue(previewHostIndex(i, standalone)) ==
                        previewSpecs[i].initial,
                    "Default wrong");
        plugin.activate();
        mappings();
        integration(plugin, false);
        integration(plugin, true);
        parametersAndRecall(plugin);
        bypassAndAutomation(plugin);
        syncIntegration(plugin);
        reverbIntegration(plugin);
        tailReporting(plugin);
        if (standalone) {
            for (bool sync : {false, true}) {
                monitorSafety(plugin, false, sync);
                monitorSafety(plugin, true, sync);
            }
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
