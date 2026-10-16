// SPDX-License-Identifier: GPL-3.0-only
#include "DarkDelay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

static bool trackAllocations = false;
static std::size_t allocations = 0;

void *operator new(std::size_t size)
{
    if (trackAllocations)
        ++allocations;
    if (void *pointer = std::malloc(size == 0 ? 1 : size))
        return pointer;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void *pointer) noexcept
{
    std::free(pointer);
}
void operator delete[](void *pointer) noexcept
{
    std::free(pointer);
}
void operator delete(void *pointer, std::size_t) noexcept
{
    std::free(pointer);
}
void operator delete[](void *pointer, std::size_t) noexcept
{
    std::free(pointer);
}

using stormsurge::DarkDelay;
using stormsurge::DelayParameters;
using Audio = std::array<std::vector<float>, 2>;

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void near(double actual, double expected, double tolerance, const char *message)
{
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

static Audio signal(std::size_t frames)
{
    return {std::vector<float>(frames), std::vector<float>(frames)};
}

static void block(DarkDelay &delay, const Audio &input, Audio &output, std::size_t offset,
                  std::uint32_t frames, bool inPlace = false)
{
    const float *inputs[] = {input[0].data() + offset, input[1].data() + offset};
    float *outputs[] = {output[0].data() + offset, output[1].data() + offset};
    if (inPlace) {
        std::copy_n(inputs[0], frames, outputs[0]);
        std::copy_n(inputs[1], frames, outputs[1]);
        inputs[0] = outputs[0];
        inputs[1] = outputs[1];
    }
    trackAllocations = true;
    delay.process(inputs, outputs, frames);
    trackAllocations = false;
}

static Audio render(double rate, const DelayParameters &p, const Audio &input,
                    std::uint32_t blockSize, bool inPlace = false)
{
    DarkDelay delay;
    delay.prepare(rate, p);
    auto output = signal(input[0].size());
    for (std::size_t offset = 0; offset < input[0].size();) {
        const auto frames =
            static_cast<std::uint32_t>(std::min<std::size_t>(blockSize, input[0].size() - offset));
        block(delay, input, output, offset, frames, inPlace);
        offset += frames;
    }
    return output;
}

static void timingAndDecay()
{
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        for (double time : {20.0, 400.0, 2000.0}) {
            DelayParameters p;
            p.timeMs = time;
            p.feedback = 0.5;
            p.mix = 1.0;
            p.toneHz = 400.0;
            const auto interval = static_cast<std::size_t>(std::lround(rate * time / 1000.0));
            auto input = signal(interval * 4);
            input[0][0] = 1.0f;
            auto output = render(rate, p, input, 127);
            require(std::all_of(output[0].begin(), output[0].begin() + interval,
                                [](float x) { return x == 0.0f; }),
                    "Wet signal arrived before the delay time");
            require(output[0][interval] > 0.0f, "Missing first repeat");
            require(
                std::all_of(output[1].begin(), output[1].end(), [](float x) { return x == 0.0f; }),
                "Left signal leaked into right channel");
            double previous = 1.0;
            double previousBrightness = 1.0;
            for (std::size_t repeat = 1; repeat <= 3; ++repeat) {
                double sum = 0.0, alternating = 0.0;
                for (std::size_t i = 0; i < interval; ++i) {
                    const double sample = output[0][repeat * interval + i];
                    sum += sample;
                    alternating += i % 2 == 0 ? sample : -sample;
                }
                near(sum, previous, 1.0e-5, "Unexpected repeat gain/decay");
                const double brightness = std::abs(alternating / sum);
                require(brightness < previousBrightness, "Repeats did not get darker");
                previousBrightness = brightness;
                previous *= p.feedback;
            }
        }
    }
    // Fractional reads distribute the impulse between adjacent samples.
    DelayParameters p;
    p.timeMs = 20.0625; // 160.5 samples at 8 kHz
    p.mix = 1.0;
    p.feedback = 0.0;
    auto input = signal(1000);
    input[0][0] = 1.0f;
    auto output = render(8000, p, input, 63);
    const double pole = std::exp(-2.0 * 3.141592653589793 * p.toneHz / 8000.0);
    near(output[0][159], 0.0, 0.0, "Fractional read arrived early");
    near(output[0][160], 0.5 * (1.0 - pole), 1.0e-6, "Fractional first sample wrong");
    near(output[0][161], 0.5 * (1.0 - pole) * (1.0 + pole), 1.0e-6,
         "Fractional interpolation wrong");
}

static void buffersAndMix()
{
    auto input = signal(4097);
    for (std::size_t i = 0; i < input[0].size(); ++i) {
        input[0][i] = static_cast<float>(2.5 * std::sin(i * 0.137));
        input[1][i] = static_cast<float>(-3.0 * std::cos(i * 0.291));
    }
    DelayParameters p;
    p.timeMs = 20;
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        auto reference = render(rate, p, input, 1);
        for (std::uint32_t size : {2u, 63u, 64u, 127u, 512u, 2048u}) {
            require(render(rate, p, input, size) == reference, "Block partition changed output");
            require(render(rate, p, input, size, true) == reference, "In-place output differs");
        }
    }
    p.mix = 0.0;
    require(render(48000, p, input, 512) == input, "Dry-only altered over-range audio");
    p.outputGain = 0.0;
    auto muted = render(48000, p, input, 512);
    for (const auto &channel : muted)
        require(std::all_of(channel.begin(), channel.end(), [](float x) { return x == 0.0f; }),
                "Output gain zero did not silence");
    p.outputGain = std::pow(10.0, 6.0 / 20.0);
    auto boosted = render(48000, p, input, 512);
    near(boosted[1][0], input[1][0] * p.outputGain, 1.0e-6, "Output boost wrong");

    DarkDelay delay;
    delay.prepare(48000);
    auto sentinel = signal(2);
    sentinel[0].assign(2, 99.0f);
    sentinel[1].assign(2, 99.0f);
    block(delay, input, sentinel, 0, 0);
    require(sentinel[0][0] == 99 && sentinel[1][0] == 99, "Zero frames wrote output");
    block(delay, input, sentinel, 0, 1);
    require(sentinel[0][1] == 99 && sentinel[1][1] == 99, "Wrote beyond block boundary");
}

static void bypassTail(bool pingPong)
{
    DelayParameters p;
    p.pingPong = pingPong;
    p.timeMs = 100;
    p.mix = 0.2;
    p.feedback = 0.5;
    auto input = signal(48000);
    input[0][0] = 1.0f;
    input[0][12000] = 1.0f; // well after bypass ramp has ended
    DarkDelay delay;
    delay.prepare(48000, p);
    auto output = signal(48000);
    block(delay, input, output, 0, 2400);
    p.bypass = true;
    delay.setParameters(p);
    block(delay, input, output, 2400, 45600);
    require(output[0][4800] > 0, "Bypass cut the existing tail");
    near(output[0][12000], 1.0, 1.0e-6, "Bypass did not restore unity dry");

    // The new bypassed impulse must contribute no echo or feedback state.
    input[0][12000] = 0.0f;
    DarkDelay reference;
    p.bypass = false;
    reference.prepare(48000, p);
    auto expected = signal(48000);
    block(reference, input, expected, 0, 2400);
    p.bypass = true;
    reference.setParameters(p);
    block(reference, input, expected, 2400, 45600);
    for (std::size_t i = 12001; i < 48000; ++i)
        near(output[0][i], expected[0][i], 0.0, "Bypassed notes entered delay");
    p.bypass = false;
    delay.setParameters(p);
    reference.setParameters(p);
    auto silence = signal(48000);
    block(delay, silence, output, 0, 48000);
    block(reference, silence, expected, 0, 48000);
    require(output == expected, "Re-engaging exposed bypassed notes");
}

static Audio automated(std::uint32_t blockSize)
{
    DarkDelay delay;
    DelayParameters p;
    p.timeMs = 20;
    delay.prepare(8000, p);
    auto input = signal(80000);
    auto output = signal(80000);
    for (std::size_t i = 0; i < input[0].size(); ++i) {
        input[0][i] = static_cast<float>(0.25 * std::sin(i * 0.01));
        input[1][i] = -input[0][i];
    }
    // Parameter events have identical sample positions for every block partition.
    for (std::size_t event = 0; event < input[0].size(); event += 80) {
        p.timeMs = event % 160 == 0 ? 20.0 : 2000.0;
        p.feedback = event % 320 == 0 ? 0.9 : 0.0;
        p.mix = event % 240 == 0 ? 1.0 : 0.2;
        p.toneHz = event % 160 == 0 ? 400.0 : 16000.0;
        p.outputGain = event % 320 == 0 ? 0.0 : 1.0;
        p.bypass = event % 400 == 0;
        trackAllocations = true;
        delay.setParameters(p);
        trackAllocations = false;
        for (std::size_t offset = event; offset < event + 80;) {
            auto frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(blockSize, event + 80 - offset));
            block(delay, input, output, offset, frames);
            offset += frames;
        }
    }
    for (const auto &channel : output) {
        for (std::size_t i = 0; i < channel.size(); ++i) {
            require(std::isfinite(channel[i]), "Automation produced nonfinite output");
            if (i != 0)
                require(std::abs(channel[i] - channel[i - 1]) < 0.1f,
                        "Automation produced a discontinuity on smooth test input");
        }
    }
    return output;
}

static void stabilityAndReset()
{
    DelayParameters p;
    p.timeMs = 20;
    p.feedback = 0.9;
    p.mix = 1;
    p.toneHz = 16000;
    auto input = signal(8000 * 8);
    std::fill(input[0].begin(), input[0].begin() + 8000 * 4, 1.0f);
    auto output = render(8000, p, input, 127);
    require(*std::max_element(output[0].begin(), output[0].end()) <= 10.0001f,
            "Maximum-feedback DC exceeded loop bound");
    require(output[0][8000 * 4 - 1] > 9.9f, "DC steady-state gain unexpectedly attenuated");
    require(std::abs(output[0].back()) < 1.0e-6f, "Tail failed to decay");

    DarkDelay delay;
    delay.prepare(8000, p);
    auto scratch = signal(input[0].size());
    block(delay, input, scratch, 0, static_cast<std::uint32_t>(input[0].size()));
    trackAllocations = true;
    delay.reset();
    trackAllocations = false;
    auto silence = signal(input[0].size());
    block(delay, silence, scratch, 0, static_cast<std::uint32_t>(silence[0].size()));
    require(scratch == silence, "Reset retained delay/filter state");
    delay.prepare(192000, p);
    block(delay, silence, scratch, 0, static_cast<std::uint32_t>(silence[0].size()));
    require(scratch == silence, "Sample-rate reprepare retained tail");
    bool rejected = false;
    try {
        delay.prepare(std::numeric_limits<double>::quiet_NaN());
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected, "Invalid sample rate accepted");

    DelayParameters invalid;
    invalid.timeMs = invalid.feedback = invalid.mix = invalid.toneHz = invalid.outputGain =
        std::numeric_limits<double>::quiet_NaN();
    auto badInput = signal(10000);
    badInput[0][0] = std::numeric_limits<float>::infinity();
    badInput[1][1] = std::numeric_limits<float>::quiet_NaN();
    require(render(48000, invalid, badInput, 127) == signal(10000),
            "Nonfinite samples contaminated state");
    auto clean = signal(24000);
    clean[0][0] = 1.0f;
    require(render(48000, invalid, clean, 127) == render(48000, {}, clean, 127),
            "Invalid parameters did not fall back to defaults");
    p.timeMs = -100;
    p.feedback = 20;
    p.mix = 30;
    p.toneHz = 90000;
    p.outputGain = 100;
    auto clamped = p;
    clamped.timeMs = 20;
    clamped.feedback = 0.9;
    clamped.mix = 1;
    clamped.toneHz = 16000;
    clamped.outputGain = std::pow(10.0, 6.0 / 20.0);
    require(render(48000, p, clean, 127) == render(48000, clamped, clean, 127),
            "Parameter limits not enforced");
}

static void timeChangeSettles()
{
    DelayParameters p;
    p.timeMs = 20;
    p.mix = 1;
    p.feedback = 0;
    DarkDelay delay;
    delay.prepare(8000, p);
    auto input = signal(5000);
    auto output = signal(5000);
    input[0][2000] = 1;
    p.timeMs = 100;
    delay.setParameters(p);
    block(delay, input, output, 0, 200);
    p.timeMs = 200;
    delay.setParameters(p); // queue a new target halfway through the first fade
    block(delay, input, output, 200, 4800);
    require(
        std::all_of(output[0].begin(), output[0].begin() + 3600, [](float x) { return x == 0.0f; }),
        "Time fade retained an old read head or missed the latest request");
    require(output[0][3600] > 0, "Latest time request never settled");
}

static void pingPongTimingAndTone()
{
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        for (double time : {20.0, 400.0, 2000.0}) {
            DelayParameters p;
            p.pingPong = true;
            p.timeMs = time;
            p.mix = 1;
            p.feedback = 0.5;
            p.toneHz = 400;
            const auto interval = static_cast<std::size_t>(std::lround(rate * time / 1000.0));
            auto input = signal(interval * 5);
            input[0][0] = input[1][0] = 1;
            const auto output = render(rate, p, input, 127);
            require(output[0][interval] > 0 && output[1][interval] == 0,
                    "Ping-pong first repeat was not left-only");
            double amplitude = 1, brightness = 1;
            for (std::size_t repeat = 1; repeat <= 4; ++repeat) {
                const std::size_t side = (repeat - 1) % 2;
                double sum = 0, wrongSide = 0, alternating = 0;
                for (std::size_t i = 0; i < interval; ++i) {
                    const auto frame = repeat * interval + i;
                    sum += output[side][frame];
                    wrongSide += std::abs(output[1 - side][frame]);
                    alternating += (i % 2 == 0 ? 1 : -1) * output[side][frame];
                }
                near(sum, amplitude, 1e-5, "Ping-pong repeat timing or feedback gain wrong");
                near(wrongSide, 0, 1e-5, "Ping-pong repeat did not alternate channels");
                require(std::abs(alternating / sum) < brightness,
                        "Alternating repeats did not get progressively darker");
                brightness = std::abs(alternating / sum);
                amplitude *= p.feedback;
            }
        }
    }

    DelayParameters p;
    p.pingPong = true;
    p.timeMs = 20;
    p.mix = 1;
    auto both = signal(4000);
    both[0][0] = both[1][0] = 1;
    const auto full = render(8000, p, both, 63);
    auto left = both;
    left[1][0] = 0;
    auto right = both;
    right[0][0] = 0;
    const auto half = render(8000, p, left, 63);
    require(half == render(8000, p, right, 63), "Wet averaging depends on input side");
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t i = 0; i < both[0].size(); ++i)
            near(half[channel][i], full[channel][i] * 0.5, 1e-8, "Stereo wet averaging wrong");
    auto opposite = both;
    opposite[1][0] = -1;
    require(render(8000, p, opposite, 63) == signal(4000),
            "Opposite-phase wet input did not cancel as documented");
    p.mix = 0;
    require(render(8000, p, opposite, 63) == opposite, "Ping-pong changed dry stereo routing");
    // Cross-feedback remains silent after the first left repeat when feedback is zero.
    p.mix = 1;
    p.feedback = 0;
    const auto single = render(8000, p, both, 63);
    require(std::all_of(single[1].begin(), single[1].end(), [](float x) { return x == 0; }),
            "Zero feedback unexpectedly produced a right repeat");
    p.timeMs = 20.0625; // 160.5 samples per hop, not 160.5 per round trip
    p.feedback = 0.5;
    p.toneHz = 2500;
    const auto fractional = render(8000, p, both, 63);
    const double a = std::exp(-2.0 * 3.141592653589793 * p.toneHz / 8000.0);
    near(fractional[0][159], 0, 0, "Fractional ping-pong left arrived early");
    near(fractional[0][160], 0.5 * (1 - a), 1e-6, "Fractional first hop wrong");
    near(fractional[1][319], 0, 0, "Fractional ping-pong right arrived early");
    near(fractional[1][320], 0.25 * p.feedback * (1 - a) * (1 - a), 1e-6,
         "Fractional cross-feedback hop wrong");
}

static Audio routingAutomation(std::uint32_t blockSize, bool inPlace)
{
    DelayParameters p;
    p.timeMs = 20;
    p.mix = 1;
    p.feedback = 0.9;
    DarkDelay delay;
    delay.prepare(8000, p);
    auto input = signal(48000), output = signal(48000);
    std::fill(input[0].begin(), input[0].end(), 1.0f);
    std::fill(input[1].begin(), input[1].end(), 1.0f);
    for (std::size_t event = 0; event < 48000; event += 128) {
        if (event >= 8192) {
            p.pingPong = event % 256 == 0;
            p.timeMs = event % 512 == 0 ? 400 : 20;
            p.toneHz = event % 256 == 0 ? 400 : 16000;
        }
        trackAllocations = true;
        delay.setParameters(p);
        trackAllocations = false;
        for (std::size_t offset = event; offset < event + 128;) {
            auto frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(blockSize, event + 128 - offset));
            block(delay, input, output, offset, frames, inPlace);
            offset += frames;
        }
    }
    for (const auto &channel : output)
        for (std::size_t i = 0; i < channel.size(); ++i) {
            require(std::isfinite(channel[i]) && std::abs(channel[i]) <= 10.0001f,
                    "Mode/time automation exceeded the maximum-feedback DC bound");
            if (i > 8192)
                require(std::abs(channel[i] - channel[i - 1]) < 0.1f,
                        "Routing changes caused a discontinuity on steady test input");
        }
    // Reset must clear both lines and routing state after sustained cross-feedback.
    auto silence = signal(48000);
    p.pingPong = true;
    delay.setParameters(p);
    trackAllocations = true;
    delay.reset();
    trackAllocations = false;
    auto cleared = signal(48000);
    block(delay, silence, cleared, 0, 48000);
    require(cleared == silence, "Ping-pong reset retained buffered/cross-feedback state");
    return output;
}

static void modeSwitchPreservesTail()
{
    DelayParameters p;
    p.timeMs = 100;
    p.mix = 1;
    DarkDelay delay;
    delay.prepare(48000, p);
    auto input = signal(20000), output = signal(20000);
    input[0][0] = input[1][0] = 1;
    block(delay, input, output, 0, 2400);
    p.pingPong = true;
    delay.setParameters(p);
    block(delay, input, output, 2400, 17600);
    require(output[0][4800] > 0 && output[1][4800] > 0,
            "Switching mode discarded an existing tail");
    p.pingPong = false;
    delay.setParameters(p);
    auto silence = signal(20000);
    block(delay, silence, output, 0, 20000);
    require(std::any_of(output[0].begin(), output[0].end(), [](float x) { return x != 0; }) ||
                std::any_of(output[1].begin(), output[1].end(), [](float x) { return x != 0; }),
            "Returning to Normal discarded an existing tail");
}

int main()
{
    try {
        timingAndDecay();
        buffersAndMix();
        bypassTail(false);
        bypassTail(true);
        auto reference = automated(1);
        require(automated(63) == reference && automated(512) == reference,
                "Sample-timed automation depends on block partition");
        stabilityAndReset();
        timeChangeSettles();
        pingPongTimingAndTone();
        auto routing = routingAutomation(1, false);
        require(routingAutomation(63, false) == routing && routingAutomation(512, true) == routing,
                "Mode/time automation depends on blocks or in-place processing");
        modeSwitchPreservesTail();
        require(allocations == 0, "Allocated during processing/parameter update/reset");
        std::puts(
            "PASS: dark delay timing, decay, darkening, routing, mix, bypass tails, buffers, "
            "automation, ping-pong, reset, headroom, invalid values and callback allocations");
        return 0;
    } catch (const std::exception &error) {
        trackAllocations = false;
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
