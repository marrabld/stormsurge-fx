# Roadmap

This is a direction, not a release schedule. Undertow Preview now has the delay,
Ping-pong, tempo sync, optional wet-path reverb and working pedal interface.
The current task is a cross-platform experimental plugin preview, not an entire
pedalboard or a finished standalone device workflow.

## First work plan

Work through one issue at a time. Release preparation is authorised under #21;
publication/tagging and PR merging still require explicit approval. Preserve the
approved choices in [design.md](docs/design.md) and keep unapproved device/host
proposals separate. Do not merge automatically.

| Order | Issue | Outcome | State |
| --- | --- | --- | --- |
| 1 | [Define the sound and controls](https://github.com/marrabld/stormsurge-fx/issues/1) | Agreed delay sound and headroom policy | Core choices approved; issue sign-off remains the maintainer's |
| 2 | [Choose framework and prove builds](https://github.com/marrabld/stormsurge-fx/issues/2) | GPL-compatible foundation and build targets | Implemented, with Linux/Windows and now Mac CI; not universal playback verification |
| 3 | [Build the basic dark delay](https://github.com/marrabld/stormsurge-fx/issues/3) | Reusable, numerically tested DSP | Implemented in merged PR #9 |
| 4 | [Make it playable](https://github.com/marrabld/stormsurge-fx/issues/4) | Host integration, Ping-pong, Sync and safety | Preview works in tested Bitwig; accurate tail reporting and device policies remain |
| 5 | [Playtest the preview](https://github.com/marrabld/stormsurge-fx/issues/5) | Listening, exports and usable packages | First Linux listening successful; final-package and other-host checks pending |

PR #8 already consolidated the foundation into `main`; do not repeat that merge.
PRs #10/#12/#14/#17 added audible controls, Ping-pong, Sync and reverb. PR #18
refined the GUI; PR #20 added experimental Mac plugins. No feature issue is
silently closed merely because its implementation merged.

## Current: prepare the experimental preview

Issue #21 and PR #22 prepare optimized Linux x86_64 VST3, Windows x86_64 VST3 and
Mac universal VST3/AUv2 archives, matching recursive source and checksums. They
exclude Foundation and standalone from the initial advertised downloads.

CI/archive/loader checks are separate from listening. The maintainer needs a final
Linux package check including automation, multiple instances, saved settings and
an export with trailing silence. Windows DAW playback remains unverified; Dom's
Mac test is pending. Mac bundles are ad-hoc signed, not Developer ID signed or
notarized. Review [draft notes](release/NOTES.md) before any publication decision.

## 1. Agree on Undertow's sound

The musical direction is dark, spacious, rhythmic repeats with stereo movement,
inspired by what the maintainer enjoys in Devin Townsend's Deadhead. This is a
listening reference, not an exact-rig reconstruction or an endorsement.

An internet recipe suggesting 800 ms delay, 75% feedback, long reverb and a second
ping-pong delay is unverified. Use its numbers only as experiments, not guaranteed
safe settings. Preserve dry-guitar clarity while the wet ambience fills the gaps.

Approved core, Sync and fallback decisions are recorded in
[design.md](docs/design.md), separately from remaining host/device proposals.
The maintainer's outstanding input for live monitoring is the audio
interface model, whether they will play live or process recorded tracks, and a
permitted short dry guitar recording with gaps, palm-muted notes and a sustained
chord. No private music needs to be uploaded publicly.

Ping-pong and tempo sync belong in the first playable preview. Millisecond timing
also remains useful for offset echoes.

## 2. Choose the build foundation

Implemented in `main`:

- Selected foundation: C++20, CMake and DPF, after reviewing JUCE's current
  AGPL/commercial terms and obtaining maintainer approval for the GPL-compatible
  alternative. See [technology decision](docs/technology.md).
- Pinned dependencies, retained notices and a minimised pass-through shell.
- The shell builds and passes automated tests on Linux and Windows, including
  sanitizers, with DSP kept separate from hosts, interfaces and devices.

The maintainer tested Bitwig Studio Producer 6.1.3 on Omarchy. Real standalone
devices and Windows/Mac DAW playback still need testing. Release packages are now
being prepared under #21. A successful build is not playback evidence; issue
acceptance and closure remain the maintainer's decision.

The first standalone uses JACK-compatible routing and an initially muted monitor
gate, not a finished in-app device selector. Prepared enclosure artwork is now
approved and embedded; original local references stay ignored. See assets/SOURCES.md.

## 3. Understand and test the basic delay

- Prototype delay-line and feedback behaviour with generated signals and permitted
  guitar examples. An offline learning prototype is optional, not a prerequisite
  for the production architecture.
- Test repeat timing, decay, tone response and stereo/channel independence.
- Establish safe parameter ranges, startup/reset behaviour and numerical handling.
- Listen at low levels before making the delay playable through headphones.

## 4. Make Undertow playable on Linux

- Shared real-time DSP used by one plugin and one standalone target.
- Millisecond delay, feedback, mix, tone, output level and bypass.
- Alternating stereo ping-pong with explicit mono/stereo input and dry-path rules.
- Host-tempo sync with defined note divisions and behaviour when tempo is missing,
  stopped or changing. Standalone uses an explicit internal tempo.
- Smooth controls; no allocation, file access, blocking locks or analysis in the
  audio callback. Prepare buffers before playback.
- Handle sample-rate/block-size changes, zero-length callbacks where applicable,
  silence, transport changes and long tails without unstable state.
- Restore plugin settings in Bitwig projects and document tail reporting.
- Standalone device selection, explicit mono input mapping, meters and a clear
  response to unavailable devices. No unexpected microphone monitoring on launch.
- Simple readable controls before custom pedal graphics.

## 5. Playtest the first preview

Use the same dry guitar example across revisions and record listening observations
alongside numerical tests. Start at low monitoring volume. Fix core problems
before adding more effects; approval to publish a preview is a separate decision.

### Preview acceptance

- Builds reproducibly from documented source dependencies.
- Scans and plays correctly in the tested Linux Bitwig version.
- Project close/reopen restores the effect's parameters, with an explicit policy
  for unsaved delay-line contents. Don't promise sample-identical tail recall.
- Correct results across block sizes and tested sample rates, including offline
  host export, multiple instances and automated parameter changes.
- Tested bypass/tail behaviour, no obvious clicks on normal adjustments, and no
  uncontrolled feedback at supported settings. Safe settings don't replace
  sensible monitoring volume or an explicit output/headroom policy.
- Standalone launch, selected-device playback and error handling tested on real
  hardware. No claim of zero monitoring latency.
- Licence/notices, build instructions, limitations and a small listening example.

## After the preview: validate Windows distribution

- Extend the early Windows build checks into distributable plugin/standalone bundles.
- Validate real audio-device behaviour and installation on Windows; document
  backend requirements and any unsigned-build warnings.
- Test a Windows DAW rather than assuming cross-platform compilation proves use.
- Keep parameter/state compatibility with earlier Undertow projects.

Avoid Linux-only assumptions from day one even if Windows binaries arrive later.
Runtime validation may be brought forward if hardware/testers are available;
successful compilation alone is not playback evidence.

## Later: explore the ambience chain and next pedal

The maintainer has now authorised the first optional reverb experiment in issue #16:
existing Normal/Ping-pong delay feeding long dark reverb on wet only, with a separate
dry path. This is not yet the complete three-stage chain below. Other effects and
release publication remain separate decisions.

The larger musical destination is a parallel wet ambience path:

```text
Dry guitar ───────────────────────────────────────────────────→ Output
           └→ dark mono delay → long reverb → stereo ping-pong ─↗
```

This is a proposed listening experiment, not the first delay's architecture or a
promise to reproduce the reference recording. Reverb and the combined chain are
separate scope. A wet-path high-pass near 200 Hz and low-pass near 4 kHz are
starting suggestions, not fixed specifications or automatic dry-path filtering.
Ducking, shimmer and compression can wait until listening demonstrates a need.

Possible effects include tremolo, overdrive and reverb. Choose one at a time
based on listening and interest. Overdrive needs deliberate anti-aliasing work;
reverb needs its own design and listening tests. Neither is a trivial checkbox.

## Later: assemble a small pedalboard

Reuse the effect modules in an initially fixed-order chain with individual bypass
and saved settings. Decide whether the board is an additional plugin/application
without breaking the identity of released individual effects.

Drag-and-drop ordering, parallel routes, third-party plugin hosting, elaborate
modulation, amp/cab modelling and additional plugin formats are separate future
decisions, not requirements for Undertow.
