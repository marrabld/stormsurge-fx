// SPDX-License-Identifier: GPL-3.0-only
#include "src/DistrhoUIInternal.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

START_NAMESPACE_DISTRHO
const char *getPluginFormatName() noexcept
{
    return "VST3";
}
END_NAMESPACE_DISTRHO

int main()
{
    using namespace DISTRHO;
    // A real instance of our editor, without a DSP instance, server or audio
    // device. There are no editable audio parameters in the plugin foundation.
    UIExporter editor(nullptr, 0, 48000.0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr);
    editor.setWindowTitle("Undertow Foundation: GUI smoke test, no audio");
    editor.setWindowVisible(true);
    editor.focus();
    for (int frame = 0; frame < 30; ++frame) {
        if (!editor.plugin_idle())
            return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    uint width = 0, height = 0;
    bool aspect = false;
    editor.getGeometryConstraints(width, height, aspect);
    if (width != 390 || height != 510 || !aspect) {
        std::fprintf(stderr, "Unexpected GUI geometry constraints\n");
        return 1;
    }
    editor.quit();
    std::puts("PASS: pedal editor creation, painting and destruction without audio devices");
    return 0;
}
