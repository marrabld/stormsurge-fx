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

GPL-3.0-only effects for Bitwig and standalone use. Linux first, with Windows
support and experimental macOS plugin support. Named after a planned song,
**StormSurge**; the first musical milestone was to use Undertow on that track.
Undertow remains the first effect. The project now also has Maelstrom, Whitecap and
Squall. The longer-term goal is a small set of effects worth playing through, not a
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
- Issue #19 now authorises experimental Mac VST3/AUv2 plugins before release
  packaging. A colleague is willing to test; their Mac/OS/DAW details are pending.
  First presets target universal arm64+x86_64 and macOS 13. No Mac standalone yet.
- **Artwork:** the maintainer confirmed personal Gemini Nano Banana, using an
  assistant-supplied text prompt without uploaded third-party reference images.
  They approved reuse of the enclosure for #15. Its prepared PNG and provenance
  notice are in `assets/runtime/`; originals remain unchanged and gitignored.
  Controls and labels stay separate from the background. See `assets/SOURCES.md`.
- The standalone is JACK-only with native-device fallback disabled. It **starts
  muted** and requires explicit monitoring consent, which resets on every audio
  reactivation. Friendly device selection and native Windows backends are later work.

## Where things stand

PR #8 consolidated the initial foundation. Do not repeat that work. The current
pedal state is:

| Pedal | Implemented | Listening / validation still needed |
| --- | --- | --- |
| Undertow | Delay, ping-pong, tempo sync, optional wet reverb; Linux/Windows VST3, experimental macOS VST3/AUv2, and JACK-only standalone. Preview `v0.4.0-preview.1` has been published with approval. | Broader host/platform checks, final-package/export behavior, accurate host tail reporting, and standalone device usability. See [preview guide](docs/preview.md), [roadmap](ROADMAP.md), and [Mac notes](docs/macos.md). |
| Maelstrom | Host-independent overdrive DSP and separate plugin/UI work are implemented; mockup is display-only. | Listen through NAM or another amp modeller before judging its provisional tone. See [design and listening plan](docs/maelstrom.md). |
| Whitecap | Host-independent chorus/vibrato DSP and Linux/Windows VST3 listening preview. | Private listening in a song; refine only if that reveals a need. See [Whitecap notes](docs/whitecap.md). |
| Squall | Host-independent vocal DSP and Linux/Windows VST3 listening preview; installed locally at the maintainer's request. | Listen on recorded vocals; check mono-track handling and reverb-tail behavior. DSP settings remain provisional. See [Squall notes](docs/squall.md). |

There is no open PR at the time this handoff was updated. GitHub has open issues
for both active listening/design questions and work already implemented; do not
assume an open issue means the code is missing. Do not close issues without the
maintainer's approval. No further pedal should be started until its role and scope
are chosen; Riptide and Ebbtide remain concepts, not committed implementations.

The maintainer confirmed Undertow playback, project reopening, ping-pong and basic
sync/reverb behavior in Bitwig. This is not exhaustive export or cross-platform
validation. Numerical tests and CI do not establish sound quality. Private audio
must not be committed or uploaded publicly.

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
