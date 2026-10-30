// SPDX-License-Identifier: GPL-3.0-only
#include "FloodGateParameters.hpp"
#include "src/DistrhoUIInternal.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

static unsigned gesturesStarted = 0;
static unsigned gesturesEnded = 0;
static unsigned edits = 0;

static void editParameter(void *, uint32_t, bool started)
{
    if (started)
        ++gesturesStarted;
    else
        ++gesturesEnded;
}

static void setParameter(void *, uint32_t index, float value)
{
    if (index < stormsurge::FloodGateParameterCount && std::isfinite(value))
        ++edits;
}

START_NAMESPACE_DISTRHO
const char *getPluginFormatName() noexcept
{
    return "VST3";
}
bool floodGateExerciseControls();
void floodGateResizeForTest();
END_NAMESPACE_DISTRHO

int main(int argc, char **argv)
{
    const bool view = argc == 2 && std::strcmp(argv[1], "--view") == 0;
    const bool minimumSize = argc == 2 && std::strcmp(argv[1], "--small") == 0;
    const bool snapshot = argc == 2 && std::strcmp(argv[1], "--snapshot") == 0;
    if (argc != 1 && !view && !minimumSize && !snapshot) {
        std::fprintf(stderr, "Usage: floodgate_gui [--view|--small|--snapshot]\n");
        return 1;
    }

    using namespace DISTRHO;
    UIExporter editor(nullptr, 0, 48000.0, editParameter, setParameter, nullptr, nullptr, nullptr,
                      nullptr, nullptr);
    editor.setWindowTitle("FloodGate VST3 editor: GUI smoke test, no audio");
    if (minimumSize)
        floodGateResizeForTest();
    editor.setWindowVisible(true);
    editor.focus();
    if (!view && !snapshot &&
        (!floodGateExerciseControls() || edits != 5 || gesturesStarted != 5 ||
         gesturesEnded != gesturesStarted)) {
        std::fprintf(stderr, "FAIL: FloodGate controls or host edit gestures\n");
        return 1;
    }

    const uint expectedWidth = 390, expectedHeight = 510;
    if (minimumSize &&
        (editor.getWidth() != expectedWidth || editor.getHeight() != expectedHeight)) {
        std::fprintf(stderr, "FAIL: minimum-size FloodGate editor geometry\n");
        return 1;
    }
    uint width = 0, height = 0;
    bool aspect = false;
    editor.getGeometryConstraints(width, height, aspect);
    if (width != expectedWidth || height != expectedHeight || !aspect) {
        std::fprintf(stderr, "FAIL: FloodGate editor geometry constraints\n");
        return 1;
    }
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
    editor.quit();
    std::puts("PASS: FloodGate editor controls and geometry without audio devices");
    return 0;
}
