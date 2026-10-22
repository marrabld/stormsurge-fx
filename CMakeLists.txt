cmake_minimum_required(VERSION 3.24)
project(StormSurgeFX VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STORMSURGE_SANITIZERS "Enable address/undefined sanitizers on supported compilers" OFF)
if(STORMSURGE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "Use Clang/GCC for this sanitizer preset")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/dpf/dgl/src/pugl-upstream/include/pugl/pugl.h")
    message(FATAL_ERROR "Missing dependencies. Run git submodule update --init --recursive")
endif()
add_subdirectory(external/dpf EXCLUDE_FROM_ALL)

# Mac's first preview is plugin-only. Native standalone devices need separate
# consent/routing work; do not enable an audio fallback to get a Mac executable.
if(APPLE)
    set(foundation_formats vst3)
    set(preview_formats vst3 au)
    set(maelstrom_formats vst3 au)
    set(whitecap_formats "")
    set(squall_formats "")
else()
    set(foundation_formats vst3 jack)
    set(preview_formats vst3 jack)
    # Maelstrom's first integration slice is plugin-only: its bypass has no
    # monitoring gate, so a standalone app would open a device with no consent UI.
    set(maelstrom_formats vst3)
    set(whitecap_formats vst3)
    set(squall_formats vst3)
endif()

# JACK-only standalone: never silently open a fallback native audio device.
# Routing is external for this foundation, not a finished device-selection UI.
dpf_add_plugin(UndertowFoundation
    TARGETS ${foundation_formats}
    UI_TYPE opengl
    SKIP_NATIVE_AUDIO_FALLBACK
    FILES_DSP src/Plugin.cpp
    FILES_UI src/Editor.cpp)
target_include_directories(UndertowFoundation PUBLIC src)

# Separate identity: old pass-through sessions must not silently acquire delay.
dpf_add_plugin(UndertowPreview
    TARGETS ${preview_formats}
    UI_TYPE opengl
    SKIP_NATIVE_AUDIO_FALLBACK
    FILES_DSP src/PreviewPlugin.cpp src/DarkDelay.cpp src/DarkReverb.cpp
    FILES_UI src/PreviewEditor.cpp)
target_include_directories(UndertowPreview PUBLIC src)
target_compile_definitions(UndertowPreview PUBLIC STORMSURGE_PREVIEW)
include(cmake/PreviewArtwork.cmake)
target_include_directories(UndertowPreview-ui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
include(cmake/PreviewVst3Tempo.cmake)
if(APPLE)
    include(cmake/PreviewAuTempo.cmake)
endif()

# Third pedal: a tightening overdrive that feeds an amp model. Its own identity,
# parameter space and state schema, so Undertow projects never change.
dpf_add_plugin(Maelstrom
    TARGETS ${maelstrom_formats}
    UI_TYPE opengl
    SKIP_NATIVE_AUDIO_FALLBACK
    FILES_DSP src/MaelstromPlugin.cpp src/MaelstromOverdrive.cpp
    FILES_UI src/MaelstromEditor.cpp)
target_include_directories(Maelstrom PUBLIC src)
target_compile_definitions(Maelstrom PUBLIC STORMSURGE_MAELSTROM)
include(cmake/MaelstromArtwork.cmake)
target_include_directories(Maelstrom-ui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
include(cmake/ConceptMockupArtwork.cmake)

# Whitecap's first plugin slice targets Bitwig on Linux and VST3 hosts on Windows.
# Its DSP remains independent; no standalone device or Mac plugin is added here.
if(whitecap_formats)
    dpf_add_plugin(Whitecap
        TARGETS ${whitecap_formats}
        UI_TYPE opengl
        FILES_DSP src/WhitecapPlugin.cpp src/Whitecap.cpp
        FILES_UI src/WhitecapEditor.cpp)
    target_include_directories(Whitecap PUBLIC src)
    target_compile_definitions(Whitecap PUBLIC STORMSURGE_WHITECAP)
    target_include_directories(Whitecap-ui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endif()

# Squall's first integration slice is a recorded-vocal VST3 preview only.
if(squall_formats)
    dpf_add_plugin(Squall
        TARGETS ${squall_formats}
        UI_TYPE opengl
        FILES_DSP src/SquallPlugin.cpp src/Squall.cpp
        FILES_UI src/SquallEditor.cpp)
    target_include_directories(Squall PUBLIC src)
    target_compile_definitions(Squall PUBLIC STORMSURGE_SQUALL)
    target_include_directories(Squall-ui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endif()
# DPF's bundled self-test includes its POSIX thread helper, unavailable to MSVC.
# Our headless processor tests still run on both platforms.
if(NOT WIN32)
    target_compile_definitions(UndertowFoundation PUBLIC DPF_RUNTIME_TESTING)
    target_compile_definitions(UndertowPreview PUBLIC DPF_RUNTIME_TESTING)
    if(TARGET Whitecap)
        target_compile_definitions(Whitecap PUBLIC DPF_RUNTIME_TESTING)
    endif()
    if(TARGET Squall)
        target_compile_definitions(Squall PUBLIC DPF_RUNTIME_TESTING)
    endif()
endif()

# At this pinned DPF revision the OpenGL helper treats generator-expression
# booleans as true while configuring optional browser code. This foundation has
# neither a file browser nor a web view. Do not build those optional code paths.
foreach(property COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(dgl_definitions dgl-opengl ${property})
    list(REMOVE_ITEM dgl_definitions DGL_USE_FILE_BROWSER DGL_USE_WEB_VIEW)
    set_target_properties(dgl-opengl PROPERTIES ${property} "${dgl_definitions}")
endforeach()

if(WIN32)
    include(cmake/KhronosHeaders.cmake)
    target_include_directories(dgl-opengl BEFORE PUBLIC "${STORMSURGE_KHRONOS_DIR}")
endif()

# Carry the upstream notices with development binaries, including the embedded
# font and rendering helpers. Some licence notices live inside source headers;
# retain those files intact rather than extracting incomplete paragraphs.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/notices")
configure_file(assets/runtime/NOTICE.md "${CMAKE_BINARY_DIR}/bin/notices/Undertow-artwork.md" COPYONLY)
configure_file(assets/runtime/MAELSTROM-NOTICE.md
    "${CMAKE_BINARY_DIR}/bin/notices/Maelstrom-artwork.md" COPYONLY)
if(TARGET Whitecap)
    configure_file(assets/runtime/FUTURE-PEDALS-NOTICE.md
        "${CMAKE_BINARY_DIR}/bin/notices/Whitecap-artwork.md" COPYONLY)
endif()
if(TARGET Squall)
    configure_file(assets/runtime/SQUALL-NOTICE.md
        "${CMAKE_BINARY_DIR}/bin/notices/Squall-artwork.md" COPYONLY)
endif()
configure_file(external/dpf/LICENSE "${CMAKE_BINARY_DIR}/bin/notices/DPF-LICENSE.txt" COPYONLY)
configure_file(external/dpf/dgl/src/pugl-upstream/COPYING "${CMAKE_BINARY_DIR}/bin/notices/Pugl-COPYING.txt" COPYONLY)
configure_file(external/dpf/dgl/src/nanovg/LICENSE.txt "${CMAKE_BINARY_DIR}/bin/notices/NanoVG-LICENSE.txt" COPYONLY)
configure_file(external/dpf/dgl/src/resources/LICENSE-DejaVuSans.ttf.txt
    "${CMAKE_BINARY_DIR}/bin/notices/DejaVuSans-LICENSE.txt" COPYONLY)
foreach(header fontstash.h stb_truetype.h stb_image.h)
    configure_file("external/dpf/dgl/src/nanovg/${header}"
        "${CMAKE_BINARY_DIR}/bin/notices/${header}" COPYONLY)
endforeach()
if(WIN32)
    file(COPY "${STORMSURGE_KHRONOS_DIR}/" DESTINATION "${CMAKE_BINARY_DIR}/bin/notices/khronos")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(dark_delay_tests tests/DarkDelayTests.cpp src/DarkDelay.cpp)
    target_include_directories(dark_delay_tests PRIVATE src)
    add_test(NAME dark_delay_core COMMAND dark_delay_tests)
    add_executable(tempo_sync_tests tests/TempoSyncTests.cpp src/DarkDelay.cpp)
    target_include_directories(tempo_sync_tests PRIVATE src)
    add_test(NAME tempo_sync COMMAND tempo_sync_tests)
    add_executable(au_tempo_bridge_tests tests/AuTempoBridgeTests.cpp)
    target_include_directories(au_tempo_bridge_tests PRIVATE src)
    add_test(NAME au_tempo_bridge COMMAND au_tempo_bridge_tests)
    add_executable(squall_tests tests/SquallTests.cpp src/Squall.cpp)
    target_include_directories(squall_tests PRIVATE src)
    add_test(NAME squall_core COMMAND squall_tests)
    add_executable(riptide_tests tests/RiptideTests.cpp src/Riptide.cpp)
    target_include_directories(riptide_tests PRIVATE src)
    add_test(NAME riptide_core COMMAND riptide_tests)
    add_executable(riptide_audition
        tools/RiptideAudition.cpp tools/AuditionRender.cpp tools/WavFile.cpp src/Riptide.cpp)
    target_include_directories(riptide_audition PRIVATE src tools)
    add_executable(riptide_audition_tests tests/RiptideAuditionTests.cpp
        tools/AuditionRender.cpp tools/WavFile.cpp src/Riptide.cpp)
    target_include_directories(riptide_audition_tests PRIVATE src tools)
    add_test(NAME riptide_audition COMMAND riptide_audition_tests)
    if(APPLE)
        add_executable(mac_au_tests tests/MacAuTests.cpp)
        target_include_directories(mac_au_tests PRIVATE src)
        target_link_libraries(mac_au_tests PRIVATE "-framework AudioToolbox" "-framework AudioUnit" "-framework CoreFoundation")
        add_dependencies(mac_au_tests UndertowPreview-au)
        add_test(NAME mac_au_adapter COMMAND mac_au_tests "${CMAKE_BINARY_DIR}/bin/UndertowPreview.component")
        add_executable(mac_vst3_tests tests/Vst3ModuleTests.cpp)
        target_include_directories(mac_vst3_tests PRIVATE external/dpf/distrho/src)
        target_link_libraries(mac_vst3_tests PRIVATE "-framework CoreFoundation")
        add_dependencies(mac_vst3_tests UndertowPreview-vst3 Maelstrom-vst3)
        add_test(NAME mac_vst3_loader COMMAND mac_vst3_tests "${CMAKE_BINARY_DIR}/bin/UndertowPreview.vst3")
        add_test(NAME mac_maelstrom_vst3_loader
            COMMAND mac_vst3_tests "${CMAKE_BINARY_DIR}/bin/Maelstrom.vst3" "Maelstrom")
    endif()
    # The sanitizer job deliberately builds only headless DSP harnesses. Actual
    # binary loading is checked by the normal and Release platform jobs instead.
    if(NOT APPLE AND NOT STORMSURGE_SANITIZERS)
        add_executable(vst3_loader_tests tests/Vst3ModuleTests.cpp)
        target_include_directories(vst3_loader_tests PRIVATE external/dpf/distrho/src)
        target_link_libraries(vst3_loader_tests PRIVATE ${CMAKE_DL_LIBS})
        if(NOT WIN32)
            find_package(X11 REQUIRED)
            # JACK is only needed by the experimental practice-board audio path. Keep it
            # optional so a plain plugin/editor build does not demand audio packages that
            # CI and most contributors do not need.
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(JACK IMPORTED_TARGET jack)
            endif()
            # These probes instantiate actual Linux bundles and stay out of sanitizer
            # runs. The optional editor --view path is manual; neither opens audio devices.
            add_executable(vst3_processing_tests tests/Vst3ProcessingTests.cpp)
            target_include_directories(vst3_processing_tests PRIVATE external/dpf/distrho/src)
            target_link_libraries(vst3_processing_tests PRIVATE ${CMAKE_DL_LIBS})
            add_dependencies(vst3_processing_tests UndertowPreview-vst3)
            add_executable(vst3_editor_tests tests/Vst3EditorTests.cpp)
            target_include_directories(vst3_editor_tests PRIVATE external/dpf/distrho/src
                "${CMAKE_CURRENT_BINARY_DIR}/generated")
            target_link_libraries(vst3_editor_tests PRIVATE ${CMAKE_DL_LIBS} X11::X11)
            if(JACK_FOUND)
                target_compile_definitions(vst3_editor_tests PRIVATE STORMSURGE_HAVE_JACK=1)
                target_link_libraries(vst3_editor_tests PRIVATE PkgConfig::JACK)
            endif()
            add_dependencies(vst3_editor_tests UndertowPreview-vst3)
            if(TARGET Maelstrom)
                add_dependencies(vst3_processing_tests Maelstrom-vst3)
                add_dependencies(vst3_editor_tests Maelstrom-vst3)
            endif()
            if(TARGET Whitecap)
                add_dependencies(vst3_processing_tests Whitecap-vst3)
                add_dependencies(vst3_editor_tests Whitecap-vst3)
            endif()
            if(TARGET Squall)
                add_dependencies(vst3_processing_tests Squall-vst3)
                add_dependencies(vst3_editor_tests Squall-vst3)
            endif()
        endif()
        add_dependencies(vst3_loader_tests UndertowPreview-vst3)
        if(TARGET Whitecap)
            add_dependencies(vst3_loader_tests Whitecap-vst3)
        endif()
        if(TARGET Squall)
            add_dependencies(vst3_loader_tests Squall-vst3)
        endif()
        if(WIN32)
            set(preview_module "${CMAKE_BINARY_DIR}/bin/UndertowPreview.vst3/Contents/x86_64-win/UndertowPreview.vst3")
        else()
            set(preview_module "${CMAKE_BINARY_DIR}/bin/UndertowPreview.vst3/Contents/x86_64-linux/UndertowPreview.so")
        endif()
        add_test(NAME preview_vst3_loader COMMAND vst3_loader_tests "${preview_module}" "Undertow Preview" 0.4.0)
        if(NOT WIN32)
            add_test(NAME preview_vst3_process
                COMMAND vst3_processing_tests "${preview_module}")
            add_test(NAME maelstrom_vst3_process
                COMMAND vst3_processing_tests
                    "${CMAKE_BINARY_DIR}/bin/Maelstrom.vst3/Contents/x86_64-linux/Maelstrom.so")
            add_test(NAME preview_vst3_editor
                COMMAND vst3_editor_tests "${preview_module}")
            add_test(NAME maelstrom_vst3_editor
                COMMAND vst3_editor_tests
                    "${CMAKE_BINARY_DIR}/bin/Maelstrom.vst3/Contents/x86_64-linux/Maelstrom.so")
            if(TARGET Whitecap)
                add_test(NAME whitecap_vst3_process
                    COMMAND vst3_processing_tests
                        "${CMAKE_BINARY_DIR}/bin/Whitecap.vst3/Contents/x86_64-linux/Whitecap.so")
                add_test(NAME whitecap_vst3_editor
                    COMMAND vst3_editor_tests
                        "${CMAKE_BINARY_DIR}/bin/Whitecap.vst3/Contents/x86_64-linux/Whitecap.so")
            endif()
            if(TARGET Squall)
                add_test(NAME squall_vst3_process
                    COMMAND vst3_processing_tests
                        "${CMAKE_BINARY_DIR}/bin/Squall.vst3/Contents/x86_64-linux/Squall.so")
                add_test(NAME squall_vst3_editor
                    COMMAND vst3_editor_tests
                        "${CMAKE_BINARY_DIR}/bin/Squall.vst3/Contents/x86_64-linux/Squall.so")
            endif()
        endif()
        if(TARGET Whitecap)
            if(WIN32)
                set(whitecap_module
                    "${CMAKE_BINARY_DIR}/bin/Whitecap.vst3/Contents/x86_64-win/Whitecap.vst3")
            else()
                set(whitecap_module
                    "${CMAKE_BINARY_DIR}/bin/Whitecap.vst3/Contents/x86_64-linux/Whitecap.so")
            endif()
            add_test(NAME whitecap_vst3_loader
                COMMAND vst3_loader_tests "${whitecap_module}" "Whitecap" 0.1.0)
        endif()
        if(TARGET Squall)
            if(WIN32)
                set(squall_module
                    "${CMAKE_BINARY_DIR}/bin/Squall.vst3/Contents/x86_64-win/Squall.vst3")
            else()
                set(squall_module
                    "${CMAKE_BINARY_DIR}/bin/Squall.vst3/Contents/x86_64-linux/Squall.so")
            endif()
            add_test(NAME squall_vst3_loader
                COMMAND vst3_loader_tests "${squall_module}" "Squall" 0.1.0)
        endif()
        if(WIN32)
            set(maelstrom_module
                "${CMAKE_BINARY_DIR}/bin/Maelstrom.vst3/Contents/x86_64-win/Maelstrom.vst3")
        else()
            set(maelstrom_module
                "${CMAKE_BINARY_DIR}/bin/Maelstrom.vst3/Contents/x86_64-linux/Maelstrom.so")
        endif()
        add_test(NAME maelstrom_vst3_loader
            COMMAND vst3_loader_tests "${maelstrom_module}" "Maelstrom" 0.4.0)
    endif()
    add_executable(dark_reverb_tests tests/DarkReverbTests.cpp src/DarkReverb.cpp src/DarkDelay.cpp)
    target_include_directories(dark_reverb_tests PRIVATE src)
    add_test(NAME dark_reverb_core COMMAND dark_reverb_tests)
    add_executable(whitecap_tests tests/WhitecapTests.cpp src/Whitecap.cpp)
    target_include_directories(whitecap_tests PRIVATE src)
    add_test(NAME whitecap_core COMMAND whitecap_tests)
    if(TARGET Whitecap)
        add_executable(whitecap_plugin_tests tests/WhitecapPluginTests.cpp
            src/WhitecapPlugin.cpp src/Whitecap.cpp
            external/dpf/distrho/src/DistrhoPlugin.cpp)
        target_include_directories(whitecap_plugin_tests PRIVATE src external/dpf/distrho)
        target_compile_definitions(whitecap_plugin_tests PRIVATE STORMSURGE_WHITECAP)
        if(NOT WIN32)
            target_link_libraries(whitecap_plugin_tests PRIVATE ${CMAKE_DL_LIBS})
        endif()
        add_test(NAME whitecap_plugin COMMAND whitecap_plugin_tests vst3)
    endif()
    add_executable(maelstrom_tests tests/MaelstromTests.cpp src/MaelstromOverdrive.cpp)
    target_include_directories(maelstrom_tests PRIVATE src)
    add_test(NAME maelstrom_overdrive COMMAND maelstrom_tests)
    add_executable(maelstrom_plugin_tests tests/MaelstromPluginTests.cpp
        src/MaelstromPlugin.cpp src/MaelstromOverdrive.cpp
        external/dpf/distrho/src/DistrhoPlugin.cpp)
    target_include_directories(maelstrom_plugin_tests PRIVATE src external/dpf/distrho)
    target_compile_definitions(maelstrom_plugin_tests PRIVATE STORMSURGE_MAELSTROM)
    if(NOT WIN32)
        target_link_libraries(maelstrom_plugin_tests PRIVATE ${CMAKE_DL_LIBS})
    endif()
    add_test(NAME maelstrom_plugin COMMAND maelstrom_plugin_tests vst3)
    add_executable(squall_plugin_tests tests/SquallPluginTests.cpp
        src/SquallPlugin.cpp src/Squall.cpp
        external/dpf/distrho/src/DistrhoPlugin.cpp)
    target_include_directories(squall_plugin_tests PRIVATE src external/dpf/distrho)
    target_compile_definitions(squall_plugin_tests PRIVATE STORMSURGE_SQUALL)
    if(NOT WIN32)
        target_link_libraries(squall_plugin_tests PRIVATE ${CMAKE_DL_LIBS})
    endif()
    add_test(NAME squall_plugin COMMAND squall_plugin_tests vst3)
    add_executable(preview_tests tests/PreviewTests.cpp src/PreviewPlugin.cpp src/DarkDelay.cpp src/DarkReverb.cpp
        external/dpf/distrho/src/DistrhoPlugin.cpp)
    target_include_directories(preview_tests PRIVATE src external/dpf/distrho)
    target_compile_definitions(preview_tests PRIVATE STORMSURGE_PREVIEW)
    if(NOT WIN32)
        target_link_libraries(preview_tests PRIVATE ${CMAKE_DL_LIBS})
    endif()
    add_test(NAME preview_plugin COMMAND preview_tests vst3)
    add_test(NAME preview_monitor_gate COMMAND preview_tests standalone)
    add_executable(foundation_tests
        tests/FoundationTests.cpp
        src/Plugin.cpp
        external/dpf/distrho/src/DistrhoPlugin.cpp)
    target_include_directories(foundation_tests PRIVATE src external/dpf/distrho)
    if(NOT WIN32)
        target_link_libraries(foundation_tests PRIVATE ${CMAKE_DL_LIBS})
    endif()
    add_test(NAME plugin_passthrough COMMAND foundation_tests vst3)
    add_test(NAME standalone_monitor_gate COMMAND foundation_tests standalone)
    add_executable(foundation_gui
        tests/GuiSmoke.cpp src/Editor.cpp
        external/dpf/distrho/src/DistrhoUI.cpp)
    target_include_directories(foundation_gui PRIVATE src external/dpf/distrho)
    target_compile_definitions(foundation_gui PRIVATE STORMSURGE_GUI_HARNESS)
    target_link_libraries(foundation_gui PRIVATE dgl-opengl)
    add_executable(preview_gui tests/GuiSmoke.cpp src/PreviewEditor.cpp
        external/dpf/distrho/src/DistrhoUI.cpp)
    target_include_directories(preview_gui PRIVATE src external/dpf/distrho)
    target_include_directories(preview_gui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(preview_gui PRIVATE STORMSURGE_PREVIEW STORMSURGE_GUI_HARNESS)
    target_link_libraries(preview_gui PRIVATE dgl-opengl)
    if(TARGET Whitecap)
        add_executable(whitecap_gui tests/WhitecapGuiSmoke.cpp src/WhitecapEditor.cpp
            external/dpf/distrho/src/DistrhoUI.cpp)
        target_include_directories(whitecap_gui PRIVATE src external/dpf/distrho
            "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(whitecap_gui PRIVATE STORMSURGE_WHITECAP STORMSURGE_GUI_HARNESS)
        target_link_libraries(whitecap_gui PRIVATE dgl-opengl)
    endif()
    if(TARGET Squall)
        add_executable(squall_gui tests/SquallGuiSmoke.cpp src/SquallEditor.cpp
            external/dpf/distrho/src/DistrhoUI.cpp)
        target_include_directories(squall_gui PRIVATE src external/dpf/distrho
            "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(squall_gui PRIVATE STORMSURGE_SQUALL STORMSURGE_GUI_HARNESS)
        target_link_libraries(squall_gui PRIVATE dgl-opengl)
    endif()
    # Display-only prototype, not a plugin/standalone processor or release payload.
    configure_file(assets/runtime/MAELSTROM-NOTICE.md
        "${CMAKE_CURRENT_BINARY_DIR}/mockup-notices/Maelstrom-artwork.md" COPYONLY)
    add_executable(maelstrom_mockup tests/MaelstromMockup.cpp
        external/dpf/distrho/src/DistrhoUI.cpp)
    target_include_directories(maelstrom_mockup PRIVATE src external/dpf/distrho
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(maelstrom_mockup PRIVATE STORMSURGE_GUI_HARNESS)
    target_link_libraries(maelstrom_mockup PRIVATE dgl-opengl)
    configure_file(assets/runtime/FUTURE-PEDALS-NOTICE.md
        "${CMAKE_CURRENT_BINARY_DIR}/concept-mockup-notices/Future-pedals-artwork.md" COPYONLY)
    configure_file(assets/runtime/SQUALL-NOTICE.md
        "${CMAKE_CURRENT_BINARY_DIR}/concept-mockup-notices/Squall-artwork.md" COPYONLY)
    add_executable(future_pedal_mockup tests/FuturePedalMockup.cpp
        external/dpf/distrho/src/DistrhoUI.cpp)
    target_include_directories(future_pedal_mockup PRIVATE src external/dpf/distrho
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(future_pedal_mockup PRIVATE STORMSURGE_GUI_HARNESS)
    target_link_libraries(future_pedal_mockup PRIVATE dgl-opengl)
    include(cmake/PracticeBoardArtwork.cmake)
    configure_file(assets/runtime/PEDAL-BOARD-NOTICE.md
        "${CMAKE_CURRENT_BINARY_DIR}/practice-board-notices/Pedal-board-artwork.md" COPYONLY)
    add_executable(practice_board_gui tests/PracticeBoardGui.cpp
        external/dpf/distrho/src/DistrhoUI.cpp)
    target_include_directories(practice_board_gui PRIVATE src external/dpf/distrho
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(practice_board_gui PRIVATE STORMSURGE_GUI_HARNESS)
    target_link_libraries(practice_board_gui PRIVATE dgl-opengl)
    add_executable(squall_mockup tests/SquallMockup.cpp
        external/dpf/distrho/src/DistrhoUI.cpp)
    target_include_directories(squall_mockup PRIVATE src external/dpf/distrho
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(squall_mockup PRIVATE STORMSURGE_GUI_HARNESS)
    target_link_libraries(squall_mockup PRIVATE dgl-opengl)
    if(APPLE)
        # DPF adds this Cocoa scale helper to plugin UI libraries, but these
        # no-audio harnesses compile DistrhoUI.cpp directly instead.
        target_sources(foundation_gui PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
        target_sources(preview_gui PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
        if(TARGET whitecap_gui)
            target_sources(whitecap_gui PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
        endif()
        if(TARGET squall_gui)
            target_sources(squall_gui PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
        endif()
        target_sources(maelstrom_mockup PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
        target_sources(future_pedal_mockup PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
        target_sources(practice_board_gui PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
        target_sources(squall_mockup PRIVATE external/dpf/distrho/DistrhoUI_macOS.mm)
    endif()
    if(NOT WIN32)
        target_link_libraries(foundation_gui PRIVATE ${CMAKE_DL_LIBS})
        target_link_libraries(preview_gui PRIVATE ${CMAKE_DL_LIBS})
        if(TARGET whitecap_gui)
            target_link_libraries(whitecap_gui PRIVATE ${CMAKE_DL_LIBS})
        endif()
        if(TARGET squall_gui)
            target_link_libraries(squall_gui PRIVATE ${CMAKE_DL_LIBS})
        endif()
        target_link_libraries(maelstrom_mockup PRIVATE ${CMAKE_DL_LIBS})
        target_link_libraries(future_pedal_mockup PRIVATE ${CMAKE_DL_LIBS})
        target_link_libraries(practice_board_gui PRIVATE ${CMAKE_DL_LIBS})
        target_link_libraries(squall_mockup PRIVATE ${CMAKE_DL_LIBS})
    endif()
    # GUI smoke is intentionally explicit, since it needs a display/OpenGL context.
    # `UndertowFoundation selftest` uses generated data and never opens audio devices.
endif()
