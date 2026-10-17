# Artwork

- `concepts/`: full pedal mockups and visual references, not runtime backgrounds.
- `ui/textures/`: paint, grain and enclosure surfaces for later UI preparation.
- Future `ui/artwork/`: separate wave graphics and logos.
- Future `ui/controls/`: knob/switch artwork if the interface needs image assets.
- `runtime/`: prepared artwork approved for bundling, with its provenance notice.

Keep original exports unchanged. Produce cropped, resized or optimised runtime
versions separately once the interface dimensions and scaling behaviour are agreed.
Only bundle assets the application actually needs.

The current enclosure image includes its outer rim and a white surrounding margin.
It is a full enclosure surface, not a seamless repeating texture. The original
has not been edited. `runtime/undertow-enclosure.png` is a separate rotated,
masked and reduced-colour derivative. The editor preserves rim/corner proportions
with nine-slice drawing rather than stretching the whole image.
The runtime knob material is a separate original render from `tools/render_knob.py`,
not a crop of the local knob concept. Its value marker remains a live control.

Keep labels and interactive controls separate from backgrounds. See [SOURCES.md](SOURCES.md)
for provenance and usage status before publishing or bundling artwork.
