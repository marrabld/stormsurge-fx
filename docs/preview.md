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

| Control | Range | Meaning |
| --- | --- | --- |
| Time | 20–2000 ms | Delay between repeats; changes crossfade without a deliberate pitch swoop |
| Feedback | 0–90% | How much of each filtered repeat returns for another pass |
| Tone | 400–16000 Hz | Lowpass cutoff; lower settings make repeats darker |
| Mix | 0–100% wet | Linear blend; raising Mix also reduces dry level |
| Level | Silence to +6 dB | Output gain, also applied during effect bypass |
| Footswitch | Delay / bypass | Stops new notes entering the delay and preserves existing echoes |
| Mode button above LED | Normal / Ping-pong | Independent stereo or left-first alternating repeats |

Use the pedal footswitch or its **Bypass** parameter to test fading tails. Bitwig's
own device enable/hard-bypass button can stop processing and does not promise the
same spillover behaviour. Lower monitoring volume before raising feedback or Level.
There is no limiter, clipping or automatic level compensation.

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

## Save, restore and automate

Time, Feedback, Mix, Tone, Output gain, Bypass and Ping-pong are host-automatable input
parameters, saved by DPF's VST3 adapter. The schema marker is
`undertow_preview_schema=1`. Host state never contains the audio delay buffers.
Their published IDs/order must not change. This experimental identity is `UtP0`,
URI `https://github.com/marrabld/stormsurge-fx/undertow-preview`, separate from `UtF0`.

The original six effect parameters keep DPF indices 0–5. Ping-pong is appended at
index 6 in the VST3 target. Standalone preserves monitor request/status at 6/7 and
appends Ping-pong at 8. UI/DSP use explicit logical-effect/DPF-index mapping helpers.
Old projects without a mode value start Normal; the optional appended parameter does
not require changing the existing schema marker or plugin identity. The developer
version is now 0.2.0. Real-host compatibility still needs the manual reopen checks.

New instances, audio activation/reactivation and sample-rate changes start with an
empty tail. A buffer-size callback may cause host/DPF reactivation and clear the tail.
While processing remains active, transport moves alone do not clear it. This preview
does not claim sample-identical tail restoration or sample-accurate host automation.

The maintainer verified basic Preview controls, audible repeats, bypass spillover
and saved-setting reopening in Bitwig Studio Producer 6.1.3 on Omarchy for PR #10.
That is not a numerical null test or proof of the new mode. Remaining manual checks:

- Hear alternating new repeats in Ping-pong on the actual recorded-guitar track.
- Use a short phrase followed by silence to check bypass spillover in both modes.
- Reopen an older Normal project, then save/reopen with Ping-pong enabled.
- Automate Time, Feedback and Mix gently, then test faster changes at low volume.
- Try multiple instances and a short offline export with explicit trailing silence.

**Automatic host tail reporting is not implemented.** At the pinned DPF revision,
VST3 `getTailSamples()` returns zero. A stable feedback tail can be much longer than
one delay interval. Do not assume an export ending at the last note captures it;
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
mono-to-stereo mapping, tempo sync, device selector or metering yet. Route
mono guitar explicitly before testing live. Supported sample rates are 8–192 kHz;
unsupported rates fail muted. Do not replace a known-good recording setup with this
development build.
