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

# JACK-only standalone: never silently open a fallback native audio device.
# Routing is external for this foundation, not a finished device-selection UI.
dpf_add_plugin(UndertowFoundation
    TARGETS vst3 jack
    UI_TYPE opengl
    SKIP_NATIVE_AUDIO_FALLBACK
    FILES_DSP src/Plugin.cpp
    FILES_UI src/Editor.cpp)
target_include_directories(UndertowFoundation PUBLIC src)

# Separate identity: old pass-through sessions must not silently acquire delay.
dpf_add_plugin(UndertowPreview
    TARGETS vst3 jack
    UI_TYPE opengl
    SKIP_NATIVE_AUDIO_FALLBACK
    FILES_DSP src/PreviewPlugin.cpp src/DarkDelay.cpp src/DarkReverb.cpp
    FILES_UI src/PreviewEditor.cpp)
target_include_directories(UndertowPreview PUBLIC src)
target_compile_definitions(UndertowPreview PUBLIC STORMSURGE_PREVIEW)
include(cmake/PreviewVst3Tempo.cmake)
# DPF's bundled self-test includes its POSIX thread helper, unavailable to MSVC.
# Our headless processor tests still run on both platforms.
if(NOT WIN32)
    target_compile_definitions(UndertowFoundation PUBLIC DPF_RUNTIME_TESTING)
    target_compile_definitions(UndertowPreview PUBLIC DPF_RUNTIME_TESTING)
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
    add_executable(dark_reverb_tests tests/DarkReverbTests.cpp src/DarkReverb.cpp src/DarkDelay.cpp)
    target_include_directories(dark_reverb_tests PRIVATE src)
    add_test(NAME dark_reverb_core COMMAND dark_reverb_tests)
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
    target_compile_definitions(preview_gui PRIVATE STORMSURGE_PREVIEW STORMSURGE_GUI_HARNESS)
    target_link_libraries(preview_gui PRIVATE dgl-opengl)
    if(NOT WIN32)
        target_link_libraries(foundation_gui PRIVATE ${CMAKE_DL_LIBS})
        target_link_libraries(preview_gui PRIVATE ${CMAKE_DL_LIBS})
    endif()
    # GUI smoke is intentionally explicit, since it needs a display/OpenGL context.
    # `UndertowFoundation selftest` uses generated data and never opens audio devices.
endif()
