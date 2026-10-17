# Draft: Undertow Preview 0.4.0-preview.1

Proposed experimental prerelease. **Not published.** Publishing and tagging need
the maintainer's final approval after review. These notes do not declare Windows
or Mac real-DAW compatibility while those results are outstanding.

## What's included

- Dark stereo delay, Normal/Ping-pong routing and free/tempo-synced timings.
- Optional long dark reverb on delayed audio only, leaving the original guitar clear.
- The worn blue pedal interface, compact timing menus and realistic metal controls.
- Optimized Linux x86_64 VST3, Windows x86_64 VST3 and macOS universal VST3/AUv2 ZIPs.
- Installation notes, licences/notices, source-commit metadata, checksums and a
  matching corresponding-source archive including recursive DPF/Pugl sources.

Plugin identity `UtP0`, parameter IDs and saved-setting compatibility are unchanged.
The processor reports version 0.4.0; the download's preview suffix identifies this
experimental distribution. No Foundation or standalone binary is advertised in
these plugin-only packages. Nothing is installed automatically.

## Evidence and limits

The maintainer listened to earlier development builds in Bitwig Studio Producer
6.1.3 on Omarchy, including delay, Ping-pong, Sync, reverb, effect-bypass spillover
and saved settings. That is not a completed final-release listening/export check.

The candidate workflow checks Release builds, numerical/integration tests,
actual VST3 loading/factory/version metadata and archive contents. Mac additionally
checks actual AU rendering/state, universal CPU slices and ad-hoc signatures after
archive extraction. Loader/CI checks do not open audio devices and are not DAW
playback evidence or a substitute for pluginval/auval on the final package.

- **Linux:** Ubuntu 24.04 binary baseline; no promise of older-distribution loading.
- **Windows:** Release CRT required; real Windows DAW playback remains unverified.
- **Mac:** Dom's DAW test is pending. macOS 13 deployment target, native macOS 15 CI.
  Bundles are ad-hoc signed, not Developer ID signed or Apple-notarized. Security
  checks may block downloads; do not disable system security to force loading.
- Stereo-in/stereo-out only. No native mono-to-stereo plugin negotiation yet.
- **Leave explicit trailing silence in exports.** Automatic VST3 tail reporting
  is inaccurate, especially with long reverb. Listen through the exported tail.
- No limiter or automatic compensation. Start quietly, especially with feedback/output boost.

## Before publication

- [ ] Maintainer reviews the candidate PR, dependencies, archive/source checks and exact asset checksums.
- [ ] Maintainer tests the final Linux package in Bitwig: automation, multiple
  instances, settings restoration, bypass and an export with trailing silence.
- [ ] Record Dom's Mac result if available; otherwise retain the experimental/unverified label.
- [ ] Retain the Windows runtime/host-coverage caveat until a tester confirms it.
- [ ] Assemble all four matching archives, `SHA256SUMS` and `CANDIDATE.json` from one
  source commit after merging the preparation PR.
- [ ] Maintainer explicitly approves the final tag, assets and prerelease publication.

Proposed tag: `v0.4.0-preview.1`. This document and the CI workflow do not create it.
