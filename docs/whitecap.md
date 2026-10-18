# Whitecap DSP core

Whitecap is in design and DSP development under issues [#38](https://github.com/marrabld/stormsurge-fx/issues/38)
and [#44](https://github.com/marrabld/stormsurge-fx/issues/44). It remains a separate
effect from Undertow and Maelstrom. This core has no plugin identity, host adapter,
parameter/state schema or UI integration.

## Musical contract

- Chorus blends the dry signal with a modulated delay return. Vibrato is fully wet.
- New instances default to Chorus. The proposed default sound is gentle, with a
  subtle dry/wet blend and modest stereo motion.
- Rate is intended to move from slow/flowing to moderately quick; extreme wobble is
  not the goal. Depth should range from subtle to clearly noticeable.
- Width affects both modes. Its minimum has coherent left/right movement; larger
  values introduce a modest phase difference between the channels. Mono usefulness
  is a design goal, not a guarantee against every possible phase interaction.
- Parameter and mode changes ramp over 20 ms. A mode change blends into the new
  output rather than leaving an independent long tail.
- The first listening reference is after the amp, but the effect should work either
  before or after an amp model.

## Provisional engineering values

The maintainer approved conservative implementation values as provisional pending
private listening review:

| Control | Initial range | Starting value |
| --- | ---: | ---: |
| Rate | 0.1–5 Hz | 0.5 Hz |
| Depth | 0–2 ms peak modulation around the centre delay | 0.5 ms |
| Chorus Mix | 0–100%, linear dry/wet blend | 20% |
| Width | 0–90 degrees of LFO phase separation | 20 degrees |
| Mode | Chorus / Vibrato | Chorus |

The modulated signal uses a 10 ms centre delay, so it has intrinsic signal delay.
This is a DSP property, not a host-latency report. Any plugin integration must decide
how to report and verify it before a plugin is built.

These ranges and values are engineering starting points, not final sound-design
approval or demonstrated listening results. Review them against the private test
passage described in issue #38 before product integration. Private recordings must
stay out of the repository and release assets.

## Core checks

```sh
ctest --preset linux-debug -R '^whitecap_core$' --output-on-failure
```

The numerical test covers centre-delay timing at supported sample rates, Chorus and
Vibrato routing, coherent and widened stereo behavior, a representative mono sum,
parameter/mode smoothing, reset, invalid inputs, and ordinary C++ allocation counts
in selected processing calls. These checks do not establish that Whitecap sounds
good; that requires private musical listening.
