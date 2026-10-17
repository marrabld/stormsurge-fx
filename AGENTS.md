# AGENTS.md

Instructions for AI coding assistants working in this repository. Read this before
touching anything. See also [HANDOFF.md](HANDOFF.md) for project context and
[docs/design.md](docs/design.md) for the musical spec.

## The maintainer is not a software developer

Explain tradeoffs in plain language before asking for decisions. Don't ask them to
write technical specifications. Musical and safety choices need their reasoning
stated so they can be judged by ear later.

## Scope

StormSurge FX is a GPL-3.0-only guitar effects suite for Bitwig and standalone use.
Undertow, a dark stereo delay, is the first effect; more follow. Linux first,
Windows planned, no macOS commitment.

### What is authorised

| Action | State |
| --- | --- |
| Implementing Undertow's delay DSP | **Authorised** |
| Optional wet-path dark reverb, issue #16 | **Authorised** |
| Iterating on the pedal GUI | **Authorised** |
| Merging a PR | **Not** authorised; open a PR and wait |
| Publishing a release or tagging | **Not** authorised |
| Installing a plugin anywhere | **Not** authorised |
| Closing issues | **Not** authorised without approval |

The repository is already **public**. It was created with the maintainer's approval.

### Current state

PR #8 brought the foundation and handoff into `main`. Do not repeat that merge.
PR #9 added the tested independent-stereo core. `PreviewPlugin.cpp` and
`PreviewEditor.cpp` connect it as **Undertow Preview**, with a separate `UtP0` identity
and build target. Keep `UndertowFoundation`/`UtF0` pass-through for old test projects.

Preview's parameter order/symbols are in `PreviewParameters.hpp`; append, do not
reorder. Issue #11 adds switchable Ping-pong; #13 adds tempo sync; #16 adds optional
wet reverb. Default Amount zero preserves existing delay sessions. Final host/device
policies and tail reporting remain issue #4 work.

## Issue-per-task

- Record every feature in a GitHub issue before implementing it. Check for an
  existing issue first; create one if none covers the feature. Record agreed
  behaviour and acceptance checks in the issue.
- Give each feature its own `feat/<issue-number>-<short-name>` branch, based on
  up-to-date `main`. Do not implement features directly on `main` or combine
  unrelated features in one branch.
- Open a PR into `main` for each feature, referencing its issue and documenting
  verification results. Use `Refs #<number>` rather than automatic issue-closing
  keywords unless the maintainer approves closing the issue.
- Wait for the maintainer's review and explicit approval before merging. Do not
  enable auto-merge. Start the next feature from updated `main` after the previous
  feature merges, rather than stacking feature branches.

Issues #1–#5 in this repo are the ordered work plan. Implement one at a time. Note
that #1 (sound and controls) and #2 (framework/build) are effectively complete; their
acceptance boxes still need the maintainer's sign-off rather than silent closure.

## Musical spec

[docs/design.md](docs/design.md) records approved core and sync/hold/fallback choices
separately from remaining proposals.
Do not treat remaining host/device proposals as approved. Keep spec and code consistent.

## Setup and build

Submodules are mandatory and must be **recursive** (Pugl is nested inside DPF).
CMake hard-fails on a missing `external/dpf/dgl/src/pugl-upstream/include/pugl/pugl.h`:

```sh
git submodule update --init --recursive
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

- Always configure through a **preset**. There is no plain `cmake ..` path, and each
  preset has its own `build/<preset>/` tree. Presets: `linux-debug`, `linux-release`,
  `linux-sanitized`, `windows-debug`.
- Requires CMake 3.24+, Ninja and Clang on Linux. **No dependency install is run by
  CMake** — X11/Xcursor/Xext/Xrandr and `libgl1-mesa-dev` must already be present.
- `linux-sanitized` enables ASan/UBSan; build its `foundation_tests` target first.
- Windows configure **downloads** two hash-pinned Khronos headers via
  `cmake/KhronosHeaders.cmake`. That module needs network access at configure time.

## Tests

The two foundation tests drive `src/Plugin.cpp` through DPF's headless exporter.
The separate `dark_delay_core` test has no DPF, UI or device dependency:

```sh
ctest --preset linux-debug -R plugin_passthrough        # VST3 mode
ctest --preset linux-debug -R standalone_monitor_gate   # JACK/Standalone mode
ctest --preset linux-debug -R '^dark_delay_core$'        # independent delay core
ctest --preset linux-debug -R '^preview_'                # Preview integration and monitor
ctest --preset linux-debug -R '^tempo_sync$'             # timing math and extended buffers
ctest --preset linux-debug -R '^dark_reverb_core$'       # reverb response and wet-insert contract
```

Non-obvious test facts:

- **`foundation_gui` is NOT a ctest test.** It builds with the project but is run by
  hand, because it needs a display/OpenGL context: `xvfb-run -a build/linux-debug/foundation_gui`.
  It renders 30 frames, asserts the 390x510 geometry constraint, opens no audio device.
  `--view` keeps the window open for inspection. `STORMSURGE_CAPTURE=/abs/path.ppm`
  writes a PPM — supported **only** in the GUI harness build, not in the plugin binary.
- `UndertowFoundation selftest` is DPF's own bundled self-test. It exists only on
  non-Windows, because `DPF_RUNTIME_TESTING` is compiled out for MSVC.
- The harness tracks ordinary C++ `new`/`delete` allocations during selected processing
  calls, not every allocator or every lifecycle method. Allocate delay buffers in
  `prepare()` off the audio callback. `dark_delay_core` also checks parameter updates
  and reset for these allocations; it is not proof of all real-time properties.
- The harness fakes the format via a `getPluginFormatName()` definition of its own.
  `src/Plugin.cpp` and `src/Editor.cpp` both branch on that string to detect
  standalone. `PluginExporter` needs `d_nextBufferSize` / `d_nextSampleRate` set
  before construction.
- Don't `include "DistrhoPluginInfo.h"` anywhere. DPF's `DistrhoPluginChecks.h`
  already pulls it in.
- `STORMSURGE_PREVIEW` selects Preview metadata for all its adapter, DSP, UI and
  harness targets. Do not let it leak into Foundation targets.
- Use `previewHostIndex`/`previewEffectIndex` for effect/UI parameters. Ping-pong's
  logical ID 6 maps to DPF index 6 in VST3, but 8 in standalone; the latter's legacy
  monitor slots 6/7 must not move. Monitor constants are raw standalone slots only.
- Sync, Division and INT/FB append at logical IDs 7–9. Read-only timing statuses are
  10–13; Reverb Space/Darkness/Amount are 14–16. Future parameters append after the
  current final ID. Inputs are no longer a contiguous prefix; do not count them that way.
  `previewSpecs.readOnly` controls write access; readouts are not saved parameters.
  Use `previewLogicalIndex` to receive readouts; `previewEffectIndex` excludes them.
- `preview_gui` exercises real control events/host edit gestures; `--standalone`
  tests the standalone UI without audio. Like `foundation_gui`, it needs a display.
  Preview now uses 520×900 coordinates, minimum 390×675. Share its geometry constants
  for drawing/hit testing. Foundation keeps its original 520×680/minimum 390×510.

## Live-monitoring safety invariants

The standalone monitor gate is the one safety-critical behaviour in the codebase.
`tests/FoundationTests.cpp` enforces all of these — keep them passing if you touch it:

- Standalone always starts muted, including on every `activate()` (device restart).
- A **new instance never inherits** consent from another instance.
- `setState()` must not restore monitoring; consent is not persisted.
- Invalid parameter values (e.g. NaN) must **fail safe to muted**.
- Do not silently attenuate, clip or limit to hide instability. Stable feedback can
  still accumulate substantial level from sustained input before dry/wet summing.
- Never remove `SKIP_NATIVE_AUDIO_FALLBACK`. Falling back to a native device would
  open the user's microphone without consent.

Also: bypassing the effect must never unmute the monitor, and a ringing delay tail is
the opposite of the monitor's fail-safe mute. Keep the two mechanisms separate.

## DSP boundaries

Keep effect processing host-independent: no host, window, device or filesystem
dependencies, so the plugin and standalone share one implementation.

Follow the constraints already in the spec, and expect tests for them:

- No allocation, blocking locks, file access or analysis in the audio callback.
- Allocate delay buffers off the audio callback, sized from the agreed maximum time.
- Sample-rate and buffer-size changes must be handled without unstable state.
- Time changes crossfade stationary read heads. Test rapid retargeting as well as
  isolated changes; crossfades can still produce audible interference.
- Smooth parameter changes. Test extremes and transitions, not only steady settings.
- Host tail reporting must account for feedback/filter decay, not a single delay
  interval. Pinned DPF's VST3 adapter hardcodes zero tail samples. This preview has
  no adapter fix; document trailing-silence export tests rather than promise tails.
- Preview monitor requests read back as zero so consent cannot be saved. Poll the
  separate status output. Its footswitch is effect bypass, not monitor consent.
- Ping-pong averages only wet input into the left line; dry routing stays unchanged.
  Read/filter both returns before writing cross-feedback. Mode changes preserve tails
  and smooth a non-boosting routing matrix; old tails need not immediately alternate.
- Free time stays 20–2000 ms. Preview prepares an eight-second core capacity for sync
  off the callback; never resize on mode/note/BPM changes. Default core capacity
  remains two seconds for other callers.
- Tempo sync follows valid BPM even while stopped. Missing clock holds last valid
  BPM; initial fallback and standalone internal tempo default to 120, explicitly
  labelled. Do not silently substitute free milliseconds or hide sync range limits.
- `DarkDelay::WetInsert` transforms only the wet return, outside delay feedback and
  before Mix/Level. Keep it DSP-only. The reverb owner must reset both stages on
  activation, rate changes and monitor mute. Amount zero is tested byte-identical
  to the original delay, not an approximate bypass. Never apply reverb to dry input.

## Platform-specific CMake workarounds

These exist because of the pinned DPF revision and must be **re-checked whenever the
submodule is bumped** ([docs/technology.md](docs/technology.md) says so explicitly):

- The `foreach` loop stripping `DGL_USE_FILE_BROWSER` / `DGL_USE_WEB_VIEW` works
  around an upstream generator-expression boolean bug. Do not drop it.
- `DPF_RUNTIME_TESTING` is excluded on MSVC because DPF's bundled self-test helper
  needs POSIX threads.
- `STORMSURGE_SANITIZERS` is a hard `FATAL_ERROR` under MSVC by design.
- Windows fetches pinned Khronos OpenGL extension headers because the Windows SDK
  alone lacks the declarations NanoVG needs.
- Preview VST3 replaces only its build wrapper using `cmake/PreviewVst3Tempo.cmake`.
  The generated adapter preserves raw host tempo validity before DPF can synthesize
  120 BPM. Its LF-normalized source hash is checked; review on DPF updates. Never
  hand-edit generated files or the submodule. Foundation is not patched. Use the raw
  bridge, not `getTimePosition().bbt.valid`, for Preview tempo availability.

## Licensing

Own code is **GPL-3.0-only**; every source file carries
`// SPDX-License-Identifier: GPL-3.0-only`.

- DPF was chosen over JUCE specifically because JUCE is AGPLv3/commercial. Do not
  reintroduce AGPL-or-weaker dependencies without raising it first.
- Adding any dependency means: an entry in `THIRD_PARTY_NOTICES.md` **and** a
  `configure_file(...)` copy into `bin/notices/` in `CMakeLists.txt`.
- `assets/concepts/` and `assets/ui/` originals are Gemini-generated and **gitignored**.
  The maintainer confirmed personal Gemini Nano Banana, an assistant-supplied prompt
  and no uploaded third-party references, and approved enclosure reuse for #15.
  Only the prepared `assets/runtime/undertow-enclosure.png` is bundled, with its
  provenance/licence notice copied to `bin/notices/`. Keep originals unchanged.
  See `assets/SOURCES.md`; do not infer blanket clearance of other local artwork.
  Labels and controls stay separate from the texture. CMake embeds the PNG into
  a generated header; no user-local file or ImageMagick is needed for normal builds.

## Style

`.clang-format` (LLVM base, 4-space, 100 columns, Linux braces, **`SortIncludes: Never`**).
**CI does not run clang-format** — check it manually before committing:

```sh
clang-format --dry-run -Werror src/*.cpp src/*.hpp tests/*.cpp
```

Comments in this codebase explain *why* a constraint exists, and name the case they
guard against. Match that; do not add restating-the-code comments.

## Claims discipline

The docs draw hard lines between "compiles" and "works". Preserve that.

- A successful build is **not** evidence of playback, host compatibility or device
  behaviour. No CI job opens an audio device.
- The maintainer tested Foundation scan/load, clean recorded playback and project
  reopening in Bitwig Studio Producer 6.1.3 on Omarchy, then basic Preview knobs,
  audible echoes, effect-bypass spillover and saved-setting reopening for PR #10.
  The maintainer also confirmed Ping-pong/Normal/bypass and mode reopening for PR #12.
  The maintainer reported Sync works as expected for PR #14, not an exhaustive
  all-division/stop/export result. Reverb still needs its own listening checks.
- Don't claim zero monitoring latency, and don't claim a platform is "verified"
  without pointing at actual CI output.
- pluginval 1.0.4 passed at strictness 5, but DPF emits teardown reference-count
  warnings in that validator. Investigate before calling it warning-free.
- Numerical tests are not listening evidence. Real-music playtesting decides whether
  Undertow sounds good; the maintainer does that, not the tests.
