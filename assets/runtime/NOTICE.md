# Undertow artwork

## Prepared enclosure

`undertow-enclosure.png` is a prepared derivative of the maintainer's local
`assets/ui/textures/undertow-enclosure-v01.jpeg`. The original remains unchanged.

The maintainer confirmed that they generated the artwork with Gemini Nano Banana
in a personal Gemini account, using an assistant-supplied text prompt and no uploaded
third-party reference images. Confirmation was recorded during issue #15 work on
2026-10-17. Exact model revision, generation date and original prompt text have not
been recovered. This is AI-generated artwork, not a photograph of a real product.

The maintainer approved reuse in StormSurge FX. The prepared artwork is supplied
under the project's GPL-3.0-only terms to the extent applicable rights exist.
This does not assert exclusive ownership, guaranteed copyright protection or
guaranteed clearance of all third-party rights.

[Google's general terms](https://policies.google.com/terms) state that Google does
not claim ownership of original generated content. [Gemini's image guidance](https://support.google.com/gemini/answer/14286560?hl=en)
requires respecting others' copyright and privacy rights. Reviewed 2026-10-17;
this record is not legal advice or a blanket clearance of other local assets.

Preparation rotates the enclosure clockwise, removes the connected white outer
background and downsizes it. The editor uses nine-slice drawing to retain the rim
and corner proportions. Labels, knobs and switches remain separate live controls.
The PNG is embedded at configure time using CMake and DPF's existing image decoder.
No image file is read from disk by the running plugin.

Preparation command, requiring ImageMagick only when regenerating the PNG:

```sh
magick assets/ui/textures/undertow-enclosure-v01.jpeg -rotate 90 \
  -alpha on -fuzz 8% -fill none -draw 'color 0,0 floodfill' \
  -resize 1040x -dither None -colors 128 -strip assets/runtime/undertow-enclosure.png
```

## Original knob material

`undertow-knob.png` is original GPL-3.0-only artwork rendered by
`tools/render_knob.py`. It is not cropped from the local Gemini knob image. That
image was used as a visual reference for the silver machined-metal style only.

The renderer draws a short ridged cylinder, bevel, concentric tool marks, fixed
metal reflections and a transparent cast shadow. The value pointer is drawn
separately in the editor, so changing a value does not rotate the lighting. One
252×252 RGBA texture is shared by all eight controls. It uses the same embedded
image path as the enclosure, with no runtime file loading or extra dependency.

Regenerate with Python's standard library, not needed for normal builds:

```sh
python tools/render_knob.py
```
