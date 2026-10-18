# SPDX-License-Identifier: GPL-3.0-only
# Maelstrom embeds the approved enclosure plus Undertow's original knob and
# footswitch renders, so it reuses preview_embed_image() from PreviewArtwork.cmake.
# Include this after that module and outside BUILD_TESTING: the plugin's UI target
# needs the generated header even when tests are disabled.
preview_embed_image(maelstrom-enclosure.png maelstrom_bytes)
preview_embed_image(maelstrom-led-off.png maelstrom_led_off_bytes)
preview_embed_image(maelstrom-led-on.png maelstrom_led_on_bytes)
preview_embed_image(maelstrom-toggle-left.png maelstrom_toggle_left_bytes)
preview_embed_image(maelstrom-toggle-right.png maelstrom_toggle_right_bytes)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MaelstromArtwork.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/MaelstromArtwork.hpp" @ONLY)
