// SPDX-License-Identifier: GPL-3.0-only
#include "Riptide.hpp"
#include "RiptideParameters.hpp"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace stormsurge;

class RiptidePlugin final : public Plugin
{
  public:
    RiptidePlugin() : Plugin(RiptidePluginParameterCount, 0, 1)
    {
        for (std::uint32_t i = 0; i < RiptidePluginParameterCount; ++i)
            values[i].store(riptideSpecs[i].initial, std::memory_order_relaxed);
        prepare(getSampleRate());
    }

  protected:
    const char *getLabel() const override { return "riptide"; }
    const char *getDescription() const override
    {
        return "Drop tuning and a blended lower octave layer";
    }
    const char *getMaker() const override { return "StormSurge FX contributors"; }
    const char *getHomePage() const override { return "https://github.com/marrabld/stormsurge-fx"; }
    const char *getLicense() const override { return "GPL-3.0-only"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initState(uint32_t, State &state) override
    {
        state.key = "riptide_schema";
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
        const auto &spec = riptideSpecs[index];
        parameter.name = spec.name;
        parameter.symbol = spec.symbol;
        parameter.unit = spec.unit;
        parameter.ranges = ParameterRanges(spec.initial, spec.minimum, spec.maximum);
        parameter.hints = kParameterIsAutomatable;
        if (index == RiptideShiftParameter)
            parameter.hints |= kParameterIsInteger;
        else if (index == RiptideBypassParameter)
            parameter.hints |= kParameterIsBoolean | kParameterIsInteger;
    }

    float getParameterValue(uint32_t index) const override
    {
        return values[index].load(std::memory_order_relaxed);
    }

    void setParameterValue(uint32_t index, float value) override
    {
        values[index].store(sanitizeRiptideParameter(index, value), std::memory_order_relaxed);
    }

    void activate() override
    {
        if (supportedRate) {
            effect.setParameters(parameters());
            effect.reset();
        }
    }

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
        effect.setParameters(parameters());
        effect.process(inputs, outputs, frames);
    }

  private:
    RiptideParameters parameters() const noexcept
    {
        // Bypass dissolves the shifted layer through the same 20 ms blend ramp the
        // Blend control uses. The read heads keep running, so leaving bypass is a
        // fade back in rather than a discontinuity. This is not the monitor mute:
        // the practice board's live input gate remains a separate mechanism.
        const bool bypassed = getParameterValue(RiptideBypassParameter) >= 0.5f;
        return {static_cast<double>(getParameterValue(RiptideShiftParameter)),
                bypassed ? 0.0
                         : static_cast<double>(getParameterValue(RiptideBlendParameter)) / 100.0};
    }

    void prepare(double rate)
    {
        supportedRate = std::isfinite(rate) && rate >= 8000.0 && rate <= 192000.0;
        if (!supportedRate)
            return;
        effect.prepare(rate, 2, parameters());
        // The read heads look back at most about 41 ms. That is the effect's own
        // grain window, not host latency, so no latency offset is reported. The
        // pinned DPF VST3 adapter hardcodes zero tail samples; do not claim a tail.
    }

    static_assert(std::atomic<float>::is_always_lock_free);
    std::array<std::atomic<float>, RiptidePluginParameterCount> values;
    Riptide effect;
    bool supportedRate = false;
};

Plugin *createPlugin()
{
    return new RiptidePlugin;
}

END_NAMESPACE_DISTRHO
