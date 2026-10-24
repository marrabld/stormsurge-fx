# Undertow Preview on Mac

This is an experimental colleague-testing build, not a published stable release.
It contains VST3 and Audio Unit v2 plugins for Apple Silicon and Intel Macs in
universal bundles, with a deployment target of macOS 13. The deployment target is
not evidence of testing on every supported OS version. No standalone app or audio
device backend is included in this first Mac preview.

## Before trying it

Please send the Mac chip/model, macOS version and DAW/version. Logic Pro and
GarageBand use the `.component` Audio Unit. Bitwig, REAPER and other VST3 hosts
use the `.vst3` bundle. Neither format has real-DAW playback approval merely
because CI builds or numerical tests pass.

The archive includes source-commit information in `BUILD.txt`, licences and
notices. Source is available at <https://github.com/marrabld/stormsurge-fx>.
Verify you received the intended build and keep those notices with it.

## Signing and installation

The bundles have ad-hoc signatures to preserve their integrity and permit local
development checks. They are **not Developer ID signed or Apple-notarized**.
These signatures do not establish the developer's identity. Gatekeeper or your
DAW may block a downloaded plugin. Do not disable Gatekeeper or system-wide
security settings. If blocked, stop and send the exact message so we can decide
how to provide a suitable signed build.

No installer is run. If you choose to install the tester plugin, quit your DAW
and copy the whole appropriate bundle, not its inner executable, into your
user plugin location:

- VST3: `~/Library/Audio/Plug-Ins/VST3/UndertowPreview.vst3`
- Audio Unit: `~/Library/Audio/Plug-Ins/Components/UndertowPreview.component`

Reopen the DAW and use its plugin rescan/validation process. Look for **Undertow
Preview**, not the old pass-through Foundation. Report validation/scan errors
rather than trying to force the DAW to accept a rejected plugin.

## Listening checklist

Use a disposable project and a recorded guitar track first. Start at low volume.
This build is stereo-in/stereo-out only. Use a stereo track or stereo bus/aux;
if the recording is mono, use the DAW's routing to feed that stereo path. A
native mono or mono-to-stereo plugin layout is not implemented yet.

1. Confirm scan/load and that the editor appears, resizes and has readable controls.
2. Confirm audible repeats, Normal/Ping-pong switching and clear dry guitar.
3. Enable Sync at 120 BPM. A straight quarter should show 500 ms. Change host tempo,
   stop/start and change tempo while stopped. The source should say HOST when the
   host supplies it, HELD if a previously valid clock disappears, or labelled
   FALLBACK before any valid clock. Do not mistake fallback editing for a host override.
4. Try the reverb, then effect bypass. Existing tails should fade while new guitar
   remains clean. The LED is effect state, not microphone monitoring consent.
5. Save with non-default Sync, Division and Reverb settings, close and reopen.
6. Try automation and a second instance. Record glitches or unexpected level changes.
7. Export a short passage with generous trailing silence and listen to the result.

VST3 tail reporting now estimates decay to -60 dB from the current delay and reverb
settings, capped at 10 minutes, but a host may not refresh it after parameter
changes. Do not assume export stopping at the last note captures the ambience; add
trailing silence and audition. AU tail reporting and Mac host suspension behaviour
still need real-host testing. This preview does not add a limiter or automatic
volume compensation. Stable feedback can still build up level on sustained material.

Please report format, architecture/OS/DAW, source commit, settings, steps and exact
error text. A successful scan is not yet evidence of reliable playback or exports.
