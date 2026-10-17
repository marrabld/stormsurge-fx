# SPDX-License-Identifier: GPL-3.0-only
# Preview-only build copy. Keep the dependency checkout and Foundation unchanged.
set(adapter "${STORMSURGE_DISTRHO}/src/DistrhoPluginAU.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${adapter}")
file(READ "${adapter}" adapter_source)
string(REPLACE "\r\n" "\n" adapter_source "${adapter_source}")
string(SHA256 adapter_hash "${adapter_source}")
if(NOT adapter_hash STREQUAL "ed38f675014bf6ee1e2e4013d88783c83350380e7eb474f51523723086ed70b8")
    message(FATAL_ERROR "Review Preview's AU tempo bridge against the updated DPF adapter")
endif()

set(needle [=[       #if DISTRHO_PLUGIN_WANT_TIMEPOS
        if (fHostCallbackInfo.beatAndTempoProc != nullptr ||]=])
set(replacement [=[       #if DISTRHO_PLUGIN_WANT_TIMEPOS
        const auto previewClock = stormsurge::supplyPreviewAuTempo(fPlugin.getInstancePointer(),
                                                                  fHostCallbackInfo);
        if (fHostCallbackInfo.beatAndTempoProc != nullptr ||]=])
string(FIND "${adapter_source}" "${needle}" position)
if(position LESS 0)
    message(FATAL_ERROR "Cannot locate Preview's AU raw-tempo insertion point")
endif()
string(REPLACE "${needle}" "${replacement}" adapter_source "${adapter_source}")

set(needle [=[            if (fHostCallbackInfo.beatAndTempoProc != nullptr
                && fHostCallbackInfo.beatAndTempoProc(fHostCallbackInfo.hostUserData, &g1, &g2) == noErr)]=])
set(replacement [=[            g1 = previewClock.beat;
            g2 = previewClock.bpm;
            if (previewClock.valid)]=])
string(FIND "${adapter_source}" "${needle}" position)
if(position LESS 0)
    message(FATAL_ERROR "Cannot locate Preview's AU beat/tempo callback")
endif()
string(REPLACE "${needle}" "${replacement}" adapter_source "${adapter_source}")
file(WRITE "${STORMSURGE_PREVIEW_ADAPTER}/DistrhoPluginAU.cpp" "${adapter_source}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PreviewAuMain.cpp.in"
    "${STORMSURGE_PREVIEW_ADAPTER}/PreviewAuMain.cpp" @ONLY)

get_target_property(preview_sources UndertowPreview-au SOURCES)
set(replaced 0)
set(patched_sources)
foreach(source IN LISTS preview_sources)
    if(source MATCHES "(^|/)DistrhoPluginMain\\.cpp$")
        list(APPEND patched_sources "${STORMSURGE_PREVIEW_ADAPTER}/PreviewAuMain.cpp")
        math(EXPR replaced "${replaced} + 1")
    else()
        list(APPEND patched_sources "${source}")
    endif()
endforeach()
if(NOT replaced EQUAL 1)
    message(FATAL_ERROR "Expected one Preview AU DSP wrapper; review DPF target wiring")
endif()
set_property(TARGET UndertowPreview-au PROPERTY SOURCES "${patched_sources}")
target_include_directories(UndertowPreview-au PRIVATE "${STORMSURGE_DISTRHO}/src")
