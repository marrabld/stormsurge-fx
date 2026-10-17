# SPDX-License-Identifier: GPL-3.0-only
function(preview_package_metadata stage platform)
    if(NOT IS_ABSOLUTE "${SOURCE_DIR}" OR NOT EXISTS "${SOURCE_DIR}/release/preview.json")
        message(FATAL_ERROR "Supply the absolute SOURCE_DIR of this checkout")
    endif()
    file(READ "${SOURCE_DIR}/release/preview.json" release)
    string(JSON version GET "${release}" version)
    string(JSON plugin_version GET "${release}" plugin_version)
    set(baseline "${CMAKE_HOST_SYSTEM_NAME}")
    if(platform STREQUAL "linux-x86_64")
        file(STRINGS /etc/os-release ubuntu REGEX "^ID=ubuntu$")
        file(STRINGS /etc/os-release ubuntu_version REGEX "^VERSION_ID=\"24\\.04\"$")
        if(NOT ubuntu STREQUAL "" AND NOT ubuntu_version STREQUAL "")
            set(baseline "ubuntu-24.04")
        else()
            set(baseline "non-Ubuntu-24.04-development-build")
        endif()
    endif()
    execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot identify the package source commit")
    endif()
    execute_process(COMMAND git status --porcelain --untracked-files=no --ignore-submodules=none
        WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE dirty RESULT_VARIABLE result)
    if(NOT result EQUAL 0 OR NOT dirty STREQUAL "")
        message(FATAL_ERROR "Package only a committed checkout with clean recursive submodules")
    endif()
    file(WRITE "${stage}/BUILD.json"
        "{\n  \"schema\": 1,\n  \"release_version\": \"${version}\",\n"
        "  \"plugin_version\": \"${plugin_version}\",\n  \"platform\": \"${platform}\",\n"
        "  \"configuration\": \"Release\",\n  \"source_commit\": \"${commit}\",\n"
        "  \"build_baseline\": \"${baseline}\",\n"
        "  \"source_url\": \"https://github.com/marrabld/stormsurge-fx/tree/${commit}\",\n"
        "  \"source_archive\": \"stormsurge-fx-${version}-source.tar.gz\"\n}\n")
    file(COPY "${BINARY_DIR}/bin/notices" DESTINATION "${stage}")
    file(COPY "${SOURCE_DIR}/LICENSE" "${SOURCE_DIR}/THIRD_PARTY_NOTICES.md" DESTINATION "${stage}")
    configure_file("${SOURCE_DIR}/docs/install-preview.md" "${stage}/READ-ME-FIRST.md" COPYONLY)
    file(READ "${SOURCE_DIR}/docs/preview.md" controls)
    string(REPLACE "(building.md)" "(https://github.com/marrabld/stormsurge-fx/blob/${commit}/docs/building.md)" controls "${controls}")
    string(REPLACE "(../assets/SOURCES.md)" "(https://github.com/marrabld/stormsurge-fx/blob/${commit}/assets/SOURCES.md)" controls "${controls}")
    file(WRITE "${stage}/CONTROLS-AND-LIMITATIONS.md" "${controls}")
    # Embed tester-facing notes, not the review checklist or a publication-state
    # claim that would become false if these exact reviewed ZIPs are approved.
    file(READ "${SOURCE_DIR}/release/NOTES.md" notes)
    string(FIND "${notes}" "## What's included" start)
    string(FIND "${notes}" "## Before publication" end)
    if(start LESS 0 OR end LESS start)
        message(FATAL_ERROR "Release notes are missing the tester-facing section boundaries")
    endif()
    math(EXPR length "${end} - ${start}")
    string(SUBSTRING "${notes}" ${start} ${length} notes)
    file(WRITE "${stage}/RELEASE-NOTES.md" "# Undertow Preview ${version}\n\nExperimental plugin preview. See BUILD.json for source identity.\n\n${notes}")
endfunction()
