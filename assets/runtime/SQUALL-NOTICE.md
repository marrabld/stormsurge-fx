# Squall mockup artwork

On 2026-10-19 the maintainer supplied the enclosure image from the system clipboard
and approved its prepared derivative for public inclusion in the display-only
Squall mockup, under GPL-3.0-only to the extent applicable rights exist. This does
not assert exclusive ownership or guaranteed third-party clearance. The image is a
maintainer-supplied generated concept; its generator, exact model revision and
generation date have not been independently confirmed.

The original is unchanged and remains local/gitignored:

| Local original | SHA256 | Prepared derivative | SHA256 |
| --- | --- | --- | --- |
| `ui/textures/squall-enclosure-v01.png` | `4f79df3ede51ed60355dbca96c40138675effff3e4cd2faf8e13677c13194877` | `runtime/squall-enclosure.png` | `47a4e0f4b88058117582ccc2f2015cfc3f11b3f940331d1ac997b683ad5bc0e8` |

The original is 765x1024. Preparation removes only the connected near-white
outside background, trims the transparent margin, resizes to 1040 pixels wide,
reduces the palette to 256 colours and strips metadata. The pedal rim and paint
remain intact.

Regenerate with ImageMagick:

```sh
magick assets/ui/textures/squall-enclosure-v01.png -alpha on -fuzz 8% \
  -fill none -draw 'color 0,0 floodfill' -trim +repage -resize 1040x \
  -dither None -colors 256 -strip assets/runtime/squall-enclosure.png
```

The prepared image is embedded in the display-only Squall concept mockup and the
Squall VST3 listening-preview editor. It is not used by the other plugins or
authorized for a release package. Its inclusion does not imply approval of the
provisional sound settings or any additional Squall work.
