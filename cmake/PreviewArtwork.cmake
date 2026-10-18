# SPDX-License-Identifier: GPL-3.0-only
# User-local originals and artwork-generation tools are not build inputs.
function(preview_embed_image filename variable)
    set(path "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/${filename}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    file(READ "${path}" bytes HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
    # Short physical lines also work with MSVC's translation limits.
    string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n"
        bytes "${bytes}")
    set(${variable} "${bytes}" PARENT_SCOPE)
endfunction()
preview_embed_image(undertow-enclosure.png artwork_bytes)
preview_embed_image(undertow-knob.png knob_bytes)
preview_embed_image(undertow-footswitch.png footswitch_bytes)
preview_embed_image(maelstrom-led-off.png led_off_bytes)
preview_embed_image(maelstrom-led-on.png led_on_bytes)
preview_embed_image(maelstrom-toggle-left.png toggle_left_bytes)
preview_embed_image(maelstrom-toggle-right.png toggle_right_bytes)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PreviewArtwork.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PreviewArtwork.hpp" @ONLY)
