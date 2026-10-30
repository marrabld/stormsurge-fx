// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>

namespace stormsurge
{

// Called by the generated Preview VST3 adapter. The host may ask for this
// before, during or after processing, so it reads stored parameter/rate state
// rather than touching delay or reverb buffers.
std::uint32_t previewTailSamples(void *instance) noexcept;

} // namespace stormsurge
