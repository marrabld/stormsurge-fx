// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#define DISTRHO_PLUGIN_BRAND "StormSurge FX"
#ifdef STORMSURGE_PREVIEW
#define DISTRHO_PLUGIN_NAME "Undertow Preview"
#define DISTRHO_PLUGIN_URI "https://github.com/marrabld/stormsurge-fx/undertow-preview"
#define DISTRHO_PLUGIN_UNIQUE_ID UtP0
#define DISTRHO_PLUGIN_WANT_TIMEPOS 1
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
#define DISTRHO_UI_DEFAULT_HEIGHT 680
#define DISTRHO_UI_FILE_BROWSER 0
