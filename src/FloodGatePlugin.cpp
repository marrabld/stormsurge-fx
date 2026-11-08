// SPDX-License-Identifier: GPL-3.0-only
#include "FloodGate.hpp"
#include "FloodGateParameters.hpp"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace stormsurge;

class FloodGatePlugin final : public Plugin
{
  public:
    FloodGatePlugin() : Plugin(FloodGateParameterCount, 0, 1)
    {
        for (std::uint32_t i = 0; i < FloodGateParameterCount; ++i)
            values[i].store(floodGateSpecs[i].initial, std::memory_order_relaxed);
        prepare(getSampleRate());
    }

  protected:
    const char *getLabel() const override { return "floodgate"; }
    const char *getDescription() const override
    {
        return "Stereo-linked noise gate that holds back hiss between phrases";
    }
    const char *getMaker() const override { return "StormSurge FX contributors"; }
    const char *getHomePage() const override { return "https://github.com/marrabld/stormsurge-fx"; }
    const char *getLicense() const override { return "GPL-3.0-only"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initState(uint32_t, State &state) override
    {
        // DPF saves input parameters; this marker versions the surrounding schema.
        // Envelope and hold state never belong in persistent state.
        state.key = "floodgate_schema";
        state.defaultValue = "1";
    }
    void setState(const char *, const char *) override {}

    void initAudioPort(bool input, uint32_t index, AudioPort &port) override
    {
        Plugin::initAudioPort(input, index, port);
        port.groupId = kPortGroupStereo;
    }

    void initParameter(uint32_t index, Parameter &parameter) override
    {
        const auto &spec = floodGateSpecs[index];
        parameter.name = spec.name;
        parameter.symbol = spec.symbol;
        parameter.unit = spec.unit;
        parameter.ranges = ParameterRanges(spec.initial, spec.minimum, spec.maximum);
        parameter.hints = kParameterIsAutomatable;
        if (spec.logarithmic)
            parameter.hints |= kParameterIsLogarithmic;
        if (index == FloodGateBypass)
            parameter.hints |= kParameterIsBoolean | kParameterIsInteger;
    }

    float getParameterValue(uint32_t index) const override
    {
        return values[index].load(std::memory_order_relaxed);
    }

    void setParameterValue(uint32_t index, float value) override
    {
        values[index].store(sanitizeFloodGate(index, value), std::memory_order_relaxed);
    }

    void activate() override
    {
        if (supportedRate) {
            gate.setParameters(parameters());
            gate.reset();
        }
    }

    // DPF calls this only while deactivated, so preparation stays off the processing callback.
    void sampleRateChanged(double rate) override { prepare(rate); }

    void run(const float **inputs, float **outputs, uint32_t frames) override
    {
        if (frames == 0)
            return;
        if (!supportedRate) {
            for (uint32_t channel = 0; channel < 2; ++channel)
                std::fill_n(outputs[channel], frames, 0.0f);
            return;
        }
        gate.setParameters(parameters());
        for (uint32_t frame = 0; frame < frames; ++frame) {
            // Bypass always goes through processSample(): its engage ramp fades to exact dry, where
            // switching to bypassSample() would cut a half-closed gate open in one sample.
            const auto out = gate.processSample({inputs[0][frame], inputs[1][frame]});
            outputs[0][frame] = static_cast<float>(out[0]);
            outputs[1][frame] = static_cast<float>(out[1]);
        }
    }

  private:
    GateParameters parameters() const noexcept
    {
        // Bypass is effect bypass only. It is not a monitor mute; a closed gate is not one either.
        return {getParameterValue(FloodGateThreshold), getParameterValue(FloodGateRelease),
                getParameterValue(FloodGateRange), getParameterValue(FloodGateBypass) < 0.5f};
    }

    void prepare(double rate)
    {
        supportedRate = std::isfinite(rate) && rate >= 8000.0 && rate <= 192000.0;
        if (!supportedRate)
            return;
        gate.prepare(rate, parameters());
        // No lookahead, so no latency is reported. The pinned DPF VST3 adapter hardcodes zero
        // tail samples, which is accurate here: a gate adds no tail of its own.
    }

    static_assert(std::atomic<float>::is_always_lock_free);
    std::array<std::atomic<float>, FloodGateParameterCount> values;
    FloodGate gate;
    bool supportedRate = false;
};

Plugin *createPlugin()
{
    return new FloodGatePlugin;
}

END_NAMESPACE_DISTRHO
