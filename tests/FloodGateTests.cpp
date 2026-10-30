// SPDX-License-Identifier: GPL-3.0-only
// Standalone checks for the gate core. Adapt the CHECK macro to the project's test harness.
#include "FloodGate.hpp"
#include "FloodGateParameters.hpp"

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
    { // Spec extremes are accepted, not clamped lower: threshold at its maximum (-20 dB) must
      // close on a -25 dBFS sine. A core limit below the spec maximum would leave it open.
        GateParameters p = defaults;
        p.thresholdDb = floodGateSpecs[FloodGateThreshold].maximum;
        p.releaseMs = floodGateSpecs[FloodGateRelease].minimum;
        p.rangeDb = floodGateSpecs[FloodGateRange].minimum;
        FloodGate gate;
        gate.prepare(Rate, p);
        CHECK(settledPeak(gate, std::pow(10.0, -25.0 / 20.0), 1.0) < 1e-3);
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
