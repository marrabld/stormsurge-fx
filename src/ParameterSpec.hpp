// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace stormsurge
{

// Shared parameter metadata for every StormSurge plugin. Values are the units a
// host and the plugin both see; each editor formats its own readout.
struct ParameterSpec {
    const char *name;
    const char *symbol;
    const char *unit;
    float initial, minimum, maximum;
    bool logarithmic;
    bool readOnly = false;
};

} // namespace stormsurge
