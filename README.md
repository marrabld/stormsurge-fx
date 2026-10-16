# StormSurge FX

Linux-first, GPLv3 guitar effects for Bitwig and standalone use. Starting with
Undertow, a delay, and growing toward a small pedalboard. Windows support planned.

Named after the maintainer's planned song, **StormSurge**. The first musical
milestone is to use Undertow on that track.

![Undertow Foundation procedural pedal face, with clearly labelled placeholder knobs](docs/images/undertow-foundation.png)

## Status

**Under construction.** Undertow Preview connects the dark-delay core to a VST3 and
JACK-only standalone with working pedal controls, switchable stereo ping-pong and
free-millisecond or tempo-synced timing. See [listening instructions](docs/preview.md).
The pictured Foundation mockup remains a separate pass-through plugin; its knobs
are still placeholders. There is no public release yet.
This is a separate project from [Omazone](https://github.com/marrabld/omazone),
not a port of its offline mastering chain.

## First effect: Undertow

A delay for recorded guitar in Bitwig and live playing through an audio interface.
The listening direction is dark, rhythmic repeats and stereo ping-pong, inspired
by the ambience the maintainer enjoys in Devin Townsend's Deadhead. No claim to
reproduce his exact rig or recording. The initial scope is deliberately small:

- Delay time in milliseconds.
- Feedback with bounded settings.
- Wet/dry mix.
- Tone control for darker repeats.
- Output level and bypass with a defined tail policy.
- Mono/stereo operation with supported channel layouts explicitly documented.
- Alternating stereo ping-pong and tempo sync for the first playable preview.

Tempo sync and ping-pong follow the tested basic delay, but are early requirements,
not distant extras. Long reverb and a combined ambience chain are later work.
The standalone application will need device selection and input/output metering.
The plugin uses its host's audio devices and transport.

## Platforms and formats

- **Linux first**, with Bitwig as the primary DAW for initial host testing.
- **Windows planned**, using shared DSP and interface code rather than a rewrite.
  Windows builds, device backends and host behaviour still need their own tests.
- **Plugin and standalone** are intended to share the same effect implementation.
- C++20, CMake and the ISC-licensed DPF framework are the selected foundation.
  JUCE's current AGPL/commercial terms were reviewed and not adopted. VST3 and
  JACK-only standalone build targets come first. CLAP and LV2 remain later options.
- macOS is not part of the initial commitment.

## What this is not

No complete pedalboard, amp/cab simulation, plugin hosting, complex routing or
preset marketplace is promised for the first release. Additional effects will
be chosen because the maintainer wants to play through them, not to fill a list.

For live electric guitar, use a suitable instrument input and begin at low
monitoring volume. Avoid accidentally monitoring both the interface's direct
signal and a delayed software copy. Recording latency depends on the complete
audio path; an effect's delay-time knob is a separate thing.

## Build and try it

See [build instructions](docs/building.md) and the [technology decision](docs/technology.md).
Use **Undertow Preview** for audible echoes and **Undertow Foundation** for the old
pass-through experiment. Standalone requires JACK-compatible routing and starts
muted until monitoring is explicitly enabled. Device selection and finished
standalone usability are not implemented yet.

To view the pedal face without opening any audio device:

```sh
cmake --preset linux-debug && cmake --build --preset linux-debug
./build/linux-debug/foundation_gui --view
# Working-control editor, also without an audio device:
./build/linux-debug/preview_gui --view
```

No automatic installation, live-input monitoring or release publication is part of
the build. The maintainer has checked Foundation loading, clean recorded playback
and project reopening in Bitwig Studio Producer 6.1.3 on Omarchy, followed by audible
basic Preview controls, bypass spillover and saved-setting reopening. The new
ping-pong mode also passed listening/reopen checks; new tempo sync needs its own
real-host checks. Supplied Gemini artwork remains local; the interface is drawn in code.

## Next steps

See the [roadmap](ROADMAP.md) and [design questions](docs/design.md).
The roadmap links five ordered GitHub issues for design, build foundations,
delay DSP, playable integration and preview validation. `docs/design.md` separates
approved basic-delay behaviour from remaining integration proposals.

The tested basic core from issue #3 is connected in a first integration slice of
issue #4. Ping-pong is tracked by issue #11 and tempo sync by #13. Host tail reporting
and standalone usability remain work.
Publishing a release is not authorised. Listening reports are welcome.

## Working on this

[AGENTS.md](AGENTS.md) holds the rules for AI assistants: build commands, test
facts, safety invariants and licensing requirements. [HANDOFF.md](HANDOFF.md) is a
short brief for picking up the project in a new session.

## Licence

Copyright contributors. Project material is licensed under **GPL-3.0-only**.
See [LICENSE](LICENSE). Future dependencies must be reviewed separately for
compatibility, notices and redistribution requirements. Using an effect does not
place your recordings or songs under the GPL.
