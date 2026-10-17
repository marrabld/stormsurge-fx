// SPDX-License-Identifier: GPL-3.0-only
#include "MaelstromOverdrive.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

using namespace stormsurge;

static bool track = false;
static std::size_t allocations = 0;
void *operator new(std::size_t size)
{
    if (track)
        ++allocations;
    if (void *p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    std::free(p);
}

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// Goertzel magnitude, so a test can ask "how much energy landed at this exact
// frequency" without pulling in a spectrum library.
static double tone(const std::vector<double> &signal, double frequency, double rate)
{
    const double w = 2 * std::numbers::pi_v<double> * frequency / rate;
    const double c = 2 * std::cos(w);
    double previous = 0, beforePrevious = 0;
    for (double sample : signal) {
        const double next = sample + c * previous - beforePrevious;
        beforePrevious = previous;
        previous = next;
    }
    const double power =
        beforePrevious * beforePrevious + previous * previous - c * previous * beforePrevious;
    return 2 * std::sqrt(std::max(0.0, power)) / static_cast<double>(signal.size());
}

static std::vector<double> mono(const std::vector<std::array<double, 2>> &input)
{
    std::vector<double> left;
    left.reserve(input.size());
    for (const auto &frame : input)
        left.push_back(frame[0]);
    return left;
}

static std::vector<double> render(MaelstromOverdrive &overdrive,
                                  const std::vector<std::array<double, 2>> &input)
{
    std::vector<double> left;
    left.reserve(input.size());
    for (const auto &frame : input) {
        const auto out = overdrive.processSample(frame);
        left.push_back(out[0]);
    }
    return left;
}

// A steady tone, long enough that the control ramps have certainly settled.
static std::vector<std::array<double, 2>> sine(double rate, double frequency, double seconds,
                                               double amplitude = 0.2)
{
    const auto count = static_cast<std::size_t>(rate * seconds);
    std::vector<std::array<double, 2>> input(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double value = amplitude * std::sin(2 * std::numbers::pi_v<double> * frequency *
                                                  static_cast<double>(i) / rate);
        input[i] = {value, value};
    }
    return input;
}

static double energy(const std::vector<double> &signal)
{
    double sum = 0;
    for (double value : signal)
        sum += value * value;
    return sum;
}

static void test_defaults_and_sanitisation()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000);
    // A quiet tone at the approved defaults must stay close to unity. Low Drive
    // is meant to be usable as a near-clean push, not a hidden boost.
    const auto input = sine(48000, 440, 0.25, 0.05);
    const auto output = render(overdrive, input);
    require(std::abs(output.back() - input.back()[0]) < 0.02,
            "Default Drive 10% should not dramatically change a quiet signal");

    MaelstromOverdrive hostile;
    const OverdriveParameters nan{std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::infinity(), -5.0,
                                  std::numeric_limits<double>::quiet_NaN(), true};
    hostile.prepare(48000, nan);
    const auto quiet = render(hostile, sine(48000, 440, 0.2, 0.02));
    require(std::all_of(quiet.begin(), quiet.end(),
                        [](double v) { return std::isfinite(v) && std::abs(v) < 0.05; }),
            "Non-finite parameters must fall back to defaults, not to gain");

    MaelstromOverdrive extremes;
    extremes.prepare(48000, {1.0, 2.0, -3.0, 1e9, true});
    const auto loud = render(extremes, sine(48000, 220, 0.5, 0.5));
    require(std::all_of(loud.begin(), loud.end(), [](double v) { return std::isfinite(v); }),
            "Out-of-range but finite parameters must clamp to the control ranges");
    double peak = 0;
    for (double v : loud)
        peak = std::max(peak, std::abs(v));
    // +12 dB ceiling is about 4x, and tanh cannot run away on top of that.
    require(peak < 4.2, "Sustained input must stay inside the documented output ceiling");
}

static void test_bypass_is_unity()
{
    for (double level : {0.0, 1.0, 3.9810717055349722}) {
        MaelstromOverdrive overdrive;
        overdrive.prepare(48000, {1.0, 0.0, 1.0, level, false});
        const auto input = sine(48000, 330, 0.1, 0.3);
        for (const auto &frame : input) {
            const auto out = overdrive.processSample(frame);
            require(out[0] == overdrive.bypassSample(frame)[0],
                    "Fully bypassed output must be exactly the dry input");
        }
        // The hard-bypass path must ignore every control, including +12 dB.
        const auto direct = overdrive.bypassSample({0.5, -0.5});
        require(direct[0] == 0.5 && direct[1] == -0.5, "bypassSample must be unity dry");
    }
}

static void test_engage_fade_has_no_discontinuity()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {1.0, 0.0, 1.0, 1.0, true});
    const auto input = sine(48000, 220, 0.2, 0.4);
    for (const auto &frame : input)
        overdrive.processSample(frame);
    overdrive.setParameters({1.0, 0.0, 1.0, 1.0, false});
    double previous = overdrive.processSample(input.front())[0];
    for (const auto &frame : input) {
        const double value = overdrive.processSample(frame)[0];
        // The engage ramp is 10 ms; a hard switch would step far more than this.
        require(std::abs(value - previous) < 0.25, "Disabling must ramp, not jump straight to dry");
        previous = value;
    }
    for (std::size_t i = 0; i < 4800; ++i)
        previous = overdrive.processSample({0.0, 0.0})[0];
    require(previous == 0.0, "Settled bypass must be exactly silent for silent input");
}

static void test_channels_stay_independent()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {0.8, 0.2, 0.7, 1.0, true});
    const std::size_t count = 4800;
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / 48000;
        const auto out =
            overdrive.processSample({0.3 * std::sin(t * 700), 0.3 * std::sin(t * 700)});
        require(out[0] == out[1], "Identical channel inputs must produce identical outputs");
    }
    // A signal on one side must not bleed into the other.
    MaelstromOverdrive isolated;
    isolated.prepare(48000, {1.0, 0.0, 1.0, 1.0, true});
    double leakPeak = 0;
    for (std::size_t i = 0; i < 4800; ++i) {
        const double t = static_cast<double>(i) / 48000;
        const auto out = isolated.processSample({0.4 * std::sin(t * 900), 0.0});
        leakPeak = std::max(leakPeak, std::abs(out[1]));
    }
    require(leakPeak == 0.0, "Channels must not cross-feed");
}

static void test_tone_measurement_is_trustworthy()
{
    // If the measuring instrument is wrong, every other spectral assertion in
    // this file is meaningless. Check it against a known amplitude first.
    require(std::abs(tone(mono(sine(48000, 1000, 0.25, 0.05)), 1000, 48000) - 0.05) < 1e-6,
            "Goertzel must recover the amplitude of an exactly periodic tone");
    require(tone(mono(sine(48000, 1000, 0.25, 0.05)), 2000, 48000) < 1e-9,
            "Goertzel must reject a different frequency");
}

static void test_tight_reduces_bass_before_clipping()
{
    const auto measure = [](double tight, double frequency) {
        MaelstromOverdrive overdrive;
        overdrive.prepare(48000, {0.0, tight, 1.0, 1.0, true});
        const auto output = render(overdrive, sine(48000, frequency, 0.2, 0.05));
        return tone(output, frequency, 48000);
    };
    // Palm-muted low strings are the case Tight exists for. A treble-style
    // lowpass barely touches them, which is why this is a highpass.
    const double open = measure(0.0, 110), middle = measure(0.5, 110);
    const double tight = measure(1.0, 110);
    require(middle < open * 0.7, "The default Tight position must already tighten low strings");
    require(tight < middle * 0.2, "Maximum Tight must strongly reduce palm-muted bass");
    // The point is a focus shift, not an overall dulling: the attack and chord
    // definition range must survive while the bass goes.
    const double openPick = measure(0.0, 2000), tightPick = measure(1.0, 2000);
    require(tightPick > openPick * 0.6, "Tight must leave the pick range largely intact");
    require(tightPick / tight > (openPick / open) * 10,
            "Tight must shift the balance to the attack range, not just dull everything");
    // The default has to stay usable for a down-tuned low string.
    require(measure(0.5, 62) > measure(1.0, 62),
            "Maximum Tight should attenuate a down-tuned low fundamental more than the default");
}

static void test_drive_adds_harmonics_progressively()
{
    const auto harmonics = [](double drive) {
        MaelstromOverdrive overdrive;
        overdrive.prepare(48000, {drive, 0.0, 1.0, 1.0, true});
        const auto output = render(overdrive, sine(48000, 440, 0.3, 0.1));
        const double fundamental = tone(output, 440, 48000);
        const double third = tone(output, 1320, 48000);
        return third / fundamental;
    };
    const double soft = harmonics(0.1), medium = harmonics(0.5), hard = harmonics(1.0);
    require(medium > soft * 3, "Drive must add harmonic content as it increases");
    require(hard > medium, "High Drive must clip harder than medium Drive");
}

static void test_bite_shapes_generated_harmonics()
{
    // Odd harmonics only: a symmetric clipping curve cancels the even ones, so
    // measuring 8 kHz from a 1 kHz tone would compare noise against noise.
    const auto spectrum = [](double bite, double harmonic) {
        MaelstromOverdrive overdrive;
        overdrive.prepare(48000, {0.9, 0.0, bite, 1.0, true});
        const auto output = render(overdrive, sine(48000, 1000, 0.25, 0.1));
        return tone(output, harmonic, 48000);
    };
    const double dull = spectrum(0.0, 9000), sharp = spectrum(1.0, 9000);
    require(sharp > dull * 1.4, "Bite must raise the level of upper harmonics");
    // Bite is a tone control, not a bypass: the fizziest region must still be
    // quieter with it wide open than closed.
    require(spectrum(0.0, 21000) > spectrum(1.0, 21000),
            "Bite must still tame content near Nyquist");
}

static void test_level_is_a_pure_post_clip_gain()
{
    // Level sits after the clip stage, so it must scale the settled output
    // exactly rather than interacting with Drive. That is what makes 0 dB a
    // trustworthy unity reference for the output stage.
    const auto renderAt = [](double level) {
        MaelstromOverdrive overdrive;
        overdrive.prepare(48000, {0.6, 0.5, 0.5, level, true});
        return render(overdrive, sine(48000, 440, 0.2, 0.2));
    };
    const auto unityOut = renderAt(1.0);
    const auto boosted = renderAt(3.9810717055349722);
    for (std::size_t i = 0; i < unityOut.size(); ++i)
        require(std::abs(boosted[i] - unityOut[i] * 3.9810717055349722) < 1e-12,
                "Level must be an exact post-clip multiply");

    MaelstromOverdrive muted;
    muted.prepare(48000, {1.0, 0.0, 1.0, 0.0, true});
    for (const auto &frame : sine(48000, 440, 0.05, 0.4)) {
        const auto out = muted.processSample(frame);
        require(out[0] == 0.0, "Level at its floor must be digital silence, not a leak");
    }

    // The default Drive position pushes the amp on purpose, but it must stay a
    // gentle push. A large gain here would quietly change level on insertion.
    MaelstromOverdrive defaults;
    defaults.prepare(48000);
    double peak = 0;
    for (double v : render(defaults, sine(48000, 440, 0.3, 0.02)))
        peak = std::max(peak, std::abs(v));
    require(peak > 0.015 && peak < 0.03,
            "Default Drive should push a quiet signal only modestly at 0 dB Level");
}

static void test_nonfinite_input_fails_safe_and_recovers()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {1.0, 0.5, 0.5, 3.9810717055349722, true});
    for (const auto &frame : sine(48000, 220, 0.05, 0.4))
        overdrive.processSample(frame);
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto bad = overdrive.processSample({inf, nan});
    require(bad[0] == 0.0 && bad[1] == 0.0, "Non-finite input must become silence");
    const auto alsoBypassed = overdrive.bypassSample({inf, nan});
    require(alsoBypassed[0] == 0.0 && alsoBypassed[1] == 0.0,
            "Bypass must apply the same non-finite policy as the wet path");
    // State must be usable again immediately, not merely quiet for one sample.
    const auto output = render(overdrive, sine(48000, 440, 0.1, 0.1));
    require(std::all_of(output.begin(), output.end(), [](double v) { return std::isfinite(v); }),
            "Processing must recover after non-finite input");
    double peak = 0;
    for (double v : output)
        peak = std::max(peak, std::abs(v));
    // Audible again, and still inside the documented +12 dB output ceiling
    // rather than running away.
    require(peak > 0.01 && peak <= 4.0,
            "Recovered output must be musical and still bounded by the output ceiling");
}

static void test_overload_recovers_to_silence()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {1.0, 0.5, 0.5, 3.9810717055349722, true});
    const double huge = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < 16; ++i) {
        const auto out = overdrive.processSample({huge, -huge});
        require(std::isfinite(out[0]) && std::isfinite(out[1]),
                "Extreme finite input must not emit non-finite output");
    }
    const auto quiet = render(overdrive, sine(48000, 440, 0.05, 0.05));
    require(std::all_of(quiet.begin(), quiet.end(), [](double v) { return std::isfinite(v); }),
            "Overload must not leave the engine unusable");
}

static void test_reset_clears_state()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {1.0, 0.0, 1.0, 1.0, true});
    for (const auto &frame : sine(48000, 110, 0.1, 0.4))
        overdrive.processSample(frame);
    overdrive.reset();
    // A fresh engine and a reset engine must agree exactly.
    MaelstromOverdrive fresh;
    fresh.prepare(48000, {1.0, 0.0, 1.0, 1.0, true});
    const auto input = sine(48000, 110, 0.02, 0.4);
    const auto resetOutput = render(overdrive, input);
    const auto freshOutput = render(fresh, input);
    for (std::size_t i = 0; i < resetOutput.size(); ++i)
        require(resetOutput[i] == freshOutput[i], "reset() must clear all processing state");
}

static void test_sample_rates()
{
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        MaelstromOverdrive overdrive;
        overdrive.prepare(rate, {0.9, 0.3, 0.6, 1.5, true});
        const auto output = render(overdrive, sine(rate, 220, 0.1, 0.3));
        require(
            std::all_of(output.begin(), output.end(), [](double v) { return std::isfinite(v); }),
            "Every documented sample rate must produce finite output");
        // 8000 Hz cannot represent a 2 kHz harmonic cleanly; only require that
        // the documented minimum does not misbehave.
        double peak = 0;
        for (double v : output)
            peak = std::max(peak, std::abs(v));
        require(peak < 8, "Sample-rate extremes must stay bounded");
    }
    MaelstromOverdrive rejected;
    for (double rate : {0.0, -48000.0, 200000.0, std::numeric_limits<double>::quiet_NaN()}) {
        bool threw = false;
        try {
            rejected.prepare(rate);
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        require(threw, "Unsupported sample rates must be rejected explicitly, not clamped");
    }
}

static void test_latency_is_reported_and_small()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000);
    // Oversampling must not silently add an unreported delay.
    require(overdrive.latencySamples() < 8, "Reported latency must stay small enough to ignore");
}

static void test_aliasing_is_controlled()
{
    // A Goertzel bin cannot distinguish 9 kHz from 39 kHz, so "the folded copy"
    // is not measurable this way. Measure the near-Nyquist floor instead: hard
    // clipping of a tone should not spray energy into the top octave, which is
    // exactly where unsuppressed aliasing would land.
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {1.0, 0.0, 0.0, 1.0, true});
    const auto output = render(overdrive, sine(48000, 3000, 0.2, 0.05));
    const double fundamental = tone(output, 3000, 48000);
    const double nearNyquist = tone(output, 22000, 48000);
    require(fundamental > 0.1, "The test signal must actually reach the clipping stage");
    require(nearNyquist < fundamental * 0.001,
            "Near-Nyquist content must stay at least 60 dB below the intended harmonics");
}

static void test_processing_does_not_allocate()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {0.8, 0.4, 0.7, 1.0, true});
    const auto input = sine(48000, 220, 0.05, 0.2);
    for (const auto &frame : input)
        overdrive.processSample(frame);
    track = true;
    overdrive.setParameters({0.2, 0.9, 0.1, 2.0, false});
    for (const auto &frame : input)
        overdrive.processSample(frame);
    track = false;
    require(allocations == 0, "Processing and parameter changes must not allocate");
}

static void test_rapid_changes_stay_finite()
{
    MaelstromOverdrive overdrive;
    overdrive.prepare(48000, {0.5, 0.5, 0.5, 1.0, true});
    const auto input = sine(48000, 330, 0.2, 0.4);
    double worstStep = 0;
    double previous = overdrive.processSample(input.front())[0];
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (i % 37 == 0) {
            const bool high = (i / 37) % 2 == 0;
            overdrive.setParameters({high ? 1.0 : 0.0, high ? 0.0 : 1.0, high ? 1.0 : 0.0,
                                     high ? 3.9810717055349722 : 0.0, high});
        }
        const double value = overdrive.processSample(input[i])[0];
        require(std::isfinite(value), "Rapid automation must not produce non-finite output");
        worstStep = std::max(worstStep, std::abs(value - previous));
        previous = value;
    }
    require(worstStep < 1.5, "Extreme automation should ramp rather than jump");
}

static void test_stereo_matches_two_mono_engines()
{
    // Independent channels must behave like separate mono instances.
    MaelstromOverdrive stereo;
    stereo.prepare(48000, {0.8, 0.3, 0.6, 1.2, true});
    MaelstromOverdrive left;
    left.prepare(48000, {0.8, 0.3, 0.6, 1.2, true});
    const auto input = sine(48000, 440, 0.1, 0.3);
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto out = stereo.processSample(input[i]);
        const auto mono = left.processSample({input[i][0], 0.0});
        require(out[0] == mono[0], "A stereo channel must match an equivalent mono engine");
    }
}

int main()
{
    try {
        test_tone_measurement_is_trustworthy();
        test_defaults_and_sanitisation();
        test_bypass_is_unity();
        test_engage_fade_has_no_discontinuity();
        test_channels_stay_independent();
        test_tight_reduces_bass_before_clipping();
        test_drive_adds_harmonics_progressively();
        test_bite_shapes_generated_harmonics();
        test_level_is_a_pure_post_clip_gain();
        test_nonfinite_input_fails_safe_and_recovers();
        test_overload_recovers_to_silence();
        test_reset_clears_state();
        test_sample_rates();
        test_latency_is_reported_and_small();
        test_aliasing_is_controlled();
        test_processing_does_not_allocate();
        test_rapid_changes_stay_finite();
        test_stereo_matches_two_mono_engines();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::puts("PASS: Maelstrom overdrive routing, shaping, lifecycle and stability checks");
    return 0;
}
