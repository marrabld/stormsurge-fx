# Try the first audible delay

This developer build is **Undertow Preview**, not **Undertow Foundation**. Foundation
remains a separate pass-through plugin so old test projects do not suddenly change
sound. This is not a public release or the completed preview scope of issue #4.

## In Bitwig

1. Build and test the branch using [building.md](building.md). Nothing is installed
   into a plugin directory automatically.
2. Rescan `build/linux-debug/bin` in Bitwig's Plug-in Locations. If Bitwig cached an
   older preview build, close Bitwig before rebuilding and reopen/rescan afterwards.
3. On a quiet recorded-guitar test track, add **Undertow Preview**. Use only one
   instance initially. Do not confuse it with Foundation.
4. Start playback at low monitoring volume. The starting settings are 400 ms,
   35% feedback, 20% wet, 2500 Hz Tone, 0 dB output and Normal mode. Listen for fading echoes.
5. Drag knobs vertically, or scroll over them. Shift gives finer control. The
    displayed values and pointer positions follow host parameter changes too.
6. Reverb starts off. Use the lower Reverb row to add the new ambience; Amount zero
   preserves the existing delay sound exactly.

| Control | Range | Meaning |
| --- | --- | --- |
| Time | 20–2000 ms | Delay between repeats; changes crossfade without a deliberate pitch swoop |
| Feedback | 0–90% | How much of each filtered repeat returns for another pass |
| Tone | 400–16000 Hz | Lowpass cutoff; lower settings make repeats darker |
| Mix | 0–100% wet | Linear blend; raising Mix also reduces dry level |
| Level | Silence to +6 dB | Output gain, also applied during effect bypass |
| Footswitch | Delay / bypass | Stops new notes entering the delay and preserves existing echoes |
| Mode button above LED | Normal / Ping-pong | Independent stereo or left-first alternating repeats |
| TIME header button | MS / Sync | Free milliseconds or tempo-synced note intervals |
| NOTE header button | Nine divisions | Click forward, Shift-click backward; D = dotted, T = triplet |
| INT / FB header field | 20–300 BPM | Drag or scroll; Shift gives finer control |
| Reverb Space | 1–12 s, initially 6 | Nominal undamped decay, not physical room size or guaranteed measured RT60 |
| Reverb Darkness | 0–100%, initially 65 | More damping of high frequencies in the reverb feedback network |
| Reverb Amount | 0–100%, initially 0 | Blend delayed echoes with their reverberated version, on wet only |

Use the pedal footswitch or its **Bypass** parameter to test fading tails. Bitwig's
own device enable/hard-bypass button can stop processing and does not promise the
same spillover behaviour. Lower monitoring volume before raising feedback or Level.
There is no limiter, clipping or automatic level compensation.

## Long dark reverb

The new lower row is an optional ambience experiment toward the spacious character
of the listening reference. It is not an exact rig, preset or recording reproduction.

```text
Guitar dry ────────────────────────────────────────────→ existing Mix/Level → output
       └→ existing Normal/Ping-pong delay → wet reverb ─↗
```

Reverb Amount blends the original delay return with the reverb return. It does not
increase or filter the dry guitar. Master Mix still controls the dry/wet balance,
including the original reduction in dry gain as Mix increases. Amount zero retains
the original delay output bit-for-bit; Amount 100% makes the wet branch reverb-only.
Mix zero remains dry-only regardless of reverb settings. Level still applies to all.

Try Space 6 s, Darkness 55% and Amount 50% at low monitoring volume, using a short
phrase followed by silence or a sustained chord. Keep your existing delay settings
initially; if needed, bring master Mix toward 30–40% to hear the wash clearly. Compare
Amount zero and 50%, listen for clear attacks and a trailing cloud, and judge whether
it supports the guitar rather than obscuring it. Save/reopen with nonzero Amount.

The original eight-line feedback network uses orthogonal mixing, fixed input
diffusers and non-boosting damping. Space changes feedback loss, not buffer length;
it must not pitch-bend the reverb. Darkness maps damping from roughly 12 kHz toward
800 Hz, capped at 45% of sample rate. A 150 Hz high-pass affects only input to reverb.
These are engineering starting points for listening, not a promise of a specific
acoustic space or exact decay across all frequencies. Controls ramp over 20 ms.

The chain is the current delay, including Ping-pong if enabled, **then reverb**.
Reverb diffusion can blur left/right echoes. This is not yet the separate proposed
mono-delay → long reverb → second ping-pong chain, and adds no shimmer or compression.

Effect bypass lets both delay and reverb tails fade while new guitar stays clean.
Monitor mute clears both stages. Audio activation/rate changes clear both tails.
Amount zero hides reverb but keeps its internal state running, so raising Amount
while playing can reveal an already developing wash. No audio buffers are saved.
The editor is now 520×840, minimum 390×630, to fit three new controls. This is not the
broader GUI usability redesign tracked by issue #15.

## Stereo ping-pong

Click the **Mode** button above the LED, not the effect footswitch. In Ping-pong,
new repeats start left, then alternate right, left, right. Time is the gap between
successive repeats: 400 ms gives left at 400 ms, right at 800 ms, left at 1200 ms.
Feedback must be above zero to hear the second and later repeats.

Dry guitar keeps its original per-channel routing and Mix law. Only the wet input
is averaged as `(L+R)/2` and injected into the left line. This avoids a gain boost
for correlated stereo. Mono duplicated to both inputs retains its wet-input level;
single-sided input contributes half its amplitude, and opposite-phase inputs can
cancel in the wet path. This is an explicit downmix, not a stereo-preserving wet path
or a new mono-to-stereo input-layout implementation.

Normal retains independent lines. Mode changes ramp wet injection and feedback
routing over 20 ms without clearing tails. Old tails can remain centred and overlap
new alternating repeats; perfect alternation is only expected for new input after
the switch settles. Bypass and monitor mute retain their existing separate roles.

To listen, use stereo headphones or separated speakers at low volume. Try 400 ms,
50% feedback and 40% wet, play a short phrase, then listen into the gap. Confirm
left/right alternation, switch back to Normal, and save/reopen once in Ping-pong.
Do not assume a mono laptop/output route will reveal the stereo movement.

## Tempo sync

The top **TIME** button switches MS/Sync. Choose quarter, eighth or sixteenth notes,
each straight, dotted or triplet, with **NOTE**. Sync sets the interval relative to
each input note; it does not quantize attacks or force echoes to bar lines. In
Ping-pong the interval is between channel hops, not a full left/right round trip.

The header shows tempo source, BPM and the delay target. Targets transition using
the existing read-head crossfade, so the displayed new target is not a promise that
an ongoing fade has already finished. Source labels mean:

- **HOST:** valid VST3 tempo, accepted from 1–1000 BPM, even while stopped.
- **HELD:** host tempo/context went missing or invalid; keep the last valid BPM.
- **FALLBACK:** no valid host BPM has arrived yet; use FB, initially 120 BPM.
- **INTERNAL:** standalone always uses its editable INT tempo, initially 120 BPM.

The FB field does not override HOST or HELD. Tempo history is per instance and not
saved. FB is saved with the VST3 project; standalone still has no in-app preset or
preferences storage. A fresh instance consults the current host
instead of restoring a stale runtime clock. A host must supply processing callbacks
to deliver tempo changes; status may lag while it suspends processing entirely.

Free Time remains 20–2000 ms and is preserved while sync is on. The time knob's
`ms free` value edits only that saved free setting. Sync supports 20–8000 ms. Longer
or shorter combinations clamp at the boundary and show **SYNC LIMIT** explicitly;
choose another division or tempo rather than assume the selected note is exact.
A dotted quarter at 40 BPM, for example, is supported at 2250 ms without clamping.

Buffers for eight seconds are allocated before processing, not on a Sync or tempo
change. At 48 kHz the stereo double-precision delay buffers use about 6 MB per
Preview instance; Foundation and default core clients retain their old capacity.

Try 120 BPM, Sync, `1/4`: the target is 500 ms. `1/8 D` is 375 ms and `1/8 T` is
about 166.7 ms. Change Bitwig to 90 BPM: `1/4` becomes about 666.7 ms. Stop/start
transport and verify Sync does not turn into MS mode. Check both Normal/Ping-pong,
then save/reopen with a non-default division. Start at low monitoring volume.

## Save, restore and automate

Time, Feedback, Mix, Tone, Output gain, Bypass, Ping-pong, Sync, Division, INT/FB and
the three Reverb controls are host-automatable input
parameters, saved by DPF's VST3 adapter. The schema marker is
`undertow_preview_schema=1`. Host state never contains the audio delay buffers.
Their published IDs/order must not change. This experimental identity is `UtP0`,
URI `https://github.com/marrabld/stormsurge-fx/undertow-preview`, separate from `UtF0`.

The original six effect parameters keep DPF indices 0–5. Ping-pong is appended at
index 6 in the VST3 target. Standalone preserves monitor request/status at 6/7 and
appends Ping-pong at 8. UI/DSP use explicit logical-effect/DPF-index mapping helpers.
Old projects without a mode value start Normal; the optional appended parameter does
not require changing the existing schema marker or plugin identity. Old projects
without sync settings remain in MS mode. In version **0.3.0**, Sync/Division/INT-FB
append at VST3 DPF indices 7/8/9 or standalone 9/10/11. Four read-only tempo-source,
BPM, target-time and limit readouts append after those, and are not saved. Those
published slots must never move.
Version **0.4.0** appends Reverb Space/Darkness/Amount at logical/VST3 DPF indices
14/15/16, or standalone 16/17/18. They never take the place of readouts 10–13 or
monitor slots 6/7. New projects and old projects without reverb values start at
Amount zero. Identity `UtP0` and schema 1 stay unchanged. Future IDs append after
Reverb Amount. Read-only status does not form a contiguous final parameter block;
use metadata flags rather than a prefix-count assumption.

New instances, audio activation/reactivation and sample-rate changes start with an
empty tail. A buffer-size callback may cause host/DPF reactivation and clear the tail.
While processing remains active, transport moves alone do not clear it. This preview
does not claim sample-identical tail restoration or sample-accurate host automation.

The maintainer verified basic Preview controls, audible repeats, bypass spillover
and saved-setting reopening in Bitwig Studio Producer 6.1.3 on Omarchy for PR #10,
then Ping-pong alternation/Normal/bypass and mode reopening for PR #12. That is not a
numerical null test. The maintainer also reported Sync works as expected for PR #14,
without an exhaustive per-division/stop/export claim. Reverb is new and still needs
its own listening checks. Remaining manual checks:

- Listen to reverb in both delay modes, preserve dry attack and check Space/Darkness/Amount.
- Test effect-bypass spillover, zero-Amount old-delay comparison, and save/reopen nonzero Amount.
- Verify Sync follows Bitwig tempo and selected note values in both modes.
- Stop/start and change tempo while stopped; check source/target when processing resumes.
- Reopen an older MS project, then save/reopen with Sync and a non-default division.
- Automate Time, Feedback and Mix gently, then test faster changes at low volume.
- Try multiple instances and a short offline export with explicit trailing silence.

**Automatic host tail reporting is not implemented.** At the pinned DPF revision,
VST3 `getTailSamples()` returns zero. A stable feedback tail can be much longer than
one delay interval, especially with long reverb. Do not assume an export ending at the last note captures it;
extend the render region with silence, audition the result, and keep this limitation
open under issue #4. Host suspension behaviour also needs real-host testing.

## Standalone

The same core runs in `build/linux-debug/bin/UndertowPreview`, using external
JACK/PipeWire routing. It never opens a fallback native device. **It starts muted.**
The small monitor button below the effect footswitch, or Space in the standalone
window, requests monitoring. Its indicator follows the processor's status.

The effect footswitch never enables monitoring. Monitor mute immediately silences
dry and wet output and requests a tail clear. Audio reactivation requires fresh
consent. The monitor request reads back as zero so enabled consent is not serialized;
the separate status output reports the real state. Neither is present in the VST3.

Both targets are fixed stereo with Normal and Ping-pong modes. There is no built-in
mono-to-stereo mapping, device selector or metering yet. Route
mono guitar explicitly before testing live. Supported sample rates are 8–192 kHz;
unsupported rates fail muted. Do not replace a known-good recording setup with this
development build.
