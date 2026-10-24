# Riptide artwork

On 2026-10-18 the maintainer confirmed that the Riptide enclosure image was
generated with Gemini Nano Banana from an assistant-provided prompt, without
third-party image references, and approved public use under the project's
GPL-3.0-only terms to the extent applicable rights exist. This does not assert
exclusive ownership or guaranteed third-party clearance. Exact model revision and
generation time are not known.

The original saved from the maintainer's clipboard is unchanged and remains
gitignored:

| Local original | SHA256 | Prepared derivative |
| --- | --- | --- |
| `ui/textures/riptide-enclosure-v02.png` | `29dfae1b56b1e5901abf75e3b5fd3f8d437c80d34748363378f7da6e96bc6e0d` | `runtime/riptide-enclosure.png` |

The original is 765x1024. Preparation removes only the connected near-white
outside background, trims the transparent margin and downsizes to 1040 pixels
wide with a 256-colour palette. The pedal rim and paint remain intact.

Regenerate the derivative with ImageMagick:

```sh
magick assets/ui/textures/riptide-enclosure-v02.png -alpha on -fuzz 8% \
  -fill none -draw 'color 0,0 floodfill' -trim +repage -resize 1040x \
  -dither None -colors 256 -strip assets/runtime/riptide-enclosure.png
```

The prepared image is embedded in the display-only concept mockup and the Riptide
VST3 editor. It is not used by the other plugins and is not part of their plugin
packages. Riptide's pitch-shift sound and playing feel still need listening
approval beyond the prototype result recorded in issue #39; inclusion here does
not make it release-ready.
