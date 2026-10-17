// SPDX-License-Identifier: GPL-3.0-only
#include "PreviewAuTempo.hpp"
#include "TempoSync.hpp"

#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace stormsurge;
namespace
{
bool receivedValid = false;
double receivedBpm = 0;
unsigned calls = 0;
struct Host {
    int status = 0;
    double beat = 12, bpm = 100;
};
int callback(void *data, double *beat, double *bpm)
{
    ++calls;
    const auto &host = *static_cast<Host *>(data);
    *beat = host.beat;
    *bpm = host.bpm;
    return host.status;
}
struct Callbacks {
    void *hostUserData;
    int (*beatAndTempoProc)(void *, double *, double *);
};
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

void stormsurge::setPreviewHostTempo(void *, bool valid, double bpm) noexcept
{
    receivedValid = valid;
    receivedBpm = bpm;
}

int main()
{
    try {
        Host host;
        Callbacks callbacks{&host, callback};
        TempoSync timing;
        auto resolve = [&]() {
            supplyPreviewAuTempo(nullptr, callbacks);
            return timing.resolve(false, receivedValid, receivedBpm, 90, true, 400, 0);
        };
        callbacks.beatAndTempoProc = nullptr;
        require(resolve().origin == TempoOrigin::Fallback && receivedBpm == 0 && calls == 0,
                "Missing AU callback did not use explicit initial fallback");
        callbacks.beatAndTempoProc = callback;
        auto clock = supplyPreviewAuTempo(nullptr, callbacks);
        require(clock.valid && clock.beat == 12 && receivedValid && receivedBpm == 100 &&
                    calls == 1,
                "AU callback did not preserve raw tempo or was queried twice");
        require(resolve().timeMs == 600, "AU host BPM was not applied");
        host.status = -1;
        host.bpm = 120;
        auto result = resolve();
        require(result.origin == TempoOrigin::Held && result.bpm == 100 && !receivedValid,
                "Failed AU callback was mistaken for a synthetic host tempo");
        host.status = 0;
        for (double invalid : {0.0, -1.0, 1001.0, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity()}) {
            host.bpm = invalid;
            result = resolve();
            require(result.origin == TempoOrigin::Held && result.bpm == 100,
                    "Invalid AU BPM replaced the held host tempo");
        }
        host.bpm = 140;
        host.beat = std::numeric_limits<double>::infinity();
        clock = supplyPreviewAuTempo(nullptr, callbacks);
        require(clock.valid && clock.beat == 0 && receivedBpm == 140,
                "Unusable beat position contaminated BBT or invalidated a usable BPM");
        host.beat = 1e30;
        require(supplyPreviewAuTempo(nullptr, callbacks).beat == 0,
                "Out-of-range AU beat reached DPF's int32 conversion");
        require(resolve().bpm == 140, "AU tempo changes require an unrelated transport callback");
        callbacks.beatAndTempoProc = nullptr;
        require(resolve().origin == TempoOrigin::Held && receivedBpm == 0,
                "Removing all AU callbacks retained a stale HOST source");
        std::puts("PASS: AU raw tempo, callback failure/removal, fallback/hold, invalid values and "
                  "beat sanitation");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
