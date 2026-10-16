// SPDX-License-Identifier: GPL-3.0-only
#include "src/DistrhoUIInternal.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#ifdef STORMSURGE_PREVIEW
#include "PreviewParameters.hpp"
static bool standalonePreview = false;
static unsigned gesturesStarted = 0, gesturesEnded = 0, edits = 0;
static unsigned monitorRequests = 0;
static void editParameter(void *, uint32_t, bool started)
{
    if (started)
        ++gesturesStarted;
    else
        ++gesturesEnded;
}
static void setParameter(void *, uint32_t index, float value)
{
    if (stormsurge::previewEffectIndex(index, standalonePreview) >= 0 && std::isfinite(value))
        ++edits;
    if (standalonePreview && index == stormsurge::MonitorRequest)
        ++monitorRequests;
}
#endif

START_NAMESPACE_DISTRHO
const char *getPluginFormatName() noexcept
{
#ifdef STORMSURGE_PREVIEW
    if (standalonePreview)
        return "JACK/Standalone";
#endif
    return "VST3";
}
#ifdef STORMSURGE_PREVIEW
bool previewExerciseControls();
#endif
END_NAMESPACE_DISTRHO

int main(int argc, char **argv)
{
    const bool view = argc == 2 && std::strcmp(argv[1], "--view") == 0;
#ifdef STORMSURGE_PREVIEW
    standalonePreview = argc == 2 && std::strcmp(argv[1], "--standalone") == 0;
    if (argc != 1 && !view && !standalonePreview) {
        std::fprintf(stderr, "Usage: preview_gui [--view|--standalone]\n");
        return 1;
    }
#else
    if (argc != 1 && !view) {
        std::fprintf(stderr, "Usage: foundation_gui [--view]\n");
        return 1;
    }
#endif
    using namespace DISTRHO;
    // A real instance of our editor, without a DSP instance, server or audio
    // device. This is a UI preview, not a signal-processing or monitoring test.
#ifdef STORMSURGE_PREVIEW
    UIExporter editor(nullptr, 0, 48000.0, editParameter, setParameter, nullptr, nullptr, nullptr,
                      nullptr, nullptr);
#else
    UIExporter editor(nullptr, 0, 48000.0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr);
#endif
#ifdef STORMSURGE_PREVIEW
    editor.setWindowTitle("Undertow Preview: GUI smoke test, no audio");
    editor.parameterChanged(0, 800.0f);
    editor.parameterChanged(1, 35.0f);
    editor.parameterChanged(2, 20.0f);
#else
    editor.setWindowTitle("Undertow Foundation: GUI smoke test, no audio");
#endif
    editor.setWindowVisible(true);
    editor.focus();
#ifdef STORMSURGE_PREVIEW
    if (!view && (!previewExerciseControls() || edits != 23 || gesturesStarted != gesturesEnded ||
                  monitorRequests != (standalonePreview ? 1u : 0u))) {
        std::fprintf(stderr, "FAIL: preview control events or host edit gestures\n");
        return 1;
    }
#endif
    for (int frame = 0; view || frame < 30; ++frame) {
        if (view && !editor.isVisible())
            break;
        if (!editor.plugin_idle()) {
            if (!view)
                return 1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    uint width = 0, height = 0;
    bool aspect = false;
    editor.getGeometryConstraints(width, height, aspect);
#ifdef STORMSURGE_PREVIEW
    const uint expectedHeight = 630;
#else
    const uint expectedHeight = 510;
#endif
    if (width != 390 || height != expectedHeight || !aspect) {
        std::fprintf(stderr, "Unexpected GUI geometry constraints\n");
        return 1;
    }
    editor.quit();
    std::puts("PASS: pedal editor creation, painting and destruction without audio devices");
    return 0;
}
