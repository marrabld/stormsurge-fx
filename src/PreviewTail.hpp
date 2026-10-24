// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace stormsurge
{

// A delay with feedback decays geometrically. Report the time for the wet
// repeats to fall 60 dB relative to the first repeat, rather than assuming one
// delay interval is enough. Reverb is fed by the delay, so its nominal decay is
// added when it contributes to the output.
inline constexpr double PreviewTailDecay = 0.001;
inline constexpr double PreviewTailCapSeconds = 600.0;

struct PreviewTailSettings {
    double delaySeconds = 0.4;
    double feedback = 0.35;
    double mix = 0.2;
    bool bypass = false;
    double reverbSeconds = 6.0;
    double reverbAmount = 0.0;
};

inline double previewTailSeconds(const PreviewTailSettings &settings) noexcept
{
    if (settings.bypass || !(settings.mix > 0.0) || !(settings.delaySeconds > 0.0) ||
        !std::isfinite(settings.delaySeconds))
        return 0.0;

    const double feedback =
        std::isfinite(settings.feedback) ? std::clamp(settings.feedback, 0.0, 0.9) : 0.0;
    // Feedback zero still sends one wet copy through the delay line.
    double repeats = 1.0;
    if (feedback > 0.0)
        repeats = std::max(1.0, std::ceil(std::log(PreviewTailDecay) / std::log(feedback)));

    double seconds = settings.delaySeconds * repeats;
    if (settings.reverbAmount > 0.0 && std::isfinite(settings.reverbSeconds) &&
        settings.reverbSeconds > 0.0)
        seconds += std::min(settings.reverbSeconds, 12.0);

    return std::min(seconds, PreviewTailCapSeconds);
}

inline std::uint32_t previewTailSamples(double sampleRate,
                                        const PreviewTailSettings &settings) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        return 0;
    const double seconds = previewTailSeconds(settings);
    if (seconds <= 0.0)
        return 0;
    // The cap keeps this far below uint32 overflow; clamp anyway so a future
    // change cannot wrap the value hosts use to size an offline render.
    const double samples = std::ceil(seconds * sampleRate);
    return static_cast<std::uint32_t>(std::min(samples, 4294967294.0));
}

} // namespace stormsurge
