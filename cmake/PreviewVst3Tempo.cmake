# SPDX-License-Identifier: GPL-3.0-only
# Build-only adapter changes: raw host tempo and tail reporting. Never edit the
# pinned submodule or Foundation target.
set(STORMSURGE_DISTRHO "${CMAKE_CURRENT_SOURCE_DIR}/external/dpf/distrho")
set(adapter "${STORMSURGE_DISTRHO}/src/DistrhoPluginVST3.cpp")
set(original_wrapper "${STORMSURGE_DISTRHO}/DistrhoPluginMain.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${adapter}" "${original_wrapper}")
file(READ "${original_wrapper}" wrapper_source)
string(REPLACE "\r\n" "\n" wrapper_source "${wrapper_source}")
string(SHA256 wrapper_hash "${wrapper_source}")
if(NOT wrapper_hash STREQUAL "7ec44204ac3e16f3d0c126eb25f29409b785b01d1debd8ccb7d607b4b946d57e")
    message(FATAL_ERROR "Review Preview's replacement DSP wrapper against updated DPF")
endif()
file(READ "${adapter}" adapter_source)
string(REPLACE "\r\n" "\n" adapter_source "${adapter_source}")
string(SHA256 adapter_hash "${adapter_source}")
if(NOT adapter_hash STREQUAL "462facea5ac0816be7fdd119e242932256bf1a9eb585aea28ba8bcc26388299a")
    message(FATAL_ERROR "Review Preview's adapter bridges against the updated DPF adapter")
endif()
set(needle [=[        if (! fPlugin.isActive())
            fPlugin.activate();]=])
string(REPLACE "\r\n" "\n" needle "${needle}")
string(FIND "${adapter_source}" "${needle}" position)
if(position LESS 0)
    message(FATAL_ERROR "Cannot locate Preview's VST3 tempo bridge insertion point")
endif()
set(replacement [=[        stormsurge::supplyPreviewTempo(fPlugin.getInstancePointer(), data->ctx,
                                      V3_PROCESS_CTX_TEMPO_VALID);
        if (! fPlugin.isActive())
            fPlugin.activate();]=])
string(REPLACE "\r\n" "\n" replacement "${replacement}")
string(REPLACE "${needle}" "${replacement}" adapter_source "${adapter_source}")
set(tail_needle [=[    uint32_t getTailSamples() const noexcept
    {
        return 0;
    }]=])
string(REPLACE "\r\n" "\n" tail_needle "${tail_needle}")
string(FIND "${adapter_source}" "${tail_needle}" tail_position)
if(tail_position LESS 0)
    message(FATAL_ERROR "Cannot locate Preview's VST3 tail insertion point")
endif()
set(tail_replacement [=[    uint32_t getTailSamples() const noexcept
    {
        return stormsurge::previewTailSamples(fPlugin.getInstancePointer());
    }]=])
string(REPLACE "\r\n" "\n" tail_replacement "${tail_replacement}")
string(REPLACE "${tail_needle}" "${tail_replacement}" adapter_source "${adapter_source}")
set(STORMSURGE_PREVIEW_ADAPTER "${CMAKE_CURRENT_BINARY_DIR}/preview-adapter")
file(MAKE_DIRECTORY "${STORMSURGE_PREVIEW_ADAPTER}")
file(WRITE "${STORMSURGE_PREVIEW_ADAPTER}/DistrhoPluginVST3.cpp" "${adapter_source}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PreviewVst3Main.cpp.in"
    "${STORMSURGE_PREVIEW_ADAPTER}/PreviewVst3Main.cpp" @ONLY)

get_target_property(preview_sources UndertowPreview-vst3 SOURCES)
set(replaced 0)
set(patched_sources)
foreach(source IN LISTS preview_sources)
    if(source MATCHES "(^|/)DistrhoPluginMain\\.cpp$")
        list(APPEND patched_sources "${STORMSURGE_PREVIEW_ADAPTER}/PreviewVst3Main.cpp")
        math(EXPR replaced "${replaced} + 1")
    else()
        list(APPEND patched_sources "${source}")
    endif()
endforeach()
if(NOT replaced EQUAL 1)
    message(FATAL_ERROR "Expected one Preview VST3 DSP wrapper; review DPF target wiring")
endif()
set_property(TARGET UndertowPreview-vst3 PROPERTY SOURCES "${patched_sources}")
target_include_directories(UndertowPreview-vst3 PRIVATE "${STORMSURGE_DISTRHO}/src")
