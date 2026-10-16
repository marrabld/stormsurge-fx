# Handoff brief

A short brief for continuing StormSurge FX in a fresh session, especially with a
different assistant that has no prior context. For working rules and commands, see
[AGENTS.md](AGENTS.md). For the musical spec, see [docs/design.md](docs/design.md).

## Who the maintainer is

A musician who uses Linux (Arch/Omarchy), not a professional software developer.
They use AI to help build open-source tools. Explain technical tradeoffs in plain
terms rather than assuming they know build systems, and guide rather than take
over. A previous project, [Omazone](https://github.com/marrabld/omazone), is in
community beta testing; this is separate work, not a port of it.

## Goal

GPL-3.0-only guitar effects for Bitwig and standalone use. Linux first, Windows
planned, no macOS commitment. Named after a planned song, **StormSurge**; the first
musical milestone is to use Undertow on that track. The first effect is **Undertow**,
a dark delay. Longer term: a small pedalboard of effects worth playing through, not a
commercial suite or a reproduction of any existing product.

## Musical direction

Undertow should be dark, rhythmic repeats with stereo ping-pong and tempo sync,
keeping dry guitar clear while repeats fill the gaps. The listening reference is
Devin Townsend's *Deadhead*.

An internet recipe describes that ambience as: mono delay at 800 ms, 75% feedback,
into a ~10 s reverb, into a stereo ping-pong delay at 75%, with the reverb/pad
high-passed near 200 Hz and low-passed near 4 kHz, plus an octave-up shimmer and a
compressor.

**Treat that recipe as an unverified hypothesis, not a specification.** The "That
Pedal Show" attribution is unconfirmed, "75%" may mean mix or feedback, and 75%
feedback is not a loudness safety limit. The 800 ms figure is only an experimental
starting point. The long reverb, octave-up shimmer and compressor are outside the
first delay.

Concrete proposed defaults now sit in [docs/design.md](docs/design.md) — time range,
crossfade policy, mix law, feedback ceiling, tone placement, ping-pong topology,
bypass/tail behaviour and tempo divisions. **They await the maintainer's approval.**

## Decisions already made

- **DPF, not JUCE.** Current JUCE is AGPLv3 or commercial, not plain GPLv3, so it
  was rejected to keep this project GPL-3.0-only. DPF is ISC-licensed and provides
  custom UIs, VST3 and a JACK-only standalone.
- C++20, CMake, Ninja/Clang on Linux, MSVC x64 on Windows.
- **Artwork:** the maintainer generates images with Banana through Google Gemini and
  uses them as references. They are not baked into the UI; the interface is drawn
  procedurally in C++. The generated files stay local and gitignored because
  redistribution terms are unconfirmed.
- The standalone is JACK-only with native-device fallback disabled. It **starts
  muted** and requires explicit monitoring consent, which resets on every audio
  reactivation. Friendly device selection and native Windows backends are later work.

## Where things stand

Planning PR #6 is merged into `main`. Foundation PR #7 merged into its base branch
`docs/undertow-work-plan`, which is **not yet merged into `main`**. That branch is
currently ahead of `main` by the full foundation commit series, so a follow-up PR
into `main` is still needed.

| PR | Branch | State | Contents |
| --- | --- | --- | --- |
| #6 | `docs/undertow-work-plan` | Merged into `main` | Planning docs, issues #1–#5 |
| #7 | `feat/undertow-foundation` | Merged into `docs/undertow-work-plan` | Pass-through VST3 + JACK standalone, procedural pedal GUI |

The foundation builds and passes automated tests on Linux and Windows, including
sanitizers. The VST3 passed pluginval at strictness 5, with DPF teardown warnings
documented but unresolved. **There is no delay processing yet**, and Bitwig 6.0.11
has not been tested in the real DAW.

## Suggested order

1. Open and merge a PR from `docs/undertow-work-plan` into `main` so the foundation
   is on the default branch.
2. Load the pass-through VST3 in Bitwig 6.0.11 on Linux. Confirm it scans, plays,
   and does not change audio. Close and reopen a project.
3. Walk the maintainer through the proposed defaults in `docs/design.md` and get
   approval, amendment or rejection for each. Record the outcome.
4. Implement the delay DSP (issue #3) to the approved spec, with tests for timing,
   feedback stability, time-change transitions and bypass/tail behaviour.
5. Add ping-pong and tempo sync (issue #4).
6. Playtest on a real guitar (issue #5).

Still unknown from the maintainer: the audio interface model, whether they will play
live, process recorded tracks, or both, and a permitted short dry guitar recording
for testing.

## Constraints to preserve

- Never silently attenuate, clip, resample or change level. No internal limiter or
  hidden gain stage in the delay; instability must be fixed, not masked.
- Keep the monitor gate's fail-safe mute behaviour and its tests passing. Bypassing
  the effect must never unmute monitoring.
- Allocate delay buffers outside the audio callback; the test harness asserts zero
  allocations during processing.
- Keep DSP host-independent so the plugin and standalone share one implementation.
- Don't merge, publish releases or close issues without explicit approval.
- Record honest limitations. A clean compile is not proof of playback, host
  compatibility or good sound. Numerical tests are not listening evidence.

## If context is lost

Read `AGENTS.md` first: it has the build commands, the test facts that aren't
obvious from the code, the safety invariants, the licensing rules and the claims
discipline. This file explains the *why* behind the project.