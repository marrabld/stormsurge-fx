// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#define DISTRHO_PLUGIN_BRAND "StormSurge FX"
#ifdef STORMSURGE_PREVIEW
#define DISTRHO_PLUGIN_NAME "Undertow Preview"
#define DISTRHO_PLUGIN_URI "https://github.com/marrabld/stormsurge-fx/undertow-preview"
#define DISTRHO_PLUGIN_UNIQUE_ID UtP0
#define DISTRHO_PLUGIN_WANT_TIMEPOS 1
#ifdef __APPLE__
// Pinned DPF's AU metadata exporter reuses this name even without a CLAP target.
#define DISTRHO_PLUGIN_CLAP_ID "org.stormsurgefx.undertow-preview.au"
#endif
#elif defined(STORMSURGE_MAELSTROM)
// Separate identity from Undertow Preview: existing projects must never acquire
// an overdrive, and saved Maelstrom settings must not disturb them.
#define DISTRHO_PLUGIN_NAME "Maelstrom"
#define DISTRHO_PLUGIN_URI "https://github.com/marrabld/stormsurge-fx/maelstrom"
#define DISTRHO_PLUGIN_UNIQUE_ID UtM0
// The engine oversamples, so it owes the host an accurate one-sample report.
#define DISTRHO_PLUGIN_WANT_LATENCY 1
#ifdef __APPLE__
#define DISTRHO_PLUGIN_CLAP_ID "org.stormsurgefx.maelstrom.au"
#endif
#else
#define DISTRHO_PLUGIN_NAME "Undertow Foundation"
#define DISTRHO_PLUGIN_URI "https://github.com/marrabld/stormsurge-fx/foundation"
#define DISTRHO_PLUGIN_UNIQUE_ID UtF0
#endif
#define DISTRHO_PLUGIN_BRAND_ID SgFx

#define DISTRHO_PLUGIN_HAS_UI 1
#define DISTRHO_PLUGIN_IS_RT_SAFE 1
#define DISTRHO_PLUGIN_NUM_INPUTS 2
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_PROGRAMS 0
#define DISTRHO_PLUGIN_WANT_STATE 1
#define DISTRHO_UI_USE_NANOVG 1
#define DISTRHO_UI_USER_RESIZABLE 1
#define DISTRHO_UI_DEFAULT_WIDTH 520
#if defined(STORMSURGE_PREVIEW)
#define DISTRHO_UI_DEFAULT_HEIGHT 900
#elif defined(STORMSURGE_MAELSTROM)
// Matches the shorter enclosure approved in issue #26.
#define DISTRHO_UI_DEFAULT_HEIGHT 680
#else
#define DISTRHO_UI_DEFAULT_HEIGHT 680
#endif
#define DISTRHO_UI_FILE_BROWSER 0
