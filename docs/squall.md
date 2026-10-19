# Squall DSP core

Squall is a proposed vocal effect tracked in [design issue #48](https://github.com/marrabld/stormsurge-fx/issues/48)
and [DSP issue #52](https://github.com/marrabld/stormsurge-fx/issues/52). This first
step is a host-independent DSP core only. It has no plugin identity, UI, audio
device, host or filesystem integration.

## Agreed direction

- Clean keeps the natural voice and adds a subtle, short room/plate-style reverb.
- Heavy is for a growl the singer already makes. It should thicken and control the
  sound while keeping words clear, not turn clean singing into a growl.
- Both modes share the same reverb, so mode selection changes vocal color without
  changing the sense of space.
- The first control plan is effect bypass plus Clean/Heavy selection. There are no
  continuous controls in this core.
- No pitch shifting or compression is included in this first pass. Compression may
  bring breaths and harsh vocal noise forward, so it needs evidence from listening.
- The maintainer chose a VST3 effect for recorded vocals as the first listening
  target. A plugin integration issue follows after this core is reviewed and merged.

## Provisional DSP starting points

These are implementation starting values, not approved final settings. There has
not yet been private vocal listening through the DSP.

- Reverb uses four parallel feedback delay lines per channel, two short diffusers,
  a nominal 0.45-second decay and a 12% wet blend. Stereo channels use different
  delay lengths.
- Heavy applies an 85 Hz high-pass and a shallow 12% cut across the 180-420 Hz
  band before a soft saturation curve. It blends 40% of that curve with the
  filtered voice.
- Saturation, EQ and reverb values should be judged with recorded clean voice and
  real growls at matched loudness. Numerical tests do not establish musical quality.

The core accepts planar mono or stereo float buffers. A mono signal remains mono;
stereo input is processed per channel with modestly decorrelated reverb. Parameter
and bypass transitions ramp over 20 ms. `prepare()` allocates the reverb buffers
off the audio callback.

## Checks

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target squall_tests
ctest --preset linux-debug -R '^squall_core$' --output-on-failure
```

The tests check reverb response/reset, Heavy mode, mono/stereo processing, mode and
bypass transitions, invalid samples, supported rates, and ordinary C++ allocation
counts during selected processing calls. They do not prove the effect sounds good.
