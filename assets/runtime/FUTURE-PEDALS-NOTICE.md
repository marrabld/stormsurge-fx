# Whitecap and Riptide concept artwork

On 2026-10-18 the maintainer confirmed that the Whitecap and Riptide enclosure
images were generated with Gemini Nano Banana from the assistant-provided prompts,
without third-party image references, and approved public use under the project's
GPL-3.0-only terms to the extent applicable rights exist. This does not assert
exclusive ownership or guaranteed third-party clearance. Exact model revision and
generation time are not known.

The originals saved from the maintainer's clipboard are unchanged and remain
gitignored:

| Local original | SHA256 | Prepared derivative |
| --- | --- | --- |
| `ui/textures/whitecap-enclosure-v02.png` | `e3b5085eaefac29c5a73cdd349d8cbcf25f30bfc70743631edd77585b2b59ed6` | `runtime/whitecap-enclosure.png` |
| `ui/textures/riptide-enclosure-v02.png` | `29dfae1b56b1e5901abf75e3b5fd3f8d437c80d34748363378f7da6e96bc6e0d` | `runtime/riptide-enclosure.png` |

Each original is 765x1024. Preparation removes only the connected near-white
outside background, trims the transparent margin and downsizes to 1040 pixels
wide with a 256-colour palette. The pedal rim and paint remain intact.

Regenerate the derivatives with ImageMagick:

```sh
for pedal in whitecap riptide; do
  magick "assets/ui/textures/${pedal}-enclosure-v02.png" -alpha on -fuzz 8% \
    -fill none -draw 'color 0,0 floodfill' -trim +repage -resize 1040x \
    -dither None -colors 256 -strip "assets/runtime/${pedal}-enclosure.png"
done
```

These images are embedded only in the display-only concept mockup. They are not
used by Undertow or Maelstrom and are not part of their plugin packages. Whitecap's
chorus/vibrato behavior and Riptide's pitch-shift behavior remain undecided in
issues #38 and #39.
