// SPDX-License-Identifier: GPL-3.0-only
#include "Squall.hpp"
#include "SquallParameters.hpp"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace stormsurge;

class SquallPlugin final : public Plugin
{
  public:
    SquallPlugin() : Plugin(SquallPluginParameterCount, 0, 1)
    {
        for (std::uint32_t i = 0; i < SquallPluginParameterCount; ++i)
            values[i].store(squallSpecs[i].initial, std::memory_order_relaxed);
        prepare(getSampleRate());
    }

  protected:
    const char *getLabel() const override { return "squall"; }
    const char *getDescription() const override { return "Clean and controlled heavy vocal tones"; }
    const char *getMaker() const override { return "StormSurge FX contributors"; }
    const char *getHomePage() const override { return "https://github.com/marrabld/stormsurge-fx"; }
    const char *getLicense() const override { return "GPL-3.0-only"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initState(uint32_t, State &state) override
    {
        state.key = "squall_schema";
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
        const auto &spec = squallSpecs[index];
        parameter.name = spec.name;
        parameter.symbol = spec.symbol;
        parameter.unit = spec.unit;
        parameter.ranges = ParameterRanges(spec.initial, spec.minimum, spec.maximum);
        parameter.hints = kParameterIsAutomatable | kParameterIsBoolean | kParameterIsInteger;
    }

    float getParameterValue(uint32_t index) const override
    {
        return values[index].load(std::memory_order_relaxed);
    }

    void setParameterValue(uint32_t index, float value) override
    {
        values[index].store(sanitizeSquallParameter(index, value), std::memory_order_relaxed);
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
        effect.process(inputs, outputs, frames, 2);
    }

  private:
    SquallParameters parameters() const noexcept
    {
        return {getParameterValue(SquallModeParameter) >= 0.5f,
                getParameterValue(SquallBypassParameter) >= 0.5f};
    }

    void prepare(double rate)
    {
        supportedRate = std::isfinite(rate) && rate >= 8000.0 && rate <= 192000.0;
        if (!supportedRate)
            return;
        effect.prepare(rate, parameters());
        // The DSP uses no pre-delay. The short reverb tail is not latency.
    }

    static_assert(std::atomic<float>::is_always_lock_free);
    std::array<std::atomic<float>, SquallPluginParameterCount> values;
    Squall effect;
    bool supportedRate = false;
};

Plugin *createPlugin()
{
    return new SquallPlugin;
}

END_NAMESPACE_DISTRHO
