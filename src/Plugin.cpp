// SPDX-License-Identifier: GPL-3.0-only
#include "AudioPath.hpp"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"

#include <atomic>
#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

static bool standaloneFormat() noexcept
{
    return std::strcmp(getPluginFormatName(), "JACK/Standalone") == 0;
}

class FoundationPlugin final : public Plugin
{
  public:
    FoundationPlugin() : Plugin(standaloneFormat() ? 2 : 0, 0, 1), standalone(standaloneFormat()) {}

  protected:
    const char *getLabel() const override { return "undertow_foundation"; }
    const char *getDescription() const override
    {
        return "Build foundation only: stereo pass-through. No delay or tone processing.";
    }
    const char *getMaker() const override { return "StormSurge FX contributors"; }
    const char *getHomePage() const override { return "https://github.com/marrabld/stormsurge-fx"; }
    const char *getLicense() const override { return "GPL-3.0-only"; }
    uint32_t getVersion() const override { return d_version(0, 0, 1); }

    void initState(uint32_t, State &state) override
    {
        // Give hosts a nonempty state record even though there are no effect
        // parameters yet. No monitor consent or audio-buffer contents are saved.
        state.key = "foundation_schema";
        state.defaultValue = "1";
    }
    void setState(const char *, const char *) override {}

    void initAudioPort(bool input, uint32_t index, AudioPort &port) override
    {
        Plugin::initAudioPort(input, index, port);
        port.groupId = kPortGroupStereo;
    }

    // The monitor gate belongs only to standalone. The VST3 exposes no pretend
    // delay parameters and always returns unchanged samples, including over-range.
    void initParameter(uint32_t index, Parameter &parameter) override
    {
        parameter.name = "Standalone monitor";
        parameter.symbol = "standalone_monitor";
        parameter.hints = kParameterIsBoolean;
        parameter.ranges = ParameterRanges(0.0f, 0.0f, 1.0f);
        if (index == 1) {
            parameter.name = "Monitor active";
            parameter.symbol = "monitor_active";
            parameter.hints |= kParameterIsOutput;
        }
    }
    float getParameterValue(uint32_t) const override
    {
        return monitoring.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    }
    void setParameterValue(uint32_t index, float value) override
    {
        if (index == 0)
            monitoring.store(std::isfinite(value) && value >= 0.5f, std::memory_order_relaxed);
    }
    void activate() override
    {
        // A new/restarted standalone audio stream always needs fresh consent.
        if (standalone)
            monitoring.store(false, std::memory_order_relaxed);
    }
    void run(const float **inputs, float **outputs, uint32_t frames) override
    {
        stormsurge::passThrough(inputs, outputs, frames,
                                !standalone || monitoring.load(std::memory_order_relaxed));
    }

  private:
    static_assert(std::atomic<bool>::is_always_lock_free);
    const bool standalone;
    std::atomic<bool> monitoring{false};
};

Plugin *createPlugin()
{
    return new FoundationPlugin;
}

END_NAMESPACE_DISTRHO
