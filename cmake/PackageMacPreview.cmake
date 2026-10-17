# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.24)
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" OR NOT IS_ABSOLUTE "${BINARY_DIR}"
        OR NOT IS_ABSOLUTE "${SOURCE_DIR}" OR NOT EXISTS "${BINARY_DIR}/CMakeCache.txt"
        OR NOT IS_DIRECTORY "${BINARY_DIR}/bin")
    message(FATAL_ERROR "Run on macOS after a preset build, with BINARY_DIR and SOURCE_DIR")
endif()
file(STRINGS "${BINARY_DIR}/CMakeCache.txt" deployment
    REGEX "^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]+=")
string(REGEX REPLACE "^[^=]*=" "" deployment "${deployment}")
if(deployment STREQUAL "")
    message(FATAL_ERROR "Missing deployment target in the configured Mac build")
endif()

function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Mac tester packaging command failed: ${ARGV}")
    endif()
endfunction()

execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot identify the source commit for the tester package")
endif()
set(stage "${BINARY_DIR}/macos-tester-${commit}/UndertowPreview")
file(MAKE_DIRECTORY "${stage}")
foreach(format vst3 component)
    set(bundle "${BINARY_DIR}/bin/UndertowPreview.${format}")
    if(NOT EXISTS "${bundle}/Contents/MacOS/UndertowPreview")
        message(FATAL_ERROR "Missing Mac plugin: ${bundle}")
    endif()
    run_checked(/usr/bin/lipo -verify_arch arm64 x86_64 "${bundle}/Contents/MacOS/UndertowPreview")
    file(COPY "${bundle}" DESTINATION "${stage}")
endforeach()
file(COPY "${BINARY_DIR}/bin/notices" DESTINATION "${stage}")
file(COPY "${SOURCE_DIR}/LICENSE" "${SOURCE_DIR}/THIRD_PARTY_NOTICES.md" DESTINATION "${stage}")
configure_file("${SOURCE_DIR}/docs/macos.md" "${stage}/READ-ME-FIRST.md" COPYONLY)
file(WRITE "${stage}/BUILD.txt"
    "Undertow Preview experimental Mac tester build\nSource commit: ${commit}\n"
    "Architectures: arm64 and x86_64\nDeployment target: macOS ${deployment}\n"
    "Ad-hoc signatures only. Not Developer ID signed or notarized.\n"
    "No standalone app. No installer. No real-DAW playback claim from CI.\n")

# DPF's AU exporter supplies the processor version. Match the VST3's generic
# bundle metadata to it, without changing any host parameter IDs or plugin UIDs.
set(au_plist "${stage}/UndertowPreview.component/Contents/Info.plist")
set(vst_plist "${stage}/UndertowPreview.vst3/Contents/Info.plist")
execute_process(COMMAND /usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "${au_plist}"
    OUTPUT_VARIABLE version OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Missing AU processor version in generated bundle metadata")
endif()
run_checked(/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier org.stormsurgefx.undertow-preview.au" "${au_plist}")
run_checked(/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier org.stormsurgefx.undertow-preview.vst3" "${vst_plist}")
run_checked(/usr/libexec/PlistBuddy -c "Set :CFBundleVersion ${version}" "${vst_plist}")
run_checked(/usr/libexec/PlistBuddy -c "Add :CFBundleShortVersionString string ${version}" "${vst_plist}")
# The exporter advertises broad network/file resource usage not needed here.
run_checked(/usr/libexec/PlistBuddy -c "Delete :AudioComponents:0:resourceUsage" "${au_plist}")
foreach(format vst3 component)
    run_checked(/usr/bin/codesign --force --sign - --timestamp=none "${stage}/UndertowPreview.${format}")
    run_checked(/usr/bin/codesign --verify --strict "${stage}/UndertowPreview.${format}")
endforeach()
set(archive "${BINARY_DIR}/UndertowPreview-macos-universal.zip")
run_checked(/usr/bin/ditto -c -k --sequesterRsrc --keepParent "${stage}" "${archive}")
file(SHA256 "${archive}" digest)
file(WRITE "${archive}.sha256" "${digest}  UndertowPreview-macos-universal.zip\n")
