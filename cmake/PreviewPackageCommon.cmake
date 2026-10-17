# SPDX-License-Identifier: GPL-3.0-only
function(preview_package_metadata stage platform)
    if(NOT IS_ABSOLUTE "${SOURCE_DIR}" OR NOT EXISTS "${SOURCE_DIR}/release/preview.json")
        message(FATAL_ERROR "Supply the absolute SOURCE_DIR of this checkout")
    endif()
    file(READ "${SOURCE_DIR}/release/preview.json" release)
    string(JSON version GET "${release}" version)
    string(JSON plugin_version GET "${release}" plugin_version)
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
        "  \"source_url\": \"https://github.com/marrabld/stormsurge-fx/tree/${commit}\",\n"
        "  \"source_archive\": \"stormsurge-fx-${version}-source.tar.gz\"\n}\n")
    file(COPY "${BINARY_DIR}/bin/notices" DESTINATION "${stage}")
    file(COPY "${SOURCE_DIR}/LICENSE" "${SOURCE_DIR}/THIRD_PARTY_NOTICES.md" DESTINATION "${stage}")
    configure_file("${SOURCE_DIR}/docs/install-preview.md" "${stage}/READ-ME-FIRST.md" COPYONLY)
    file(READ "${SOURCE_DIR}/docs/preview.md" controls)
    string(REPLACE "(building.md)" "(https://github.com/marrabld/stormsurge-fx/blob/${commit}/docs/building.md)" controls "${controls}")
    string(REPLACE "(../assets/SOURCES.md)" "(https://github.com/marrabld/stormsurge-fx/blob/${commit}/assets/SOURCES.md)" controls "${controls}")
    file(WRITE "${stage}/CONTROLS-AND-LIMITATIONS.md" "${controls}")
    configure_file("${SOURCE_DIR}/release/NOTES.md" "${stage}/RELEASE-NOTES.md" COPYONLY)
endfunction()
