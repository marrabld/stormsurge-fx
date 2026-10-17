# SPDX-License-Identifier: GPL-3.0-only
# Maelstrom embeds the approved enclosure plus Undertow's original knob and
# footswitch renders, so it reuses preview_embed_image() from PreviewArtwork.cmake.
# Include this after that module and outside BUILD_TESTING: the plugin's UI target
# needs the generated header even when tests are disabled.
preview_embed_image(maelstrom-enclosure.png maelstrom_bytes)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MaelstromArtwork.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/MaelstromArtwork.hpp" @ONLY)
