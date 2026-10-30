// SPDX-License-Identifier: GPL-3.0-only
#include "DarkDelay.hpp"
#include "DarkReverb.hpp"
#include "PreviewParameters.hpp"
#include "PreviewTail.hpp"
#include "PreviewTailBridge.hpp"
#include "TempoSync.hpp"
#include "PreviewTempoBridge.hpp"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"

#include <atomic>
#include <cstring>

START_NAMESPACE_DISTRHO

using namespace stormsurge;

class PreviewPlugin final : public Plugin
{
  public:
    PreviewPlugin()
        : Plugin(isStandalone() ? StandaloneParameterCount : PreviewParameterCount, 0, 1),
          standalone(isStandalone())
    {
        for (std::uint32_t i = 0; i < PreviewParameterCount; ++i)
            values[i].store(previewSpecs[i].initial, std::memory_order_relaxed);
        prepare(getSampleRate());
    }

    void hostTempo(bool valid, double bpm) noexcept
    {
        // Raw adapter context arrives even for zero-frame/stopped processing calls.
        rawTempoValid = valid;
        rawBpm = bpm;
        updateTiming();
    }

    std::uint32_t tailSamples() const noexcept
    {
        if (!supportedRate)
            return 0;
        PreviewTailSettings settings;
        const double timeMs = values[Sync].load(std::memory_order_relaxed) >= 0.5f
                                  ? values[EffectiveTime].load(std::memory_order_relaxed)
                                  : values[Time].load(std::memory_order_relaxed);
        settings.delaySeconds = static_cast<double>(timeMs) / 1000.0;
        settings.feedback = values[Feedback].load(std::memory_order_relaxed) / 100.0;
        settings.mix = values[Mix].load(std::memory_order_relaxed) / 100.0;
        settings.bypass = values[Bypass].load(std::memory_order_relaxed) >= 0.5f;
        settings.reverbSeconds = values[ReverbSpace].load(std::memory_order_relaxed);
        settings.reverbAmount = values[ReverbAmount].load(std::memory_order_relaxed) / 100.0;
        return previewTailSamples(getSampleRate(), settings);
    }

  protected:
    const char *getLabel() const override { return "undertow_preview"; }
    const char *getDescription() const override
    {
        return "Early dark delay with stereo ping-pong, tempo sync and optional wet-path reverb.";
    }
    const char *getMaker() const override { return "StormSurge FX contributors"; }
    const char *getHomePage() const override { return "https://github.com/marrabld/stormsurge-fx"; }
    const char *getLicense() const override { return "GPL-3.0-only"; }
    uint32_t getVersion() const override { return d_version(0, 4, 0); }

    void initState(uint32_t, State &state) override
    {
        // DPF saves input parameters; this marker versions the surrounding schema.
        // Neither delay contents nor monitor consent belongs in persistent state.
        state.key = "undertow_preview_schema";
        state.defaultValue = "1";
    }
    void setState(const char *, const char *) override {}

    void initAudioPort(bool input, uint32_t index, AudioPort &port) override
    {
        Plugin::initAudioPort(input, index, port);
        port.groupId = kPortGroupStereo;
    }

    void initParameter(uint32_t index, Parameter &p) override
    {
        const int effect = previewLogicalIndex(index, standalone);
        if (effect >= 0) {
            const auto &spec = previewSpecs[effect];
            p.name = spec.name;
            p.symbol = spec.symbol;
            p.unit = spec.unit;
            p.ranges = ParameterRanges(spec.initial, spec.minimum, spec.maximum);
            p.hints = spec.readOnly ? kParameterIsOutput : kParameterIsAutomatable;
            if (spec.logarithmic)
                p.hints |= kParameterIsLogarithmic;
            if (effect == Bypass || effect == PingPong || effect == Sync || effect == SyncLimited)
                p.hints |= kParameterIsBoolean | kParameterIsInteger;
            if (effect == Division || effect == TempoSource)
                p.hints |= kParameterIsInteger;
            if (effect == Division) {
                p.enumValues.count = static_cast<uint8_t>(noteDivisions.size());
                p.enumValues.restrictedMode = true;
                p.enumValues.values = new ParameterEnumerationValue[noteDivisions.size()];
                for (std::size_t i = 0; i < noteDivisions.size(); ++i) {
                    p.enumValues.values[i].value = static_cast<float>(i);
                    p.enumValues.values[i].label = noteDivisions[i].label;
                }
            }
            // This is effect bypass with spillover, not the host's hard bypass.
            return;
        }
        p.name = index == MonitorRequest ? "Monitor request (not saved)" : "Monitor active";
        p.symbol = index == MonitorRequest ? "monitor_request" : "monitor_active";
        p.ranges = ParameterRanges(0, 0, 1);
        p.hints = kParameterIsBoolean;
        if (index == MonitorActive)
            p.hints |= kParameterIsOutput;
    }

    float getParameterValue(uint32_t index) const override
    {
        const int effect = previewLogicalIndex(index, standalone);
        if (effect >= 0)
            return values[effect].load(std::memory_order_relaxed);
        // Never serialize an enabled request. Actual status has its own output.
        return index == MonitorActive && monitoring.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        const int effect = previewEffectIndex(index, standalone);
        if (effect >= 0) {
            values[effect].store(sanitizePreview(effect, value), std::memory_order_relaxed);
        } else if (standalone && index == MonitorRequest) {
            const bool enabled = std::isfinite(value) && value >= 0.5f;
            if (!enabled)
                clearRequested.store(true, std::memory_order_relaxed);
            monitoring.store(enabled, std::memory_order_relaxed);
        }
    }

    void activate() override
    {
        monitoring.store(false, std::memory_order_relaxed);
        clearRequested.store(false, std::memory_order_relaxed);
        if (supportedRate) {
            delay.setParameters(parameters());
            delay.reset();
            reverb.setParameters(reverbParameters());
            reverb.reset();
        }
    }

    void sampleRateChanged(double rate) override
    {
        // DPF calls this only while deactivated. Never resize in run().
        monitoring.store(false, std::memory_order_relaxed);
        prepare(rate);
    }

    void run(const float **inputs, float **outputs, uint32_t frames) override
    {
        if (frames == 0)
            return;
        if (supportedRate) {
            delay.setParameters(parameters());
            reverb.setParameters(reverbParameters());
            if (clearRequested.exchange(false, std::memory_order_relaxed)) {
                delay.reset();
                reverb.reset();
            }
        }
        if (!supportedRate || (standalone && !monitoring.load(std::memory_order_relaxed))) {
            for (std::uint32_t channel = 0; channel < 2; ++channel)
                std::fill_n(outputs[channel], frames, 0.0f);
            return;
        }
        delay.process(inputs, outputs, frames, wetReverb, &reverb);
    }

  private:
    static bool isStandalone() noexcept
    {
        return std::strcmp(getPluginFormatName(), "JACK/Standalone") == 0;
    }

    TempoTiming updateTiming() noexcept
    {
        const auto timing =
            tempo.resolve(standalone, rawTempoValid, rawBpm,
                          values[InternalTempo].load(std::memory_order_relaxed),
                          values[Sync].load(std::memory_order_relaxed) >= 0.5f,
                          values[Time].load(std::memory_order_relaxed),
                          static_cast<int>(values[Division].load(std::memory_order_relaxed)));
        values[EffectiveTempo].store(static_cast<float>(timing.bpm), std::memory_order_relaxed);
        values[TempoSource].store(static_cast<float>(timing.origin), std::memory_order_relaxed);
        values[EffectiveTime].store(static_cast<float>(timing.timeMs), std::memory_order_relaxed);
        values[SyncLimited].store(timing.limited ? 1.0f : 0.0f, std::memory_order_relaxed);
        return timing;
    }

    DelayParameters parameters() noexcept
    {
        DelayParameters p;
        p.timeMs = updateTiming().timeMs;
        p.feedback = getParameterValue(Feedback) / 100.0;
        p.mix = getParameterValue(Mix) / 100.0;
        p.toneHz = getParameterValue(Tone);
        p.outputGain = getParameterValue(Level);
        p.bypass = getParameterValue(Bypass) >= 0.5f;
        p.pingPong = getParameterValue(previewHostIndex(PingPong, standalone)) >= 0.5f;
        return p;
    }

    void prepare(double rate)
    {
        supportedRate = std::isfinite(rate) && rate >= 8000 && rate <= 192000;
        if (supportedRate) {
            delay.prepare(rate, parameters(), 8000);
            reverb.prepare(rate, reverbParameters());
        }
    }

    ReverbParameters reverbParameters() const noexcept
    {
        return {getParameterValue(previewHostIndex(ReverbSpace, standalone)),
                getParameterValue(previewHostIndex(ReverbDarkness, standalone)),
                getParameterValue(previewHostIndex(ReverbAmount, standalone)) / 100.0};
    }

    static std::array<double, 2> wetReverb(void *context, const std::array<double, 2> &wet) noexcept
    {
        return static_cast<DarkReverb *>(context)->processSample(wet);
    }

    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    const bool standalone;
    std::array<std::atomic<float>, PreviewParameterCount> values;
    std::atomic<bool> monitoring{false};
    std::atomic<bool> clearRequested{false};
    bool supportedRate = false;
    bool rawTempoValid = false;
    double rawBpm = 0;
    TempoSync tempo;
    DarkDelay delay;
    DarkReverb reverb;
};

Plugin *createPlugin()
{
    return new PreviewPlugin;
}

END_NAMESPACE_DISTRHO

void stormsurge::setPreviewHostTempo(void *instance, bool valid, double bpm) noexcept
{
    static_cast<DISTRHO_NAMESPACE::PreviewPlugin *>(instance)->hostTempo(valid, bpm);
}

std::uint32_t stormsurge::previewTailSamples(void *instance) noexcept
{
    return static_cast<DISTRHO_NAMESPACE::PreviewPlugin *>(instance)->tailSamples();
}
