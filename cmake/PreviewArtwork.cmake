# SPDX-License-Identifier: GPL-3.0-only
# Embed only the prepared enclosure. User-local originals are not build inputs.
set(preview_artwork "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/undertow-enclosure.png")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${preview_artwork}")
file(READ "${preview_artwork}" artwork_bytes HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," artwork_bytes "${artwork_bytes}")
# Short physical lines also work with MSVC's translation limits.
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n"
    artwork_bytes "${artwork_bytes}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PreviewArtwork.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PreviewArtwork.hpp" @ONLY)
