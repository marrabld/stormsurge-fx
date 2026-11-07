// SPDX-License-Identifier: GPL-3.0-only
// Standalone checks for the gate core.
#include "FloodGate.hpp"
#include "FloodGateParameters.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace stormsurge;

namespace
{
int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

constexpr double Rate = 48000;
constexpr double Pi = 3.14159265358979323846;

// Runs a sine of the given peak for `seconds` and returns the peak of the last 100 ms of output.
double settledPeak(FloodGate &gate, double peak, double seconds, double frequency = 220.0)
{
    const auto frames = static_cast<std::size_t>(seconds * Rate);
    const auto tail = static_cast<std::size_t>(0.1 * Rate);
    double out = 0;
    for (std::size_t n = 0; n < frames; ++n) {
        const double x = peak * std::sin(2.0 * Pi * frequency * static_cast<double>(n) / Rate);
        const auto y = gate.processSample({x, x});
        CHECK(y[0] == y[1]); // identical inputs stay identical
        if (n + tail >= frames)
            out = std::max(out, std::abs(y[0]));
    }
    return out;
}

double fromDb(double db)
{
    return std::pow(10.0, db / 20.0);
}

// Runs a different sine on each channel for `seconds` and returns each channel's output peak over
// the last 100 ms divided by its input peak over the same span: the settled per-channel gain.
// Different frequencies keep a channel's result from leaning on the other channel's content.
std::array<double, 2> settledGain(FloodGate &gate, double left, double right, double seconds)
{
    const auto frames = static_cast<std::size_t>(seconds * Rate);
    const auto tail = static_cast<std::size_t>(0.1 * Rate);
    std::array<double, 2> in{}, out{};
    for (std::size_t n = 0; n < frames; ++n) {
        const double t = static_cast<double>(n) / Rate;
        const std::array<double, 2> x{left * std::sin(2.0 * Pi * 220.0 * t),
                                      right * std::sin(2.0 * Pi * 330.0 * t)};
        const auto y = gate.processSample(x);
        if (n + tail >= frames)
            for (std::size_t c = 0; c < 2; ++c) {
                in[c] = std::max(in[c], std::abs(x[c]));
                out[c] = std::max(out[c], std::abs(y[c]));
            }
    }
    return {out[0] / in[0], out[1] / in[1]};
}

// The gate starts open, so a test of opening first has to close it. Silence outlasts the detector
// decay and the 20 ms hold.
void closeGate(FloodGate &gate)
{
    for (int n = 0; n < static_cast<int>(0.2 * Rate); ++n)
        gate.processSample({0.0, 0.0});
}

// Measures the closing time constant in ms. A -100 dBFS DC probe sits below every legal close
// level, so its output divided by its input is the gain. The gain moves geometrically toward the
// Range floor once closed, so two readings give the constant regardless of when the gate closed.
double measuredReleaseMs(double releaseMs)
{
    GateParameters p;
    p.releaseMs = releaseMs;
    FloodGate gate;
    gate.prepare(Rate, p);
    settledGain(gate, 0.1, 0.1, 0.1);
    const double probe = 1e-5, floor = fromDb(p.rangeDb);
    const auto first = static_cast<int>(0.1 * Rate); // closed by ~50 ms
    const auto second = first + static_cast<int>(0.02 * Rate);
    double g1 = 0, g2 = 0;
    for (int n = 0; n <= second; ++n) {
        const double gain = gate.processSample({probe, probe})[0] / probe;
        if (n == first)
            g1 = gain;
        if (n == second)
            g2 = gain;
    }
    const double frames = second - first;
    return frames / std::log((g1 - floor) / (g2 - floor)) / Rate * 1000.0;
}
} // namespace

int main()
{
    const GateParameters defaults;

    { // Signal well above threshold passes at unity.
        FloodGate gate;
        gate.prepare(Rate, defaults);
        CHECK(std::abs(settledPeak(gate, 0.3, 0.5) - 0.3) < 1e-6);
    }
    { // Signal below threshold is pulled down to the Range floor (-80 dB).
        FloodGate gate;
        gate.prepare(Rate, defaults);
        CHECK(settledPeak(gate, 0.0003, 1.0) < 0.0003 * 2e-4); // -70 dBFS in, ~-150 dBFS out
    }
    // Every end of every range is honoured exactly, so the core never narrows what the host can
    // legally write. A core limit tighter than the spec fails at least one of these.
    { // Threshold: 1 dB above opens a closed gate; 7 dB below (past the 6 dB hysteresis) closes.
        const auto &spec = floodGateSpecs[FloodGateThreshold];
        for (const double edge : {double(spec.minimum), double(spec.maximum)}) {
            GateParameters p = defaults;
            p.thresholdDb = edge;
            FloodGate gate;
            gate.prepare(Rate, p);
            closeGate(gate);
            CHECK(std::abs(settledGain(gate, fromDb(edge + 1), fromDb(edge + 1), 0.5)[0] - 1) <
                  1e-6);
            CHECK(settledGain(gate, fromDb(edge - 7), fromDb(edge - 7), 1.0)[0] < 2e-4);
        }
    }
    { // Release: the measured closing time constant matches both ends within 0.1%.
        const auto &spec = floodGateSpecs[FloodGateRelease];
        for (const double edge : {double(spec.minimum), double(spec.maximum)})
            CHECK(std::abs(measuredReleaseMs(edge) / edge - 1) < 1e-3);
    }
    { // Range: a closed gate settles on exactly the floor at both ends.
        const auto &spec = floodGateSpecs[FloodGateRange];
        for (const double edge : {double(spec.minimum), double(spec.maximum)}) {
            GateParameters p = defaults;
            p.rangeDb = edge;
            FloodGate gate;
            gate.prepare(Rate, p);
            const auto gain = settledGain(gate, fromDb(-70), fromDb(-70), 1.5);
            CHECK(std::abs(gain[0] / fromDb(edge) - 1) < 1e-3);
            CHECK(std::abs(gain[1] / fromDb(edge) - 1) < 1e-3);
        }
    }
    { // Stereo link: a loud channel opens a closed gate for a quiet channel that would be gated
      // on its own, in both directions. Testing only one side would miss a detector that ignores
      // the other. The control proves the quiet level alone stays closed.
        const double loud = fromDb(-20), quiet = fromDb(-70);
        FloodGate control;
        control.prepare(Rate, defaults);
        closeGate(control);
        const auto closed = settledGain(control, quiet, quiet, 1.0);
        CHECK(closed[0] < 2e-4 && closed[1] < 2e-4);

        for (const bool loudLeft : {true, false}) {
            FloodGate gate;
            gate.prepare(Rate, defaults);
            closeGate(gate);
            const auto gain = loudLeft ? settledGain(gate, loud, quiet, 0.5)
                                       : settledGain(gate, quiet, loud, 0.5);
            CHECK(std::abs(gain[0] - 1) < 1e-6);
            CHECK(std::abs(gain[1] - 1) < 1e-6);
        }
    }
    { // Stereo link: both channels get the same gain on every sample while the gate opens and
      // closes, not only once settled, so a moving gate cannot pull the image. Different DC levels
      // per side make each sample's output/input ratio that channel's exact gain.
        GateParameters p = defaults;
        p.releaseMs = floodGateSpecs[FloodGateRelease].maximum; // longest, most audible fade
        FloodGate gate;
        gate.prepare(Rate, p);
        closeGate(gate);
        bool matched = true;
        double gain = 0;
        const auto run = [&](std::array<double, 2> in, double seconds) {
            for (int n = 0; n < static_cast<int>(seconds * Rate); ++n) {
                const auto y = gate.processSample(in);
                gain = y[0] / in[0];
                matched = matched && std::abs(y[1] / in[1] - gain) <= 1e-12 * gain;
            }
        };
        run({0.1, -0.03}, 0.1); // opening
        CHECK(std::abs(gain - 1) < 1e-6);
        run({1e-5, -3e-5}, 1.0); // closing, below every legal close level
        CHECK(gain < 0.5);       // the fade really happened, so the comparison was not vacuous
        CHECK(matched);
    }
    { // Stereo link: an open gate closes when both channels drop below threshold, even with
      // different levels on each side.
        FloodGate gate;
        gate.prepare(Rate, defaults);
        settledGain(gate, fromDb(-20), fromDb(-20), 0.2);
        const auto gain = settledGain(gate, fromDb(-70), fromDb(-75), 1.0);
        CHECK(gain[0] < 2e-4 && gain[1] < 2e-4);
    }
    { // Hysteresis: a level between close (-6 dB) and open stays open once opened.
        GateParameters p = defaults;
        p.thresholdDb = -40;
        FloodGate gate;
        gate.prepare(Rate, p);
        settledPeak(gate, 0.1, 0.2);                     // open the gate
        const double mid = std::pow(10.0, -43.0 / 20.0); // inside the dead zone
        CHECK(std::abs(settledPeak(gate, mid, 0.5) - mid) < 1e-6);
    }
    { // Disabling ramps to exact dry; bypassSample is exact unity.
        FloodGate gate;
        gate.prepare(Rate, defaults);
        GateParameters off = defaults;
        off.enabled = false;
        gate.setParameters(off);
        const std::array<double, 2> in{0.00001, -0.00002}; // would be gated if enabled
        std::array<double, 2> out{};
        for (int n = 0; n < 2000; ++n)
            out = gate.processSample(in);
        CHECK(out[0] == in[0] && out[1] == in[1]);
        CHECK(gate.bypassSample(in) == in);
    }
    { // A non-finite channel becomes silence and clears state; the other channel still passes.
        FloodGate gate;
        gate.prepare(Rate, defaults);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const auto bad = gate.processSample({nan, 0.5});
        CHECK(bad[0] == 0.0 && std::abs(bad[1] - 0.5) < 1e-12);
        const auto bypassed = gate.bypassSample({nan, 0.5});
        CHECK(bypassed[0] == 0.0 && bypassed[1] == 0.5);
    }
    { // Same contract as Maelstrom: unusable sample rates throw, the supported edges do not.
        const double nan = std::numeric_limits<double>::quiet_NaN();
        for (const double bad : {nan, 7999.0, 192001.0}) {
            FloodGate gate;
            bool threw = false;
            try {
                gate.prepare(bad, defaults);
            } catch (const std::invalid_argument &) {
                threw = true;
            }
            CHECK(threw);
        }
        for (const double edge : {8000.0, 192000.0}) {
            FloodGate gate;
            gate.prepare(edge, defaults);
            const auto y = gate.processSample({0.1, 0.1});
            CHECK(std::isfinite(y[0]) && std::isfinite(y[1]));
        }
    }
    { // Hostile parameters never produce a non-finite sample.
        FloodGate gate;
        const double nan = std::numeric_limits<double>::quiet_NaN();
        gate.prepare(Rate, {nan, nan, nan, true});
        gate.setParameters({1e300, -1e300, nan, true});
        for (int n = 0; n < 1000; ++n) {
            const auto y = gate.processSample({0.1, -0.1});
            CHECK(std::isfinite(y[0]) && std::isfinite(y[1]));
        }
    }
    { // Digital silence for 10 s must not leave a denormal envelope behind.
        FloodGate gate;
        gate.prepare(Rate, defaults);
        for (int n = 0; n < 480000; ++n)
            gate.processSample({0.0, 0.0});
        const auto y = gate.processSample({0.2, 0.2});
        CHECK(std::isfinite(y[0]) && y[0] != 0.0);
    }
    CHECK(FloodGate{}.latencySamples() == 0);

    std::puts(failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
