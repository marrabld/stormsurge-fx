// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace stormsurge
{

struct ReverbParameters {
    double spaceSeconds = 6;
    double darkness = 65;
    double amount = 0;
};

// Stereo wet-only insert. One audio owner, prepare off the callback before use.
// Space specifies nominal undamped decay, not a guarantee of measured RT60.
class DarkReverb
{
  public:
    void prepare(double sampleRate, const ReverbParameters &parameters = {});
    void setParameters(const ReverbParameters &parameters) noexcept;
    void reset() noexcept;
    std::array<double, 2> processSample(const std::array<double, 2> &input) noexcept;

  private:
    struct Line {
        std::vector<double> data;
        std::size_t position = 0;
        void prepare(std::size_t frames);
        void reset() noexcept;
        double read() const noexcept;
        void write(double value) noexcept;
    };
    struct Smooth {
        double value = 0, target = 0, step = 0;
        std::uint32_t remaining = 0;
        void set(double next, std::uint32_t frames) noexcept;
        double tick() noexcept;
    };
    static ReverbParameters sanitize(const ReverbParameters &parameters) noexcept;
    void coefficients(bool snap) noexcept;
    std::array<Line, 8> lines;
    std::array<std::array<Line, 2>, 2> diffusers;
    std::array<double, 8> damped{};
    std::array<double, 2> lowInput{};
    std::array<Smooth, 8> gains;
    Smooth pole, amount;
    ReverbParameters parameters;
    double rate = 48000, highpassPole = 0;
    std::uint32_t rampFrames = 960;
};

} // namespace stormsurge
