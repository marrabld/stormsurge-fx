# SPDX-License-Identifier: GPL-3.0-only
# Windows' SDK OpenGL headers alone do not cover NanoVG's extension types.
# Fetch only the two official MIT-licensed headers, pinned by commit and hash.
set(STORMSURGE_KHRONOS_DIR "${CMAKE_BINARY_DIR}/khronos")
file(MAKE_DIRECTORY "${STORMSURGE_KHRONOS_DIR}/GL" "${STORMSURGE_KHRONOS_DIR}/KHR")
file(DOWNLOAD
    "https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/6af574a14089ccfee87efe230ebcdd8742859813/api/GL/glext.h"
    "${STORMSURGE_KHRONOS_DIR}/GL/glext.h"
    EXPECTED_HASH SHA256=d7b6f15b1bb209e08bd6ada36983f8bb40429e66c24083aa2474d55205d97047
    TLS_VERIFY ON TIMEOUT 60)
file(DOWNLOAD
    "https://raw.githubusercontent.com/KhronosGroup/EGL-Registry/db3425b8246136faccb5e2782b5694960bd6edf1/api/KHR/khrplatform.h"
    "${STORMSURGE_KHRONOS_DIR}/KHR/khrplatform.h"
    EXPECTED_HASH SHA256=7b1e01aaa7ad8f6fc34b5c7bdf79ebf5189bb09e2c4d2e79fc5d350623d11e83
    TLS_VERIFY ON TIMEOUT 60)
