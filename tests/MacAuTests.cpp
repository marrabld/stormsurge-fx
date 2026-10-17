// SPDX-License-Identifier: GPL-3.0-only
#include "PreviewParameters.hpp"
#include "TempoSync.hpp"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace stormsurge;
namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void checked(OSStatus status, const char *message)
{
    if (status != noErr) {
        std::fprintf(stderr, "%s: OSStatus %d\n", message, static_cast<int>(status));
        throw std::runtime_error(message);
    }
}
constexpr OSType code(const char *text)
{
    return static_cast<OSType>(text[0]) << 24 | static_cast<OSType>(text[1]) << 16 |
           static_cast<OSType>(text[2]) << 8 | static_cast<OSType>(text[3]);
}
struct Host {
    bool available = false, impulse = false;
    double bpm = 120;
};
OSStatus tempo(void *data, Float64 *beat, Float64 *bpm)
{
    const auto &host = *static_cast<Host *>(data);
    *beat = 0;
    *bpm = host.bpm;
    return host.available ? noErr : kAudio_ParamError;
}
OSStatus input(void *data, AudioUnitRenderActionFlags *, const AudioTimeStamp *, UInt32,
               UInt32 frames, AudioBufferList *buffers)
{
    auto &host = *static_cast<Host *>(data);
    for (UInt32 channel = 0; channel < buffers->mNumberBuffers; ++channel) {
        auto *samples = static_cast<float *>(buffers->mBuffers[channel].mData);
        if (samples == nullptr)
            return kAudio_ParamError;
        std::fill_n(samples, frames, 0.0f);
        if (host.impulse && frames != 0 && channel == 0)
            samples[0] = 1;
    }
    host.impulse = false;
    return noErr;
}
struct Bundle {
    CFBundleRef value = nullptr;
    explicit Bundle(const char *path)
    {
        auto url =
            CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8 *>(path),
                                                    static_cast<CFIndex>(std::strlen(path)), true);
        require(url != nullptr, "Cannot create AU bundle URL");
        value = CFBundleCreate(nullptr, url);
        CFRelease(url);
        require(value != nullptr && CFBundleLoadExecutable(value),
                "Cannot load AU bundle executable");
    }
    ~Bundle()
    {
        if (value != nullptr)
            CFRelease(value);
    }
};
struct Unit {
    AudioUnit value = nullptr;
    Host host;
    double position = 0;
    explicit Unit(AudioComponent component)
    {
        checked(AudioComponentInstanceNew(component, &value), "Create AU instance");
        AudioStreamBasicDescription format{};
        format.mSampleRate = 48000;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
        format.mBytesPerPacket = format.mBytesPerFrame = sizeof(float);
        format.mFramesPerPacket = 1;
        format.mChannelsPerFrame = 2;
        format.mBitsPerChannel = 32;
        for (auto scope : {kAudioUnitScope_Input, kAudioUnitScope_Output})
            checked(AudioUnitSetProperty(value, kAudioUnitProperty_StreamFormat, scope, 0, &format,
                                         sizeof(format)),
                    "Set stereo planar format");
        UInt32 maximum = 512;
        checked(AudioUnitSetProperty(value, kAudioUnitProperty_MaximumFramesPerSlice,
                                     kAudioUnitScope_Global, 0, &maximum, sizeof(maximum)),
                "Set maximum render frames");
        AURenderCallbackStruct source{input, &host};
        checked(AudioUnitSetProperty(value, kAudioUnitProperty_SetRenderCallback,
                                     kAudioUnitScope_Input, 0, &source, sizeof(source)),
                "Set input callback");
        callbacks(true);
        set(Mix, 100);
        set(Feedback, 0);
        set(Sync, 1);
        set(InternalTempo, 90);
        checked(AudioUnitInitialize(value), "Initialize AU without an audio device");
    }
    ~Unit()
    {
        if (value != nullptr) {
            AudioUnitUninitialize(value);
            AudioComponentInstanceDispose(value);
        }
    }
    void callbacks(bool present, bool prefix = false)
    {
        HostCallbackInfo callbacks{};
        callbacks.hostUserData = &host;
        callbacks.beatAndTempoProc = present ? tempo : nullptr;
        const UInt32 bytes =
            prefix ? offsetof(HostCallbackInfo, musicalTimeLocationProc) : sizeof(callbacks);
        checked(AudioUnitSetProperty(value, kAudioUnitProperty_HostCallbacks,
                                     kAudioUnitScope_Global, 0, &callbacks, bytes),
                "Set/remove AU host callbacks");
    }
    void set(UInt32 id, float number)
    {
        checked(AudioUnitSetParameter(value, id, kAudioUnitScope_Global, 0, number, 0),
                "Set AU parameter");
    }
    float get(UInt32 id)
    {
        float number = 0;
        checked(AudioUnitGetParameter(value, id, kAudioUnitScope_Global, 0, &number),
                "Get AU parameter");
        return number;
    }
    std::array<std::array<float, 512>, 2> render()
    {
        std::array<std::array<float, 512>, 2> output{};
        struct StereoBuffers {
            UInt32 count;
            AudioBuffer channels[2];
        } buffers{2,
                  {{1, 512 * sizeof(float), output[0].data()},
                   {1, 512 * sizeof(float), output[1].data()}}};
        AudioTimeStamp timestamp{};
        timestamp.mFlags = kAudioTimeStampSampleTimeValid;
        timestamp.mSampleTime = position;
        AudioUnitRenderActionFlags flags = 0;
        checked(AudioUnitRender(value, &flags, &timestamp, 0, 512,
                                reinterpret_cast<AudioBufferList *>(&buffers)),
                "Render AU supplied buffers");
        position += 512;
        return output;
    }
};
} // namespace

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Supply the built .component directory");
        Bundle bundle(argv[1]);
        auto factory = reinterpret_cast<AudioComponentFactoryFunction>(
            CFBundleGetFunctionPointerForName(bundle.value, CFSTR("PluginAUFactory")));
        require(factory != nullptr, "Missing AU factory export");
        AudioComponentDescription description{code("aufx"), code("UtP0"), code("SgFx"), 0, 0};
        // Process-local registration only. No plugin directory, cache or device
        // is touched, and this does not substitute for a DAW scan or auval.
        auto component =
            AudioComponentRegister(&description, CFSTR("Undertow Preview test"), 0x000400, factory);
        require(component != nullptr, "Cannot register AU in this test process");
        Unit unit(component);
        unit.render();
        require(unit.get(TempoSource) == static_cast<float>(TempoOrigin::Fallback) &&
                    std::abs(unit.get(EffectiveTime) - 666.6667f) < 0.01f,
                "AU initial fallback differs from shared policy");
        unit.host.available = true;
        unit.callbacks(true, true);
        unit.render();
        require(unit.get(TempoSource) == static_cast<float>(TempoOrigin::Host) &&
                    unit.get(EffectiveTime) == 500,
                "AU did not follow BPM without a playing-transport callback");
        unit.host.bpm = 100;
        unit.render();
        require(unit.get(EffectiveTime) == 600, "AU host tempo change did not reach the processor");
        unit.host.available = false;
        unit.render();
        require(unit.get(TempoSource) == static_cast<float>(TempoOrigin::Held) &&
                    unit.get(EffectiveTempo) == 100,
                "AU callback failure replaced held tempo");
        unit.host.available = true;
        unit.host.bpm = std::numeric_limits<double>::quiet_NaN();
        unit.render();
        require(unit.get(EffectiveTempo) == 100, "Invalid AU BPM replaced held tempo");
        unit.callbacks(false);
        unit.render();
        require(unit.get(TempoSource) == static_cast<float>(TempoOrigin::Held),
                "Removing all AU callbacks retained a stale HOST readout");
        unit.callbacks(true);
        unit.host.bpm = 120;
        unit.render();
        checked(AudioUnitReset(unit.value, kAudioUnitScope_Global, 0), "Reset AU delay");
        unit.host.impulse = true;
        int firstEcho = -1;
        double energy = 0;
        for (int block = 0; block < 50; ++block) {
            const auto output = unit.render();
            for (int sample = 0; sample < 512; ++sample) {
                require(std::isfinite(output[0][sample]) && output[1][sample] == 0,
                        "AU stereo routing or finite-output contract failed");
                if (firstEcho < 0 && output[0][sample] != 0)
                    firstEcho = block * 512 + sample;
                energy += output[0][sample] * output[0][sample];
            }
        }
        require(firstEcho == 24000 && energy > 0, "AU did not render the actual 500 ms wet delay");

        unit.set(Division, 7);
        unit.set(ReverbAmount, 44);
        CFPropertyListRef state = nullptr;
        UInt32 bytes = sizeof(state);
        checked(AudioUnitGetProperty(unit.value, kAudioUnitProperty_ClassInfo,
                                     kAudioUnitScope_Global, 0, &state, &bytes),
                "Read AU saved state");
        require(state != nullptr, "AU returned no saved state");
        Unit restored(component);
        const auto restoredStatus =
            AudioUnitSetProperty(restored.value, kAudioUnitProperty_ClassInfo,
                                 kAudioUnitScope_Global, 0, &state, sizeof(state));
        CFRelease(state);
        checked(restoredStatus, "Restore AU saved state");
        require(restored.get(Division) == 7 && restored.get(ReverbAmount) == 44,
                "AU did not restore Sync/Reverb input parameters");
        restored.render();
        require(restored.get(TempoSource) == static_cast<float>(TempoOrigin::Fallback),
                "AU restored another instance's host-clock history");
        std::puts("PASS: loaded AU, raw clock/fallback/hold, rendered wet stereo delay and saved "
                  "state without installation/devices");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
