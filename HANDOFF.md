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

[docs/design.md](docs/design.md) records the approved first-core choices and
engineering starting points separately from remaining integration proposals.

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

PR #8 brought the foundation and guides into `main`; that merge is complete.
PR #9 added the basic delay core; merged PR #10 connected separate **Undertow Preview**
VST3/JACK targets and working controls. Issue #11 now adds a Normal/Ping-pong switch,
with left-first alternating new repeats and unchanged dry routing. Foundation remains
pass-through. Tempo sync, native device usability and accurate host tails remain work.

| PR | Branch | State | Contents |
| --- | --- | --- | --- |
| #6 | `docs/undertow-work-plan` | Merged into `main` | Planning docs, issues #1–#5 |
| #7 | `feat/undertow-foundation` | Merged into `docs/undertow-work-plan` | Pass-through VST3 + JACK standalone, procedural pedal GUI |
| #8 | `docs/consolidate-project-state` | Merged into `main` | Foundation stack and agent/handoff guides |
| #9 | `feat/3-dark-delay-core` | Merged into `main` | Tested reusable basic dark delay |
| #10 | `feat/4-playable-delay` | Merged into `main` | Audible Preview controls, automation/state and monitor safety |

The foundation builds and passes automated tests on Linux and Windows, including
sanitizers. The VST3 passed pluginval at strictness 5, with DPF teardown warnings
documented but unresolved. The maintainer confirmed Foundation scan/load, clean
recorded-guitar playback and saved-project reopening in Bitwig Studio Producer
6.1.3 on Omarchy, then audible basic Preview repeats, knob changes, bypass spillover
and non-default saved-setting reopening for PR #10. Linux, Windows and sanitizer CI
passed for that PR. Ping-pong still needs its own listening/reopening check; none of
these results proves Windows playback or standalone hardware behaviour.

## Suggested order

1. Review the issue #11 Ping-pong PR and CI. Do not merge without approval.
2. Follow [preview.md](docs/preview.md) to hear alternating echoes in stereo and
   verify mode/bypass/saved settings. Test automation and exports manually too.
3. Agree and implement tempo sync and remaining channel policies under issue #4.
4. Resolve DPF's hardcoded zero VST3 tail report before claiming correct automatic
   tail export. Test standalone input routing and playtest under issue #5.

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
