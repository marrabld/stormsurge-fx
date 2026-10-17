// SPDX-License-Identifier: GPL-3.0-only
#include "MaelstromOverdrive.hpp"
#include "MaelstromParameters.hpp"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"

#include <atomic>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace stormsurge;

class MaelstromPlugin final : public Plugin
{
  public:
    MaelstromPlugin() : Plugin(MaelstromParameterCount, 0, 1)
    {
        for (std::uint32_t i = 0; i < MaelstromParameterCount; ++i)
            values[i].store(maelstromSpecs[i].initial, std::memory_order_relaxed);
        prepare(getSampleRate());
    }

  protected:
    const char *getLabel() const override { return "maelstrom"; }
    const char *getDescription() const override
    {
        return "Tightening overdrive for tight melodic-death rhythm. Feed an amp model after it.";
    }
    const char *getMaker() const override { return "StormSurge FX contributors"; }
    const char *getHomePage() const override { return "https://github.com/marrabld/stormsurge-fx"; }
    const char *getLicense() const override { return "GPL-3.0-only"; }
    uint32_t getVersion() const override { return d_version(0, 4, 0); }

    void initState(uint32_t, State &state) override
    {
        // DPF saves input parameters; this marker versions the surrounding schema.
        // No filter state or buffer contents belong in persistent state.
        state.key = "maelstrom_schema";
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
        const auto &spec = maelstromSpecs[index];
        p.name = spec.name;
        p.symbol = spec.symbol;
        p.unit = spec.unit;
        p.ranges = ParameterRanges(spec.initial, spec.minimum, spec.maximum);
        p.hints = kParameterIsAutomatable;
        if (index == MaelstromBypass)
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
    }

    float getParameterValue(uint32_t index) const override
    {
        return values[index].load(std::memory_order_relaxed);
    }

    void setParameterValue(uint32_t index, float value) override
    {
        values[index].store(sanitizeMaelstrom(index, value), std::memory_order_relaxed);
    }

    void activate() override
    {
        if (supportedRate) {
            overdrive.setParameters(parameters());
            overdrive.reset();
        }
    }

    void sampleRateChanged(double rate) override
    {
        // DPF calls this only while deactivated, so buffer work stays off the
        // processing callback.
        prepare(rate);
    }

    void run(const float **inputs, float **outputs, uint32_t frames) override
    {
        if (frames == 0)
            return;
        if (!supportedRate) {
            for (uint32_t channel = 0; channel < 2; ++channel)
                std::fill_n(outputs[channel], frames, 0.0f);
            return;
        }
        overdrive.setParameters(parameters());
        const bool enabled = getParameterValue(MaelstromBypass) < 0.5f;
        for (uint32_t frame = 0; frame < frames; ++frame) {
            const std::array<double, 2> input{inputs[0][frame], inputs[1][frame]};
            // Bypass shares the wet path's engage fade, so the two never differ
            // in latency and switching cannot click.
            const auto out =
                enabled ? overdrive.processSample(input) : overdrive.bypassSample(input);
            outputs[0][frame] = static_cast<float>(out[0]);
            outputs[1][frame] = static_cast<float>(out[1]);
        }
    }

  private:
    OverdriveParameters parameters() const noexcept
    {
        return {getParameterValue(Drive) / 100.0, getParameterValue(Tight) / 100.0,
                getParameterValue(Bite) / 100.0, getParameterValue(MaelstromLevel),
                getParameterValue(MaelstromBypass) < 0.5f};
    }

    void prepare(double rate)
    {
        supportedRate = std::isfinite(rate) && rate >= 8000 && rate <= 192000;
        if (!supportedRate)
            return;
        overdrive.prepare(rate, parameters());
        // Oversampling adds a fixed one-sample delay. Reporting it keeps host
        // compensation honest instead of leaving the offset undocumented.
        setLatency(overdrive.latencySamples());
    }

    static_assert(std::atomic<float>::is_always_lock_free);
    std::array<std::atomic<float>, MaelstromParameterCount> values;
    MaelstromOverdrive overdrive;
    bool supportedRate = false;
};

Plugin *createPlugin()
{
    return new MaelstromPlugin;
}

END_NAMESPACE_DISTRHO
