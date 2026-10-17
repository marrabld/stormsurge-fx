# Design questions before implementation

## Agreed direction

This page records Undertow's design. The second pedal, Maelstrom, has a separate
[sound/control contract](maelstrom.md) under issue #23, with approved direction
distinguished from proposed defaults and policies. The DAW hosts the separate
pedals; StormSurge does not yet provide a shared rack or chain host.

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

The original first-delay scope excluded long reverb, the combined wet chain,
ducking, compression and shimmer. After successful delay/ping-pong/sync listening,
the maintainer authorised optional long dark reverb under issue #16. Preserve a
distinct dry path and take the remaining layers one at a time. The goal is clear
riff attacks with space behind them, not every part of the reference at once.

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

`src/DarkDelay.cpp` implements the host-independent core for issue #3. The separate
Undertow Preview plugin/standalone connect it in the first slice of issue #4. See
[preview.md](preview.md) for controls, reset policy and listening checks. Engineering
starting points for review:

- Normal mode uses independent stereo lines, preserving per-channel dry routing.
  Ping-pong is specified below under issue #11. Mono-to-stereo channel layouts
  remain integration work under issue #4.
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
The maintainer requested switchable stereo ping-pong as the next feature after
confirming the basic delay works in Bitwig. Issue #11 records this implementation:

- Normal is the unchanged default. Ping-pong starts new repeats left, then alternates
  right/left through full cross-feedback. Time is the interval between channel hops.
- Wet input is `(L+R)/2`, injected left only. Both inputs duplicated mono retain
  their level; single-sided input is halved; opposite-phase stereo can cancel in wet.
  Dry remains per-channel, with the existing Mix and output gain unchanged.
- Smooth injection and self/cross-feedback weights over 20 ms. The feedback matrix
  is a convex blend of identity and channel swap, keeping its row gain at unity
  before the bounded feedback gain. Compute both filtered returns before either write.
- Do not clear tails on a mode change. Existing symmetric tails can stay centred;
  new input after settling alternates. This avoids promising immediate hard-panned
  alternation or artifact-free switching of arbitrary waveforms.
- Append the mode parameter without moving existing effect or monitoring IDs.
  Preserve plugin identity/schema, with Normal default for old projects lacking mode.

These explicit routing choices avoid the earlier incorrect proposal to write
identical mono input into both lines, which would not create alternation. They do
not implement mono-input negotiation, a width control or tempo sync.

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
Approved under issue #13: retain free milliseconds; add quarter, eighth and sixteenth
notes, each straight, dotted and triplet. Valid host tempo applies while stopped as
well as playing. Missing/invalid tempo holds the last valid BPM; before the first
valid tempo use an editable, labelled 120 BPM default fallback. Standalone uses an
editable internal tempo. These choices supersede the earlier proposal to switch to
milliseconds when transport stops.

Free time stays 20–2000 ms; sync extends to 8000 ms. A target outside 20–8000 ms is
explicitly flagged `SYNC LIMIT`, not advertised as an exact selected note. Engineering
bounds are 20–300 BPM for INT/FB, and 1–1000 BPM for valid host tempo. Follow tempo
changes through the existing 50 ms queued read-head crossfade. The displayed target
can update before a transition has finished. Sync does not quantize the dry input.

The Preview-only VST3 build wrapper preserves the host's raw tempo-valid flag via
`PreviewTempoBridge.hpp`. Do not use DPF's synthetic 120 BPM or BBT-valid flag to
infer clock availability. Bridge, resolver, readouts and core tests cover policy;
actual loaded-plugin behaviour still needs the manual checks in `preview.md`.

Issue #19 adds a Preview-only Audio Unit wrapper using the raw beat/tempo callback
result rather than DPF's synthetic/stale BBT values. It follows the same BPM,
hold/fallback and stopped-transport policy as VST3. The wrapper queries the callback
once per processing slice and shares that result with DPF. Mac plugin builds and
headless adapter tests are not listening or DAW compatibility evidence. See
[Mac tester checklist](macos.md); native Mac standalone devices remain out of scope.

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

Review the remaining proposals before their respective integration follow-ups. Use
[issue #1](https://github.com/marrabld/stormsurge-fx/issues/1) for design decisions.
Bitwig Studio Producer **6.1.3** on the Omarchy laptop has passed manual foundation
scan/load, clean recorded-guitar playback and saved-project reopening. These were
listening/workflow checks, not a numerical null test. The audio interface model and
live-input workflow remain unknown; no private recording needs to be published.
The maintainer need not write technical specifications; explain each musical
tradeoff before asking for a decision.

## First integration slice

The maintainer requested a playable basic delay after merging PR #9. This slice
keeps Foundation unchanged and uses a separate Preview identity and parameter schema.
The initial host parameters were Time, Feedback, Mix, Tone, Output gain and Bypass;
issue #11 appends Ping-pong. Effect bypass
is deliberately separate from host hard bypass and standalone monitoring.

Implementation policies for this early preview, subject to real-host testing:

- Fixed stereo with independent lines in Normal or cross-feedback in Ping-pong.
  No mono-to-stereo mapping yet. Issue #13 appends sync timing as specified above.
- Audio activation/reactivation and rate changes clear tails but retain settings;
  buffer-size changes can induce reactivation. Transport moves alone do not clear
  tails while processing remains active. Parameter automation is smoothed, without
  a promise of sample-accurate host scheduling.
- DPF handles input-parameter state; delay-buffer contents and monitor consent are
  never saved. Monitor mute clears tails and silences all output.
- The dry path introduces no added processing latency. DPF's pinned VST3 adapter
  reports zero tail samples, which is not accurate for this delay. Automatic host
  tails remain unresolved. Test exports with explicit trailing silence meanwhile.

These policies enable an incomplete developer build, not completion of issue #4's
acceptance criteria or approval to publish a release.

## Optional dark reverb, issue #16

The maintainer approved long dark reverb after the wet delay, with clear original
guitar and Space/Darkness/Amount controls. This is a separate ambience experiment,
not approval of shimmer, compression or an exact artist-sound claim.

Engineering starting settings: nominal decay 1–12 s, initially 6; Darkness 0–100%,
initially 65; Amount 0–100%, initially zero so all old delay sessions stay unchanged.
The latter blends delay and reverberated delay within the existing wet path. No
dry processing, reverb inside delay feedback, or automatic output compensation.

An original eight-line orthogonal feedback network provides the tail; two fixed
all-pass diffusers per input build density. Wet-only 150 Hz high-pass and damping
roughly 12 kHz to 800 Hz are listening starting points. No buffer retuning or
allocation during controls/processing. Twenty-millisecond coefficient/Amount ramps,
non-boosting filters and sub-unity per-line feedback preserve the intended stable
design. Tests are not proof of every real-time property or of pleasing sound.

Core `WetInsert` runs after delayed/filter returns but outside delay feedback and
before final Mix/Level. It accepts DSP state, not a host or device. The plugin owns
both stages and resets both on activation/reconfiguration/monitor mute. Effect
bypass preserves both tails; Amount zero hides but does not clear the reverb state.

This stage follows the current Normal/Ping-pong delay, so diffusion may soften the
alternation. A second ping-pong after reverb remains separate scope. UI additions
only make room for the new controls; issue #15's redesign remains deferred.
See [preview.md](preview.md) for listening, state and export limitations.
