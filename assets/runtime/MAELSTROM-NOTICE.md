# Maelstrom artwork

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
unchanged; labels and pointers remain separate live drawing elements.

Regeneration requires ImageMagick, not needed for ordinary builds:

```sh
magick assets/ui/textures/maelstrom-enclosure-v01.jpeg -rotate 90 \
  -alpha on -fuzz 8% -fill none -draw 'color 0,0 floodfill' -trim +repage \
  -resize 1040x -dither None -colors 256 -strip assets/runtime/maelstrom-enclosure.png
```

## LED and mode toggle, issue #34

On 2026-10-18 the maintainer confirmed these controls were generated with Gemini
Nano Banana using a personal Gmail account, from the assistant-provided LED and
toggle prompt, without third-party reference images. The exact model revision and
generation date are not known. The maintainer approved use in the public project
and plugin builds under GPL-3.0-only to the extent applicable rights exist. This
does not assert exclusive ownership or guaranteed third-party clearance.

Originals, all unchanged and gitignored:

| Original | SHA256 | Use |
| --- | --- | --- |
| `ui/textures/LED_and_Toggle-v01.jpeg` | `f3507e5f61403e442bdbaab9023d317978d3a9e708434260433935f067dbdfd8` | LED-on crop and toggle concept sheet |
| `ui/textures/LED_off-v01.png` | `cac522e6586f3c9cad9fb9cf4af2ef017dc0038695317b9139740ee77f61` | LED-off source |
| `ui/textures/Toggle_on-v01.png` | `2b8407945f6ae3fcabad533342e587537b97ea77b727d277822beb3858151a09` | Toggle-left source |

The prepared PNGs use a 20% magenta key, a two-pixel alpha erosion, transparent
pixel decontamination and Lanczos downsampling to reduce the colored fringe and
minimize aliasing at the displayed size. `maelstrom-led-on.png` is the 581x582
crop at `+1509+165` from the sheet. The toggle-left asset comes from the isolated
PNG; toggle-right is the second physical position cropped at `+1515+1025` from
the sheet. Their source socket centers differ, so at the 72x59 display size the
UI shifts the right image by eight pixels to keep the pivot stationary while the
lever moves. The files are named
left/right because their physical orientation, not an electrical on/off
function, is what the pedal UI displays. LED states are separate so effect bypass
can change the light without repainting the artwork.

Regenerate from the local originals with ImageMagick:

```sh
magick assets/ui/textures/LED_off-v01.png -alpha set -fuzz 20% \
  -transparent '#ff00ff' -channel A -morphology Erode Disk:2 +channel \
  -channel RGB -fx 'a==0?0:u' +channel -filter Lanczos -resize 144x144 -strip \
  assets/runtime/maelstrom-led-off.png
magick assets/ui/textures/LED_and_Toggle-v01.jpeg -crop 581x582+1509+165 +repage \
  -alpha set -fuzz 20% -transparent '#ff00ff' -channel A \
  -morphology Erode Disk:2 +channel -channel RGB -fx 'a==0?0:u' +channel \
  -filter Lanczos -resize 144x144 -strip assets/runtime/maelstrom-led-on.png
magick assets/ui/textures/Toggle_on-v01.png -alpha set -fuzz 20% \
  -transparent '#ff00ff' -channel A -morphology Erode Disk:2 +channel \
  -channel RGB -fx 'a==0?0:u' +channel -filter Lanczos -resize 216x176 -strip \
  assets/runtime/maelstrom-toggle-left.png
magick assets/ui/textures/LED_and_Toggle-v01.jpeg -crop 714x583+1515+1025 +repage \
  -alpha set -fuzz 20% -transparent '#ff00ff' -channel A \
  -morphology Erode Disk:2 +channel -channel RGB -fx 'a==0?0:u' +channel \
  -filter Lanczos -resize 216x176 -strip assets/runtime/maelstrom-toggle-right.png
```

CMake embeds the prepared PNGs; no source artwork is read at runtime. This notice
is copied alongside development binaries and the display-only mockup. Maelstrom
artwork is not included in Undertow's binary packages.
