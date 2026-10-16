# Roadmap

This is a direction, not a release schedule. Nothing below is implemented yet.
The first useful release is one delay, not an entire pedalboard.

## 0. Agree on Undertow

- Choose supported mono/stereo layouts and the first delay-time range.
- Decide how time changes sound: pitch glide, crossfade, or another explicit policy.
- Define mix law, feedback topology, tone behaviour and output headroom.
- Define bypass/tail behaviour, including what happens when bypass changes.
- Evaluate framework, SDK and dependency licences for a GPL-3.0-only release.
- Choose stable plugin identifiers and parameter IDs before distributing sessions.
- Agree what Linux/Bitwig and standalone setups the first release will support.

## 1. Understand the delay

- Prototype delay-line and feedback behaviour with generated signals and permitted
  guitar examples. An offline learning prototype is optional, not a prerequisite
  for the production architecture.
- Test repeat timing, decay, tone response and stereo/channel independence.
- Establish safe parameter ranges, startup/reset behaviour and numerical handling.
- Listen at low levels before making the delay playable through headphones.

## 2. Make Undertow playable on Linux

- Shared real-time DSP used by one plugin and one standalone target.
- Millisecond delay, feedback, mix, tone, output level and bypass.
- Smooth controls; no allocation, file access, blocking locks or analysis in the
  audio callback. Prepare buffers before playback.
- Handle sample-rate/block-size changes, zero-length callbacks where applicable,
  silence, transport changes and long tails without unstable state.
- Restore plugin settings in Bitwig projects and document tail reporting.
- Standalone device selection, explicit mono input mapping, meters and a clear
  response to unavailable devices. No unexpected microphone monitoring on launch.
- Simple readable controls before custom pedal graphics.

### First release acceptance

- Builds reproducibly from documented source dependencies.
- Scans and plays correctly in the tested Linux Bitwig version.
- Project close/reopen restores the effect's parameters, with an explicit policy
  for unsaved delay-line contents. Don't promise sample-identical tail recall.
- Correct results across block sizes and tested sample rates, including offline
  host export, multiple instances and automated parameter changes.
- Tested bypass/tail behaviour, no obvious clicks on normal adjustments, and no
  uncontrolled feedback at supported settings. Safe settings don't replace
  sensible monitoring volume or an explicit output/headroom policy.
- Standalone launch, selected-device playback and error handling tested on real
  hardware. No claim of zero monitoring latency.
- Licence/notices, build instructions, limitations and a small listening example.

## 3. Add musical timing and Windows

- Host-tempo sync with defined note divisions and behaviour when tempo is missing,
  stopped or changing. Standalone uses an explicit internal tempo.
- Windows build/test jobs and distributable plugin/standalone bundles.
- Validate real audio-device behaviour and installation on Windows; document
  backend requirements and any unsigned-build warnings.
- Test a Windows DAW rather than assuming cross-platform compilation proves use.
- Keep parameter/state compatibility with earlier Undertow projects.

The order within this milestone can change. Avoid Linux-only assumptions from
day one even if Windows binaries arrive later.

## 4. Earn the next pedal

Possible effects include tremolo, overdrive and reverb. Choose one at a time
based on listening and interest. Overdrive needs deliberate anti-aliasing work;
reverb needs its own design and listening tests. Neither is a trivial checkbox.

## 5. Assemble a small pedalboard

Reuse the effect modules in an initially fixed-order chain with individual bypass
and saved settings. Decide whether the board is an additional plugin/application
without breaking the identity of released individual effects.

Drag-and-drop ordering, parallel routes, third-party plugin hosting, elaborate
modulation, amp/cab modelling and additional plugin formats are separate future
decisions, not requirements for Undertow.
