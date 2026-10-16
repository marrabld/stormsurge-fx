# Design questions before implementation

## Agreed direction

- StormSurge FX is the project; Undertow is the first delay.
- Learn by making an effect worth playing through, rather than replicating a suite.
- Bitwig on Linux is the primary initial DAW. Standalone use is also wanted.
- Windows is a planned destination, so platform-dependent code stays outside DSP.
- GPL-3.0-only project; DPF is the approved framework for the build foundation.
- The pass-through build foundation and GUI mockup are now authorised. This does
  not authorise implementing the delay engine or publishing a release.
- Dark rhythmic repeats and stereo ping-pong are the initial musical direction;
  tempo sync belongs in the first playable preview, alongside millisecond timing.

## Listening target

The maintainer identified [Devin Townsend's Deadhead](https://www.youtube.com/watch?v=DrDM6V08wcE)
as the ambience reference. Use it for listening, not copied audio/assets or an
exact-reproduction claim. A quoted internet recipe describes an 800 ms mono delay
feeding long reverb and a stereo ping-pong delay. That attribution is unverified;
percentage controls are not comparable across devices and are not safe presets.

Start with a dark delay that can become a ping-pong effect. Keep a long reverb,
the multi-effect wet chain, ducking, compression and shimmer outside the first
preview. Preserve a distinct dry path. The goal is clear riff attacks with space
for repeats between phrases, not every part of the reference sound at once.

## Proposed boundaries

1. **DSP modules:** process supplied buffers using prepared state and explicit
   parameters. No host, window, device or filesystem dependencies.
2. **Plugin integration:** channel layouts, host automation, parameter/state
   persistence, tempo, latency and tail reporting.
3. **Standalone integration:** device lifecycle, routing, settings and error handling.
4. **Interface:** controls and meters communicating without blocking audio processing.

These are logical responsibilities, not instructions to create a large framework.
Extract only what the first effect actually needs.

## First musical decisions

- Clean digital repeats or intentionally coloured repeats? A simple darkening
  filter can come first without claiming to model tape or analogue hardware.
- Initial time range and whether time changes should audibly bend pitch.
- Ping-pong is wanted; agree its exact cross-feedback topology and stereo-input policy.
- What happens to the wet tail when bypassed, input stops or transport jumps?
- How should mono guitar reach a stereo output without changing dry level?
- Where do tone and any gain stages sit in the feedback loop?
- What is the maximum supported feedback and how do we handle output peaks?

Bounding feedback below unity alone does not guarantee safe levels for arbitrary
input. Filters, interpolation and parameter transitions also need analysis and
tests. Don't introduce clipping or limiting silently to hide an unstable design.

## Technology decision

C++20/CMake with DPF is now selected, with the maintainer's approval after the
licence check. Current JUCE's open-source terms are AGPLv3 rather than plain GPLv3.
See [the decision](technology.md) and [build instructions](building.md) for pinned
dependencies and the initial JACK-only standalone limitations. No embedded Python
is needed in the production real-time callback.

Bitwig support means testing the actual supported host/version. Neither a
successful build nor a plugin validator proves hardware monitoring works.

## Next discussion

Use [issue #1](https://github.com/marrabld/stormsurge-fx/issues/1) to settle the
first time range, feedback/mix controls, time-transition sound and bypass/tail
behaviour. Bitwig **6.0.11** is confirmed; interface details, preferred live versus
recorded workflow and a permitted dry riff are still to be supplied.
The maintainer need not write technical specifications; explain each musical
tradeoff before asking for a decision.
