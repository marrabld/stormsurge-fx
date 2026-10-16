// SPDX-License-Identifier: GPL-3.0-only
#include "TempoSync.hpp"
#include "DarkDelay.hpp"

#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace stormsurge;
static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
static void near(double a, double b, const char *message)
{
    require(std::isfinite(a) && std::abs(a - b) < 1e-8, message);
}

static void timingMath()
{
    // Independent expectations at 120 BPM, in the published division order.
    const std::array<double, 9> expected{500,       750, 1000.0 / 3, 250,      375,
                                         500.0 / 3, 125, 187.5,      250.0 / 3};
    TempoSync resolver;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        for (double bpm : {40.0, 90.0, 120.0, 180.0, 300.0}) {
            const auto timing =
                resolver.resolve(false, true, bpm, 120, true, 777, static_cast<int>(i));
            near(timing.timeMs, expected[i] * 120.0 / bpm, "Note division formula wrong");
            require(!timing.limited && timing.origin == TempoOrigin::Host,
                    "Valid host tempo rejected");
        }
    }
    auto free = resolver.resolve(false, true, 90, 120, false, 777, 4);
    near(free.timeMs, 777, "Free time lost to note division");
    auto longTime = resolver.resolve(false, true, 40, 120, true, 400, 1);
    near(longTime.timeMs, 2250, "Synced delays incorrectly limited to free-time maximum");
    auto limit = resolver.resolve(false, true, 1, 120, true, 400, 1);
    near(limit.timeMs, 8000, "Long sync limit wrong");
    require(limit.limited, "Long sync limit not reported");
    limit = resolver.resolve(false, true, 1000, 120, true, 400, 8);
    near(limit.timeMs, 20, "Short sync limit wrong");
    require(limit.limited, "Short sync limit not reported");
}

static void fallbackAndIsolation()
{
    TempoSync resolver, other;
    auto timing = resolver.resolve(false, false, 0, 120, true, 400, 0);
    require(timing.origin == TempoOrigin::Fallback && timing.bpm == 120, "Initial fallback wrong");
    timing = resolver.resolve(false, false, 999, 100, true, 400, 0);
    require(timing.origin == TempoOrigin::Fallback && timing.bpm == 100, "Fallback not adjustable");
    timing = resolver.resolve(false, true, 90, 120, true, 400, 0);
    require(timing.origin == TempoOrigin::Host && timing.bpm == 90, "Host did not take over");
    for (double bad : {0.0, -10.0, 1001.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        timing = resolver.resolve(false, true, bad, 120, true, 400, 0);
        require(timing.origin == TempoOrigin::Held && timing.bpm == 90,
                "Invalid host tempo not held");
    }
    timing = resolver.resolve(false, false, 120, 200, true, 400, 0);
    require(timing.origin == TempoOrigin::Held && timing.bpm == 90,
            "Missing tempo invented new BPM");
    timing = other.resolve(false, false, 0, 120, true, 400, 0);
    require(timing.origin == TempoOrigin::Fallback && timing.bpm == 120,
            "Instances share tempo history");
    timing = resolver.resolve(true, true, 90, 140, true, 400, 0);
    require(timing.origin == TempoOrigin::Internal && timing.bpm == 140,
            "Standalone followed host tempo");
    timing =
        resolver.resolve(true, false, 0, std::numeric_limits<double>::quiet_NaN(), true, 400, 0);
    require(timing.bpm == 120, "Invalid internal tempo did not fall back safely");
}

static void extendedBuffers()
{
    // Default core clients keep their original two-second capacity; Preview opts in.
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        for (bool ping : {false, true}) {
            DarkDelay delay;
            DelayParameters p;
            p.timeMs = 8000;
            p.mix = 1;
            p.feedback = 0.5;
            p.pingPong = ping;
            delay.prepare(rate, p, 8000);
            const auto interval = static_cast<std::size_t>(rate * 8);
            const std::size_t count = interval * 2 + 128;
            std::vector<float> left(count), right(count), outLeft(count), outRight(count);
            left[0] = right[0] = 1;
            const float *inputs[] = {left.data(), right.data()};
            float *outputs[] = {outLeft.data(), outRight.data()};
            delay.process(inputs, outputs, static_cast<std::uint32_t>(count));
            require(std::all_of(outLeft.begin(), outLeft.begin() + interval,
                                [](float value) { return value == 0; }),
                    "Eight-second echo arrived early");
            require(outLeft[interval] > 0, "Missing eight-second echo");
            require(outRight[interval * 2] > 0, "Missing cross-feedback after eight-second hop");
            if (ping)
                require(outRight[interval] == 0, "Long ping-pong first echo not left-only");
        }
    }
    DarkDelay delay;
    bool rejected = false;
    try {
        delay.prepare(48000, {}, std::numeric_limits<double>::quiet_NaN());
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected, "Invalid buffer capacity accepted");
}

int main()
{
    try {
        timingMath();
        fallbackAndIsolation();
        extendedBuffers();
        std::puts("PASS: nine divisions, tempo sources/fallback/isolation, limits and eight-second "
                  "buffers");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
