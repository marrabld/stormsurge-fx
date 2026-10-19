# Artwork sources and usage status

These files were supplied locally by the maintainer during AI-assisted artwork
planning. On 2026-10-17 the maintainer confirmed **Nano Banana through Google
Gemini**, a personal Gemini account, an assistant-supplied text prompt and no
uploaded third-party reference images. Exact model revision, generation date and
original prompt text have not been recovered. Do not infer exclusive copyright
ownership from AI generation.

## Approved prepared enclosure, issue #15

The maintainer requested reuse of the existing artwork in the GUI and confirmed
the provenance above after a terms discussion. The prepared derivative
`runtime/undertow-enclosure.png` is approved for bundling with StormSurge FX, under
the project's GPL-3.0-only terms to the extent applicable rights exist. Its
[notice](runtime/NOTICE.md) records the preparation and terms review. This is not
a guarantee of copyright protection or third-party clearance. No other local
original image was bundled by that task. Originals remain unchanged and gitignored.
`runtime/undertow-knob.png` and `runtime/undertow-footswitch.png` are separate
original mathematical renders, with source in `tools/render_knob.py`; the Gemini
hardware concepts are visual references only and their pixels are not embedded.

The per-original entries below retain the earlier review state for context;
the enclosure derivative's current inclusion status is the approval above.

## Approved Maelstrom mockup enclosure, issue #26

On 2026-10-17 the maintainer confirmed that
`ui/textures/maelstrom-enclosure-v01.jpeg` was generated with personal
Gemini/Nano Banana, our Maelstrom prompt and only StormSurge artwork as any image
reference, without external artwork references. They explicitly approved
preparing and bundling the derivative under the project's GPL terms to the extent
applicable rights exist. No exact model revision or generation date is asserted.

`runtime/maelstrom-enclosure.png` rotates the unlabeled enclosure into portrait
orientation, removes the connected white outer background, trims transparent
margins and downsizes with the original proportions. The display-only mockup
uses nine-slice rendering for the rim/corners, and reuses the existing original
knob and footswitch materials unchanged. Labels/markers remain separate.
The original JPEG remains unchanged and ignored.

See [Maelstrom artwork notice](runtime/MAELSTROM-NOTICE.md) for the original's
SHA256, generation/reuse record and regeneration command. CMake copies this notice
to the development binary notices and mockup notices. The Maelstrom artwork is
not part of Undertow's binary packages.

## Maelstrom and Undertow LED/toggle controls, issues #34 and #36

On 2026-10-18 the maintainer confirmed the LED/toggle artwork was generated with
Gemini Nano Banana using a personal Gmail account and approved its use in the
project. The source sheet used the assistant-provided prompt; no third-party
reference material was used. No exact model revision or generation date is known.
The original files remain unchanged and gitignored. Four prepared derivatives are
embedded in Maelstrom: lit/unlit LED states and left/right toggle states. Both
toggle images come from the isolated toggle PNG; the right position mirrors the
left artwork, and the UI offsets it so the socket stays fixed. These prepared
assets are used by Maelstrom's Smooth/Grit control and Undertow Preview's Sync
control and effect-status LED. The public project and plugin use is approved to
the extent applicable rights allow;
this does not assert exclusive ownership or guaranteed third-party clearance.
See `runtime/MAELSTROM-NOTICE.md` for hashes, preparation details and the notice
copied with development binaries.

## Whitecap and Riptide concept artwork, issue #42

On 2026-10-18 the maintainer confirmed the Whitecap and Riptide face images were
generated with Gemini Nano Banana from the assistant-provided prompts, without
third-party image references, and approved prepared derivatives for public use
under GPL-3.0-only to the extent applicable rights exist. The clipboard originals
remain unchanged and gitignored. Their hashes, preparation steps and limits are in
[`runtime/FUTURE-PEDALS-NOTICE.md`](runtime/FUTURE-PEDALS-NOTICE.md). The derivatives
are used by the display-only mockups and Whitecap's VST3 preview editor. No further
DSP or plugin work beyond the separately authorized Whitecap issues is implied by
this visual concept task.

## Squall concept artwork, issue #49

On 2026-10-19 the maintainer supplied the Squall enclosure image from the system
clipboard and approved its prepared derivative for public inclusion in the
display-only mockup. The original remains unchanged and gitignored at
`ui/textures/squall-enclosure-v01.png`. Its SHA256, preparation steps and limits
are recorded in [`runtime/SQUALL-NOTICE.md`](runtime/SQUALL-NOTICE.md). The
display-only mockup and Squall's VST3 preview editor embed only
`runtime/squall-enclosure.png`; they do not use the original at build time. The
generator, exact model revision and generation date have not been independently
confirmed, and no exclusive copyright or third-party clearance is claimed.

## Undertow pedal concept

- File: `concepts/undertow-pedal-v01.jpeg`
- Original supplied filename: `assets/concept/FullPedal.jpeg`
- Purpose: visual reference for the blue enclosure, silver controls and wave motif.
- Changes here: moved/renamed only; original JPEG bytes preserved.
- Generator/service: Banana through Google Gemini; exact model unconfirmed.
- Generation date and prompt/source inputs: awaiting maintainer details.
- Output terms/licence and permission to redistribute: not yet confirmed.

## Undertow enclosure surface

- File: `ui/textures/undertow-enclosure-v01.jpeg`
- Original supplied filename: `assets/textures/Pedal.jpeg`
- Purpose: candidate blue enclosure surface; not yet a prepared runtime asset.
- Changes here: moved/renamed only; original JPEG bytes preserved.
- Generator/service: Banana through Google Gemini; exact model unconfirmed.
- Generation date and prompt/source inputs: awaiting maintainer details.
- Output terms/licence and permission to redistribute: not yet confirmed.

## Additional local originals

All supplied images remain byte-for-byte unchanged and are excluded from Git by
the root `.gitignore`. These three have not been prepared as runtime assets:

| File | Role | Preparation needed |
| --- | --- | --- |
| `concepts/undertow-knob-v01.jpeg` | Silver-knob reference | Separate static lighting from animated pointer; remove background if used |
| `concepts/undertow-footswitch-v01.jpeg` | Footswitch reference | Simplify unusual mounting hardware; remove background if used |
| `ui/artwork/undertow-wave-v01.jpeg` | Wave graphic | Isolate lines or redraw; JPEG includes a solid blue background |

Service: Banana through Google Gemini, as reported by the maintainer. Exact model,
dates, prompts/inputs and account-specific redistribution terms remain unconfirmed
for these files too. No licence has been assigned to the images by this record.

## Terms review and publication status

[Gemini Apps image guidance](https://support.google.com/gemini/answer/14286560?hl=en)
requires respecting third-party rights and notes that work/school account terms
may differ. [Google's general terms](https://policies.google.com/terms?hl=en) say
Google does not claim ownership over original generated content. That is not a
guarantee of exclusive copyright, clearance or applicability to every account.
The review is informational, not legal advice. Do not treat API terms as a
substitute for the terms of the app/account actually used.

The first compiled mockup was original procedural C++ drawing. Issue #15 now
embeds only the prepared enclosure derivative, not the local originals. All labels
and controls remain live C++ drawing, separate from the texture. Its notice is
copied to `bin/notices/Undertow-artwork.md` by CMake.

## Before publishing or bundling the images

Record the service/model, source inputs, date and available prompt. Link the
applicable output terms and check redistribution in both the public repository
and compiled application. Record any third-party restrictions and required
attribution. The project's GPL-3.0-only declaration does not establish rights
over unverified supplied assets. Keep these files local until their status is
confirmed. This precaution continues to apply to additional assets, not the
explicitly approved prepared enclosure described above.
