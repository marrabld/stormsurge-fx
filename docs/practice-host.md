<!-- SPDX-License-Identifier: GPL-3.0-only -->
# StormSurge practice host

Issue #61 tracks a Linux-first practice application for chaining StormSurge VST3
effects, playing audio files, monitoring a live instrument only after an explicit
action, and routing the chain into a separate amp simulator such as TONE3000.

The `practice_board_gui` target remains a **display-only mockup**. It uses the
replaceable draft texture from the maintainer's local artwork, embedded as a prepared
1024x1024 image. See `assets/runtime/PEDAL-BOARD-NOTICE.md`.

Build and inspect it on Linux:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target practice_board_gui
build/linux-debug/practice_board_gui --view
```

The display harness checks its reorder, bypass and minimum-size interactions when
run without `--view`; `--small` checks the minimum window size. This does not verify
audio routing or establish that the eventual host is ready for practice.

The Linux VST3 editor host's `--board` mode opens all four real pedal editors and a
JACK client in one horizontally scrollable X11 window. It processes audio through
the pedals in the displayed order. Drag the middle strips to reorder; click a pedal's
footswitch strip to toggle its real VST3 bypass parameter. For example:

```sh
build/linux-debug/vst3_editor_tests --board \
  build/linux-debug/bin/UndertowPreview.vst3/Contents/x86_64-linux/UndertowPreview.so \
  build/linux-debug/bin/Maelstrom.vst3/Contents/x86_64-linux/Maelstrom.so \
  build/linux-debug/bin/Whitecap.vst3/Contents/x86_64-linux/Whitecap.so \
  build/linux-debug/bin/Squall.vst3/Contents/x86_64-linux/Squall.so
```

Start a JACK server (PipeWire's JACK compatibility is supported) before launching.
The host registers `input_left`, `input_right`, `output_left`, and `output_right`;
connect them with a patchbay such as qpwgraph. Connect an instrument/system source to
the input ports, and connect the output ports to TONE3000's audio inputs. It never
starts a device, makes connections, or enables live input automatically. The top-left
button is an explicit live-input enable; startup, JACK restart and rate change all
leave it muted. `<`/`>` or the mouse wheel scrolls the board. `--board-smoke` still
only checks editor attachment and opens no JACK client; `--view` still opens one
editor by itself.

This is not yet a complete practice host: there is no audio-file playback, no board
state saving, and it has only been smoke-tested on the current PipeWire/JACK setup.
It does not establish reliable low-latency performance or compatibility with every
JACK server, pedal version, or TONE3000 setup. Keep the live-input routing muted until
you deliberately connect and enable it.

Device-free tests also instantiate and process one stereo block through each current
Linux StormSurge VST3 bundle. They do not verify the live JACK callback or audio-device
behaviour.
