# Foundation technology decision

## Scope and approval

The maintainer authorised the pass-through/plugin/standalone foundation and a
pedal-face mockup, and has since authorised implementing Undertow's delay engine.
Publishing a release remains unauthorised. The repository is already public; it was
created with the maintainer's approval. They selected DPF after being informed of
JUCE's AGPL/commercial terms. Own project code stays **GPL-3.0-only**.

The initial host target is **Bitwig 6.0.11**. Audio-interface details and whether
live input, recorded tracks or both come first are still to be supplied. Neither
question blocks a headless pass-through build; real monitoring requires them.

## Why DPF

The [current JUCE licence](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md)
offers AGPLv3 or commercial terms. We did not assume a commercial entitlement,
relicense the project, or choose an old JUCE revision to evade that decision.

[DPF](https://github.com/DISTRHO/DPF) is ISC-licensed and supports custom C++ UIs,
Linux/Windows VST3 and standalone targets. Its VST3 implementation uses its own
ISC-licensed interface definitions. These permissive dependencies can accompany
our GPLv3 code with their notices retained. This is an engineering licence
review, not a legal opinion or an assurance against third-party claims.

DPF and its nested Pugl dependency are pinned in Git. See
[third-party notices](../THIRD_PARTY_NOTICES.md) for the exact revisions and
rendering/font dependencies. DPF's pinned CMake helper accidentally treats its
optional browser generator-expression flags as true. Our CMake removes those
unused definitions consistently from DGL and its interface; upstream source is
not modified. This workaround must be reviewed on dependency updates.

Windows configuration also fetches hash/commit-pinned MIT-licensed Khronos
extension headers for NanoVG. DPF's optional POSIX-threaded bundled self-test is
disabled for MSVC; our processor harness tests still run on Windows. Both measures
are documented in the build guide and notices, rather than hidden in CI scripts.

## Tradeoffs

- Standalone begins as JACK-only to avoid implicit native-device fallback.
  Friendly device selection, Windows native backends and host testing remain work.
- The foundation is fixed stereo, not the final mono/stereo delay layout.
- Linux UI uses OpenGL and X11/XWayland. Headless DSP tests don't require either.
- Plugin UI is an original procedural blue/silver pedal sketch. No local Gemini
  images are compiled or uploaded as part of this foundation.
- There are no delay controls exposed to the host. Decorative knobs are explicitly
  labelled as placeholders rather than pretending to implement processing.
- This is a build/host experiment with a separate identity, not a stable preset
  format or a sound-design decision for the eventual effect.

## Art provenance

The maintainer confirmed **Nano Banana through Google Gemini** in a personal
Gemini account, with an assistant-supplied text prompt and no uploaded third-party
reference images. Exact model revision, generation date and original prompt text
have not been recovered.
[Gemini Apps image help](https://support.google.com/gemini/answer/14286560?hl=en)
points to Google's terms and warns users to respect others' rights; work/school
accounts may have different terms. The [general Google terms](https://policies.google.com/terms?hl=en)
say Google does not claim ownership of original generated content. That does not
establish copyright protection, eliminate third-party rights or prove that every
account is covered by the same terms. Do not substitute Gemini API terms for an
unknown Gemini Apps account.

Details are in [assets/SOURCES.md](../assets/SOURCES.md). The maintainer approved
enclosure reuse for issue #15. Preview embeds only the prepared derivative in
`assets/runtime/`, with its provenance notice accompanying binaries. Originals
remain local, unchanged and ignored. Foundation still uses procedural artwork.
