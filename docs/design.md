# Design questions before implementation

## Agreed direction

- StormSurge FX is the project; Undertow is the first delay.
- Learn by making an effect worth playing through, rather than replicating a suite.
- Bitwig on Linux is the primary initial DAW. Standalone use is also wanted.
- Windows is a planned destination, so platform-dependent code stays outside DSP.
- GPL-3.0-only project; no framework or SDK has been adopted yet.
- Planning and repository setup do not authorise implementing the audio engine.

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
- Independent stereo repeats or ping-pong? Pick one for the first release.
- What happens to the wet tail when bypassed, input stops or transport jumps?
- How should mono guitar reach a stereo output without changing dry level?
- Where do tone and any gain stages sit in the feedback loop?
- What is the maximum supported feedback and how do we handle output peaks?

Bounding feedback below unity alone does not guarantee safe levels for arbitrary
input. Filters, interpolation and parameter transitions also need analysis and
tests. Don't introduce clipping or limiting silently to hide an unstable design.

## Technology decision

Evaluate C++/JUCE with CMake for a VST3 and standalone starting point. Verify the
current framework licence, VST3 SDK terms, Linux dependencies and Windows audio
backend/distribution requirements before pinning versions. Compare alternatives
if CLAP/LV2 or licensing needs change the choice. Don't embed Python in the
production real-time callback merely because it helped with a prototype.

Bitwig support means testing the actual supported host/version. Neither a
successful build nor a plugin validator proves hardware monitoring works.

## First discussion

What should Undertow sound like on the song StormSurge: a tight rhythmic echo,
a wide ambient wash, or a darker trailing repeat? Use that answer to select the
smallest useful delay, without promising all three modes.
