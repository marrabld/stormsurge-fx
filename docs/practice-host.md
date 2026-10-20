<!-- SPDX-License-Identifier: GPL-3.0-only -->
# StormSurge practice host

Issue #61 tracks a Linux-first practice application for chaining StormSurge VST3
effects, playing audio files, monitoring a live instrument only after an explicit
action, and routing the chain into a separate amp simulator such as TONE3000.

The current `practice_board_gui` target is only a **display and interaction prototype**.
It shows the four existing VST3 pedals in a horizontal signal chain. Drag a pedal to
reorder it; click its footswitch to toggle the local on/bypass indicator. No VST3 is
loaded, no controls are connected to DSP, and it opens no audio device. The labels
for file playback, live input and output routing are illustrative, not functional.

Build and inspect it on Linux:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target practice_board_gui
build/linux-debug/practice_board_gui --view
```

The display harness checks its reorder, bypass and minimum-size interactions when
run without `--view`; `--small` checks the minimum window size. This does not verify
audio routing or establish that the eventual host is ready for practice.

The Linux VST3 editor probe can also attach one real pedal editor to a host-owned
X11 window, without starting an audio device. For example:

```sh
build/linux-debug/vst3_editor_tests \
  build/linux-debug/bin/UndertowPreview.vst3/Contents/x86_64-linux/UndertowPreview.so \
  --view
```

This is an editor-embedding probe for one pedal, not yet the multi-pedal practice
board. It requires a graphical X11 session and exits when the host window closes.

The host investigation has also added device-free tests that instantiate and process
one stereo block through each current Linux StormSurge VST3 bundle. Those tests verify
that the VST3 modules can run through DPF's existing interface; they do not yet connect
plugins into a user-selectable chain or open an audio device.
