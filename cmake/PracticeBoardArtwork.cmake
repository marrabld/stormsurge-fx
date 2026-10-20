# SPDX-License-Identifier: GPL-3.0-only
set(board_texture "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/pedal-board-texture-v01.jpeg")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${board_texture}")
file(READ "${board_texture}" board_texture_bytes HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," board_texture_bytes "${board_texture_bytes}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n"
    board_texture_bytes "${board_texture_bytes}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PracticeBoardArtwork.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PracticeBoardArtwork.hpp" @ONLY)
