# Design questions before implementation

## Agreed direction

- StormSurge FX is the project; Undertow is the first delay.
- Learn by making an effect worth playing through, rather than replicating a suite.
- Bitwig on Linux is the primary initial DAW. Standalone use is also wanted.
- Windows is a planned destination, so platform-dependent code stays outside DSP.
- GPL-3.0-only project; DPF is the approved framework for the build foundation.
- The pass-through build foundation and GUI mockup are authorised and done, and
  implementing Undertow's delay engine is now authorised too. Publishing a release
  still is not.
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

## Approved first core, 2026-10-16

The maintainer approved these choices during guided testing and design discussion.
The decisions and host-test observations are also recorded in issues #1 and #3.

- Progressively darker repeats with an adjustable Tone control and a brighter setting.
- Crossfaded delay-time changes instead of intentional pitch glides.
- Time 20–2000 ms, starting at 400 ms.
- Feedback 0–90%, starting at 35%.
- Linear wet/dry blend, starting at 20% wet, with dry-only and wet-only endpoints.
- Output from silence to +6 dB, starting at unity gain.
- Bypass restores clean guitar, stops new notes entering the delay and lets existing
  repeats fade naturally. Standalone monitor mute is separate and silences everything.

`src/DarkDelay.cpp` implements the host-independent core for issue #3, not yet wired
into the plugin. Engineering starting points for review:

- Independent stereo lines, preserving per-channel dry routing. Mono-to-stereo,
  ping-pong and host channel layouts remain integration work under issue #4.
- One-pole lowpass on the wet read and in the feedback path. Every return is filtered,
  including the first. Cutoff 400–16000 Hz, initially 2500 Hz; capped at 45% of sample
  rate. These cutoff choices are starting points, not approved listening results.
- Fractional delay uses linear interpolation. A time change crossfades two stationary
  read heads over 50 ms; rapid requests are coalesced to the latest value after the
  current fade, rather than restarting fades and introducing discontinuities.
- Feedback, mix, filter pole, output gain and delay-input gate ramp over 20 ms.
  Bypass stops new input after that ramp and restores unity dry before output gain;
  the wet tail remains mixed at the selected mix. Output gain applies in both modes.
- `prepare()` allocates off the processing callback and clears the tail. It accepts
  sample rates from 8 to 192 kHz. `reset()` clears preallocated buffers without
  allocating. Neither may run concurrently with processing. No transport policy is
  imposed by this host-independent core.
- Nonfinite parameters fall back to defaults; finite out-of-range parameters clamp
  to the control range. Nonfinite input samples become silence. Tiny feedback/filter
  state below 1e-30 is flushed to zero. No limiter or clipping is added.
- Tests use ordinary over-range float signals as well as impulses and DC. This is
  not a promise of representable float output for arbitrarily large finite inputs.
  At maximum feedback, sustained DC can approach ten times the input before mix and
  output gain. Stable feedback does not guarantee quiet output.

The remaining proposals below are not blanket-approved. Some earlier explanations
are corrected here so they do not become implementation requirements by accident.

## Sound rationale and remaining proposals

**Clean or coloured repeats**
Intentionally coloured, via a one-pole darkening lowpass. No tape or analogue
emulation claims. Coloured repeats sit better under dark riffs and mask themselves
less as feedback rises; clean repeats are more forgiving to start with and easier to
hear as a distinct effect.

**Time-change sound**
Crossfade stationary read heads rather than moving the write pointer or bending
pitch. Overlapping echoes can still cause interference and audible timing changes;
crossfading is not a guarantee of artifact-free automation. The queued-change policy
above avoids restarting a partially completed fade.

**Time range and taper**
20–2000 ms, default 400 ms, approved. Knob taper is still a UI decision. A linear
mapping would give 300–600 ms only about 15% of the knob travel; a nonlinear mapping
could give that region more resolution. Maximum delay time is not tail duration.

**Mix law**
Approved linear blend: `out = dry·(1−mix) + wet·mix`. At 20% wet, dry gain is 0.8;
at the midpoint both gains are 0.5. This does reduce dry level as Mix increases.
Equal-power blending has different gains and depends on signal correlation; it
does not universally preserve perceived loudness. Neither law guarantees headroom.

**Feedback topology and ceiling**
Single loop: delay output → feedback gain → tone filter → back to the delay input.
Feedback is smoothed continuously and capped at **0.90**, not 1.0. Bounding below
unity alone does not make the loop safe, because the tone filter sits inside it — any
passband gain above unity would push real loop gain past the nominal setting. Hence
the three-part rule: one-pole filter (unity at DC, never boosts), 0.90 ceiling, and a
numerical test at worst case.

**Tone placement**
Inside the feedback loop, so each repeat is darker than the last. This gives the
cumulative darkening the direction calls for and helps contain feedback. Outside the
loop would keep every repeat equally bright: cleaner to articulate, less atmospheric.

**Ping-pong topology and stereo input**
Not settled. Cross-feedback alone cannot make identical signals in identical left
and right lines alternate. An alternating design needs an explicit input-injection
and first-repeat policy. Simply summing L+R also boosts correlated stereo input.
Keep this outside the independent-stereo basic core and agree routing before #4.

**Bypass and tail**
Approved: stop feeding new notes, preserve existing echoes and restore clean guitar.
Do not ramp away the audible wet tail. Reset and transport-jump policies still need
host-integration decisions. Tail reporting must account for feedback and filter decay,
not just one delay interval, and specify a decay threshold or supported host policy.
A hard cut can click and would discard the tail the maintainer wants to preserve.

Note the safety interaction: a ringing tail is the *opposite* of the standalone
monitor gate's fail-safe mute. Keep the two mechanisms clearly separate — bypassing
the effect must never unmute the monitor.

**Output headroom**
No internal limiter, clipping or automatic gain. Output from silence to +6 dB, default
0 dB, approved. Start monitoring low. Stable feedback can still accumulate substantial
level from sustained input, even before dry/wet summing or output boost. Numeric
headroom tests and listening at low levels remain separate requirements.

**Tempo sync**
Divisions: 1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16. The long
divisions give the ambience character; the short ones give rhythmic repeats. When
tempo is missing or the transport is stopped, fall back to free milliseconds and show
that state in the UI rather than silently holding a stale tempo.

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

Review the core and settle the remaining integration proposals before #4. Use
[issue #1](https://github.com/marrabld/stormsurge-fx/issues/1) for design decisions.
Bitwig Studio Producer **6.1.3** on the Omarchy laptop has passed manual foundation
scan/load, clean recorded-guitar playback and saved-project reopening. These were
listening/workflow checks, not a numerical null test. The audio interface model and
live-input workflow remain unknown; no private recording needs to be published.
The maintainer need not write technical specifications; explain each musical
tradeoff before asking for a decision.
