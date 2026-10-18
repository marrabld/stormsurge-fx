# SPDX-License-Identifier: GPL-3.0-only
# Whitecap's concept mockup and VST3 preview use separately prepared artwork.
preview_embed_image(whitecap-enclosure.png whitecap_bytes)
preview_embed_image(riptide-enclosure.png riptide_bytes)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ConceptMockupArtwork.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ConceptMockupArtwork.hpp" @ONLY)
