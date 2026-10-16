# Artwork

- `concepts/`: full pedal mockups and visual references, not runtime backgrounds.
- `ui/textures/`: paint, grain and enclosure surfaces for later UI preparation.
- Future `ui/artwork/`: separate wave graphics and logos.
- Future `ui/controls/`: knob/switch artwork if the interface needs image assets.

Keep original exports unchanged. Produce cropped, resized or optimised runtime
versions separately once the interface dimensions and scaling behaviour are agreed.
Only bundle assets the application actually needs.

The current enclosure image includes its outer rim and a white surrounding margin.
It is a full enclosure surface, not a seamless repeating texture. It may need a
separate crop/mask or a different export later; the original has not been edited.

Keep labels and interactive controls separate from backgrounds. See [SOURCES.md](SOURCES.md)
for provenance and usage status before publishing or bundling artwork.
