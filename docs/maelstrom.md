# Maelstrom sound and controls

Design work for [issue #23](https://github.com/marrabld/stormsurge-fx/issues/23).
The maintainer approved the direction, four controls, starting settings, Level
range, unity bypass and mono/stereo routing goals below. Remaining engineering
policies and implementation choices are proposals, not yet approved. This document
does not authorise DSP implementation or claim that the pedal has been heard.

## Agreed direction

Maelstrom is a gritty tightening overdrive for tight, cutting melodic-death
rhythm guitar. It feeds NAM or another amp modeller. The amp supplies most of
the high-gain sound; Maelstrom shapes what hits it.

- Reduce loose bass before clipping so palm-muted low strings stay defined.
- Preserve useful pick attack and enough note separation for fast riffs and chords.
- Add grit and a useful mid-forward character, without committing to a particular
  Tube Screamer circuit or an exact analogue-emulation claim.
- Provide Drive, Tight, Bite and Level controls, plus effect bypass.
- Keep amp modelling, cabinet simulation and any noise gate outside the pedal.
- Make a separate plugin. Do not change Undertow's sound, identities, parameters,
  saved projects or standalone monitoring safeguards.

The DAW is the first pedalboard. Users can reorder pedals, use several instances,
mix them with other developers' plugins, or blend parallel tracks/buses. A shared
StormSurge rack or standalone chain host is not required for this first pedal.

## Intended chain

**Maelstrom → amp modeller → cabinet IR if needed → Undertow**

TONE3000 is a source of models/captures; a compatible loader runs the model. Check
whether a capture includes the cabinet/microphone before adding another cabinet
IR, and check its licence before redistributing anything. No model, IR or third-party
guitar recording is selected for inclusion in StormSurge.

Putting Undertow before the amp is also allowed, but its repeats then drive the
amp's distortion too. The chain above is the clearer starting point for rhythm
and delay listening, not a restriction on creative routing.

## Approved first controls

The maintainer approved these listening starting points on 2026-10-17. They are
not calibrated hardware values or evidence of a completed listening test.
Percentages describe knob position, not the percentage of distortion or bass removed.

| Control | Approved range/default | Intended audible result |
| --- | --- | --- |
| Drive | 0–100%, initially 10% | Nearly clean push through gritty overdrive. Low Drive is useful into an already saturated amp. Zero does not promise a transparent bypass. |
| Tight | 0–100%, initially 50% | Fuller low end through leaner, more percussive attack. It reduces bass before clipping, not just at the output. Maximum should remain useful for low-tuned riffs rather than simply making everything thin. |
| Bite | 0–100%, initially 50% | Smoother edge through sharper pick definition. Avoid treating maximum as a request for unfiltered fizz. |
| Level | Silence to +12 dB, initially 0 dB | Output gain after the tone/distortion stages. Boost drives the next amp harder; it is not a loudness-neutral control. |

The approved +12 dB ceiling allows roughly four times the output amplitude from
the Level stage. That can strongly change a downstream model's distortion and
produce clipping later in the chain. Starting at 0 dB avoids an automatic output
boost on insertion, but does not guarantee that the enabled pedal and bypass are
equally loud. Filtering and clipping change both level and perceived loudness.

Do not use automatic gain compensation to make the default sound louder or to
conceal these differences. Equal-final-loudness comparisons belong in listening
tests, with downstream amp drive kept explicit.

## Approved routing and bypass direction

- Aim for mono-in/mono-out for a recorded guitar and stereo-in/stereo-out for a
  stereo bus. Stereo would process channels independently with shared settings,
  no averaging, channel crossfeed or stereo widening. Identical left/right input
  should produce identical channels.
- Confirm what the pinned DPF adapters can negotiate before promising both
  layouts in one plugin. Mono-to-stereo expansion is not needed for this effect.
  If the first build must be stereo-only, seek approval and document host routing
  rather than silently changing this approved goal.
- Effect bypass should restore the original input at unity, ignoring Drive,
  Tight, Bite and Level. In particular, turning the pedal off should remove its
  output boost, as a normal pedal would.
- Bypass is not a live-monitor mute or permission to open an audio device.
  Maelstrom's first milestone is plugins, not a new standalone workflow.

### Transition and latency proposals

- Smooth enable/disable over a short transition, provisionally 10 ms. Smooth
  continuous control changes over approximately 20 ms. Judge clicks and rapid
  retargeting in tests; a fade is not a promise of artifact-free automation.
- There is no intentional delay or reverb tail. Do not invent Undertow-style
  spillover. If internal filters/oversampling add latency, keep bypass aligned and
  report the actual latency to the host. Host hard bypass remains host-controlled.

## Proposed input, headroom and lifecycle policy

These are engineering policies for review before #24, not additional musical controls.

- Accept ordinary floating-point audio, including finite samples above nominal
  full scale. Do not silently clamp input to ±1. Document the tested operating
  range before implementation is accepted; do not promise safe arithmetic for
  every representable floating-point value.
- The intended overdrive clipping is part of the sound. It must be distinct from
  an output safety limiter or clipping added to mask numerical instability. No
  hidden limiter, noise gate or automatic level compensation.
- Start listening quietly. Input level, Drive and Level all affect the result
  through a nonlinear amp. There is no agreed digital-to-analogue input calibration
  or universal NAM input level. Record the reference recording's peak/RMS and any
  loader input adjustment rather than silently normalising it.
- Proposed invalid-value behaviour: non-finite parameters use documented defaults;
  finite out-of-range values clamp to the control range. Non-finite samples become
  silence before they can contaminate filter state, including the bypass path.
  If a channel's internal arithmetic becomes non-finite, clear its processing state
  and output silence for the affected sample. Test recovery, not only detection.
- Prepare/reset outside concurrent processing. Activation and sample-rate changes
  clear processing history while preserving valid control settings. No obsolete
  filtered audio should burst out when re-enabling or reactivating the pedal.
- Provisional core sample-rate range is 44.1–192 kHz. Specify predictable rejection
  or safe behaviour for unsupported/invalid rates before coding. Support arbitrary
  valid block lengths, including empty and irregular blocks, without callback
  allocation. Buffer-size changes must not corrupt state or silently change tone.

## Implementation choices still open

Select the clipping curve, gain range/taper, filter orders/cutoffs, mid emphasis
and exact Bite behaviour under #24. Tight and Bite must have distinct musical
roles; do not lock in coefficients here without evaluating their interaction.

## Engine implemented under issue #24

`src/MaelstromOverdrive.cpp` is the host-independent core, tested by
`tests/MaelstromTests.cpp` (the `maelstrom_overdrive` ctest). It is a
display-free DSP module: no host, GUI, device or filesystem dependency, and it
does not touch Undertow.

Signal order is input → Tight highpass → pre-clip gain → clipping → Bite rolloff
→ anti-alias lowpass → output Level. Choices and why:

- **Tight is a two-pole highpass**, not a treble cut. A lowpass strong enough to
  change palm mutes also dulls the 1-4 kHz attack that makes a riff audible, so
  it is the wrong tool. Each pole forms its own highpass; subtracting one
  doubly-filtered signal instead produced a ~2 dB resonant bump near the cutoff.
  Cutoff sweeps 25–700 Hz, with a raised exponent so the midpoint lands near
  85 Hz and the default position is a mild push rather than a destructive cut.
  Measured at 48 kHz: 110 Hz falls about 8 dB at the default and 25 dB at
  maximum, while 2 kHz stays within 2 dB. Any highpass that tightens palm mutes
  also attenuates a down-tuned low fundamental, which is the intended trade.
- **Clipping is `tanh`**, chosen because it is smooth and monotonic everywhere,
  so the stage cannot fold back on itself or run away. A hard clamp would only
  create harsher edges and more aliasing. Drive maps 1.0 to 20x pre-clip gain on
  a squared curve, so 10% is a genuinely gentle push instead of a hidden few dB.
- **Bite is a post-clip lowpass** (4.5–11 kHz, clamped below Nyquist), so it
  shapes harmonics the clip stage just created rather than pre-empting them.
- **2x oversampling** with a gentle decimating lowpass. The anti-alias cutoff
  sits below the base-rate Nyquist, not the oversampled one; anything above it
  would fold back, which defeats the purpose. Reported latency is one sample.
  Near-Nyquist content under hard clipping measures more than 60 dB below the
  intended harmonics.
- **Bypass** is a unity dry path that ignores Drive, Tight, Bite and Level, and
  `processSample()` reaches the same signal through a 10 ms engage fade so
  switching cannot click or add level.
- Non-finite samples become silence and clear that channel's state; a
  non-finite result is caught and the channel is reset rather than emitting
  noise into the next stage. Unsupported sample rates are rejected explicitly.

Observed behaviour, not a listening verdict: with Level at 0 dB the output stage
adds no gain, while the default Drive position raises a quiet signal by roughly
2 dB. That is the intended push into the amp model, and it is why the approved
contract warns that bypass and enabled are not level-matched.

Numerical results establish routing, stability and spectral behaviour. They
cannot establish that Maelstrom sounds good. Oversampling factor, clipping
asymmetry, mid emphasis and the exact cutoff ranges remain adjustable after
real listening through NAM, and are expected to change.

## First listening and acceptance plan

1. The maintainer chooses a clean recorded guitar passage with palm-muted low
   notes, fast picking and chords. A private local recording is sufficient; no
   upload or public redistribution is required.
2. Choose an amp model that already sounds good without Maelstrom. Record loader,
   capture name/version or identifier, cabinet inclusion/IR, tuning, input level,
   sample rate and downstream settings. These references are still pending.
3. Compare enabled and bypassed at similar final listening loudness, without
   silently changing the signal entering the amp to compensate. Separate the
   intended effect of Level boost from simple louder-is-better bias.
4. Sweep Tight while keeping Drive low. Palm mutes should get more controlled,
   without losing all weight. Sweep Drive and Bite separately to judge grit,
   attack, chord separation, noise and fizz.
5. Repeat through another appropriate capture before tuning the pedal around a
   single model. Compare silence/noise without a gate hiding the pedal's behaviour.
6. Test isolated changes, rapid automation and enable/disable transitions. The
   plugin milestone also checks multiple instances, saved settings and offline
   rendering against playback.

Numerical tests can check routing, stability, lifecycle, spectra and selected
callback allocations. They cannot establish that Maelstrom sounds good. Listening
passes when the maintainer hears tighter riffs, distinct fast notes and defined
chords at useful settings, rather than only more gain or volume. Record failures
and remaining platform gaps alongside successes.

## Work order and approval boundary

After merging #23's design PR #28, the maintainer requested bringing the visual
mockup slice of #26 forward to review the appearance before DSP work. This changes
the order only for the display-only prototype, not host/plugin integration.
See [the UI mockup](maelstrom-mockup.md). The remaining DSP/integration sequence
below is unchanged.

1. [#23](https://github.com/marrabld/stormsurge-fx/issues/23): agree this contract.
2. [#24](https://github.com/marrabld/stormsurge-fx/issues/24): reusable DSP and tests.
3. [#25](https://github.com/marrabld/stormsurge-fx/issues/25): separate plugin, stable controls/state.
4. [#26](https://github.com/marrabld/stormsurge-fx/issues/26): shared-family interface and runtime art.
5. [#27](https://github.com/marrabld/stormsurge-fx/issues/27): NAM listening, host checks and preview preparation.

The maintainer approved the charcoal/red artwork direction and adapting the supplied
wide enclosure texture rather than stretching its bevels. Preserve its original,
reuse the existing hardware materials, and confirm the new image's provenance
before bundling. Dimensions and layout still need a rendered review.

Each feature gets its own branch/PR from updated main after the previous merge.
No automatic merge, issue closure, plugin installation or future release approval.
Undertow's experimental release remains available for feedback independently.
