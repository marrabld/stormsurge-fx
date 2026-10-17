# Maelstrom mockup artwork

`maelstrom-enclosure.png` is a prepared derivative of the maintainer's local
`assets/ui/textures/maelstrom-enclosure-v01.jpeg`. The original is unchanged,
gitignored, and has SHA256
`aedd00a4825b0c58446520debb5b73b405af4bc0e8c06956ac81f88d36b63e7a`.

On 2026-10-17 the maintainer confirmed personal Gemini/Nano Banana generation,
using the assistant's Maelstrom enclosure prompt and only StormSurge artwork as
any image reference, with no external artwork references. Exact model revision
and generation date are not established. This is AI-generated art, not a photo
of a manufactured pedal.

The maintainer explicitly approved preparing and bundling this derivative under
the project's GPL-3.0-only terms to the extent applicable rights exist. This does
not assert exclusive ownership, copyright protection or guaranteed third-party
clearance. See the Google/Gemini terms discussion in `assets/SOURCES.md` and
the separate Undertow hardware notice in `assets/runtime/NOTICE.md`.

Preparation rotates the unlabeled enclosure clockwise into portrait orientation,
removes the connected white background, trims the outer transparent margin and
downsizes with preserved aspect ratio. Nine-slice drawing retains the metal rim
and corners in the shorter mockup. The original knob and footswitch PNGs are reused
unchanged; labels, pointers and the LED are drawn separately.

Regeneration requires ImageMagick, not needed for ordinary builds:

```sh
magick assets/ui/textures/maelstrom-enclosure-v01.jpeg -rotate 90 \
  -alpha on -fuzz 8% -fill none -draw 'color 0,0 floodfill' -trim +repage \
  -resize 1040x -dither None -colors 256 -strip assets/runtime/maelstrom-enclosure.png
```

The mockup embeds the prepared PNG at configure time and reads no artwork from
disk at runtime. Its notice is copied to the build's `mockup-notices/` directory,
not added to Undertow's plugin release payload.
