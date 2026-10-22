<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Riptide: pitch-down guitar pedal

Riptide is the pitch-down pedal for whole-guitar drop tuning and for blending a
lower layer under the dry guitar. Its first VST3 slice exposes only the controls
needed for those two uses.

## Controls

| Control | Range | Default | Meaning |
| --- | --- | --- | --- |
| Shift | 0 to -12 semitones, whole steps | 0 | Downward transposition. `0` is the unshifted guitar. |
| Blend | 0 to 100 % | 100 % | How much shifted signal replaces the dry signal. `0` is dry. |
| Bypass | On/off | Off | Fades the shifted layer out over the core's 20 ms ramp. |

The defaults are deliberately an exact identity: inserting Riptide at its defaults
does not change the sound. Move Shift down for a drop tune, or set Shift to -12 and
lower Blend for an added lower layer.

There is no pitch-up mode, no harmony interval and no live note tracking. The
tuner-style display in the earlier concept mockup was illustrative; the editor shows
the Shift value, not what note is playing.

## What it is not

The core uses two overlapping granular read heads. It is not a polyphonic
high-quality pitch shifter. Expect some modulation, transient smearing and
frequency-dependent dips, especially on chords and low notes. Numerical tests do not
establish that those are acceptable; the sound and playing feel decide that.

At 48 kHz the read heads look back from about 1 ms to about 41 ms. That moving span
is the effect's own grain window, **not** a fixed plugin latency, and Riptide reports
no latency offset to the host. The pinned DPF VST3 adapter already hardcodes zero
tail samples; Riptide does not promise a reported tail. The read heads keep running
while bypassed, so leaving bypass fades back in rather than restarting from silence.

## Building

Riptide builds its own VST3 target on Linux and Windows, and experimental VST3/AUv2
on macOS. It has a separate identity (`UtR0`) and state schema (`riptide_schema`);
it does not share parameters or saved state with any other StormSurge pedal.

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target Riptide-vst3
build/linux-debug/riptide_gui --view          # inspect the editor without audio
```

The editor's default size is 500x680, with a 390x530 minimum. The enclosure uses
the same prepared artwork as the concept mockup; see
`assets/runtime/RIPTIDE-NOTICE.md`.

There is no Riptide standalone application. Like Maelstrom, it is a plugin only:
its bypass is not a monitoring consent gate, so a standalone build would need its
own explicit input-enable design first.

## Checking it

```sh
ctest --preset linux-debug -R '^riptide_core$'
ctest --preset linux-debug -R '^riptide_plugin$'
ctest --preset linux-debug -R '^riptide_vst3_(loader|process|editor)$'
```

The core test is numerical. The plugin test drives the DPF processor directly. The
VST3 checks load an actual built module, process one stereo block and create a real
editor, but open no audio device, install nothing and are not DAW playback evidence.
`riptide_gui` needs a display; it exercises the actual knobs and footswitch through
host edit gestures.

## Listening status

On 2026-10-22 the maintainer listened to private offline renders of the prototype
and judged it good enough to build on. That approves proceeding with this plugin
slice, not the final sound. Riptide still needs listening through an amp modeller
and with the practice board, covering drop tuning, chugs, power chords, doubled
guitars and the low-layer blend. Keep private recordings local.

The offline renderer remains available for comparing settings before loading the
plugin; see [the prototype notes](riptide-dsp-prototype.md).
