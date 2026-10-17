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
original image is currently bundled. Originals remain unchanged and gitignored.
`runtime/undertow-knob.png` is a separate original mathematical render, with the
source in `tools/render_knob.py`; the Gemini knob concept is a visual reference
only and its pixels are not embedded.

The per-original entries below retain the earlier review state for context;
the enclosure derivative's current inclusion status is the approval above.

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
