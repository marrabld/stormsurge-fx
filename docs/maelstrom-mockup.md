# Maelstrom visual mockup

Display-only prototype for [issue #26](https://github.com/marrabld/stormsurge-fx/issues/26),
brought forward at the maintainer's request after design PR #28 merged.
There is no overdrive engine, plugin, audio device or monitoring path in this
executable. It does not alter Undertow or the published preview.

![Maelstrom visual mockup, no audio](images/maelstrom-mockup.png)

The shorter 520×680 face uses the supplied charcoal/red enclosure, StormSurge's
existing silver knobs and metal footswitch, and a recessed amber LED. Starting
readouts match the approved Drive 10%, Tight/Bite 50% and Level 0 dB.
The source texture is rotated to portrait and trimmed, rather than stretching the
whole original landscape image. Nine-slice rendering preserves the rim/corners.
The original JPEG and Undertow's runtime assets remain untouched.

## Build and view

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target maelstrom_mockup
build/linux-debug/maelstrom_mockup --view
```

Drag a knob vertically, scroll over it, or hold Shift for finer movement. Click
the footswitch to change the visual LED/state. These edits are local to the mockup,
not host parameters, DSP settings, automation or persisted state. The Level knob's
travel is a display placeholder from silence through -36 to +12 dB, not an approved
final taper. Closing/reopening restores the approved initial readouts.

Without `--view`, the harness checks image loading, knob drag/scroll endpoints,
footswitch state and geometry, renders 30 frames and exits. `--small` repeats at
390×510. It needs a display/OpenGL context and is deliberately not a CTest test:

```sh
xvfb-run -a build/linux-debug/maelstrom_mockup
xvfb-run -a build/linux-debug/maelstrom_mockup --small
```

The target also compiles with the existing Windows/Mac preset builds. Linux/Mac
CI runs the display checks; claims about a platform require an actual passing run.
No audio test or host-compatibility claim follows from this mockup.

Screenshot capture, available only in this mockup executable:

```sh
STORMSURGE_CAPTURE=/absolute/path/maelstrom.ppm build/linux-debug/maelstrom_mockup
magick /absolute/path/maelstrom.ppm -strip docs/images/maelstrom-mockup.png
```

The prepared PNG is embedded by CMake; ordinary builds need neither ImageMagick nor
the ignored local original. See [artwork provenance](../assets/SOURCES.md) and
[the Maelstrom notice](../assets/runtime/MAELSTROM-NOTICE.md). The build keeps its
notice in `mockup-notices/` and existing renderer/font notices in `bin/notices/`.
Neither this executable nor its new texture is added to Undertow binary packages.

The maintainer still needs to review the layout. Real host controls/state wiring
remains #25/#26 work after the reusable DSP in #24. No new plugin identity or
parameter schema is established by this visual prototype.
