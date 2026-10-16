# StormSurge FX

Linux-first, open-source guitar effects for Bitwig and standalone use.
Starting with **Undertow**, a delay, and growing toward a small pedalboard.

Named after the maintainer's planned song, **StormSurge**. The first musical
milestone is to use Undertow on that track.

## Status

**Planning only.** There is no audio implementation, plugin or download yet.
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
The standalone application will
need device selection and input/output metering. The plugin uses its host's
audio devices and transport.

## Platforms and formats

- **Linux first**, with Bitwig as the primary DAW for initial host testing.
- **Windows planned**, using shared DSP and interface code rather than a rewrite.
  Windows builds, device backends and host behaviour still need their own tests.
- **Plugin and standalone** are intended to share the same effect implementation.
- C++ with JUCE, CMake, VST3 and standalone targets is a candidate, not a settled
  dependency choice. Confirm current licences and platform support before adoption.
  CLAP and LV2 are possible later formats, not first-release promises.
- macOS is not part of the initial commitment.

## What this is not

No complete pedalboard, amp/cab simulation, plugin hosting, complex routing or
preset marketplace is promised for the first release. Additional effects will
be chosen because the maintainer wants to play through them, not to fill a list.

For live electric guitar, use a suitable instrument input and begin at low
monitoring volume. Avoid accidentally monitoring both the interface's direct
signal and a delayed software copy. Recording latency depends on the complete
audio path; an effect's delay-time knob is a separate thing.

## Next steps

See the [roadmap](ROADMAP.md) and [design questions](docs/design.md).
The roadmap links five ordered GitHub issues for design, build foundations,
delay DSP, playable integration and preview validation.
Agree on the first release's behaviour before implementing DSP or choosing a
framework. Contributions and listening reports will be welcome once there is
something to build and test.

## Licence

Copyright contributors. Project material is licensed under **GPL-3.0-only**.
See [LICENSE](LICENSE). Future dependencies must be reviewed separately for
compatibility, notices and redistribution requirements. Using an effect does not
place your recordings or songs under the GPL.
