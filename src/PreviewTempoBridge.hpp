// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>

namespace stormsurge
{

// Called by the generated Preview VST3 adapter, on the processing thread only.
// Keep the raw host-valid flag: DPF's BBT conversion otherwise invents 120 BPM.
void setPreviewHostTempo(void *instance, bool valid, double bpm) noexcept;

template <class Context>
void supplyPreviewTempo(void *instance, const Context *context,
                        std::uint32_t tempoValidFlag) noexcept
{
    const bool valid = context != nullptr && (context->state & tempoValidFlag) != 0;
    setPreviewHostTempo(instance, valid, valid ? context->bpm : 0.0);
}

} // namespace stormsurge
