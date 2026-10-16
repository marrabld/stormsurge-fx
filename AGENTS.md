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
| Iterating on the pedal GUI | **Authorised** |
| Merging a PR | **Not** authorised; open a PR and wait |
| Publishing a release or tagging | **Not** authorised |
| Installing a plugin anywhere | **Not** authorised |
| Closing issues | **Not** authorised without approval |

The repository is already **public**. It was created with the maintainer's approval.

### Current state

Planning PR #6 is merged into `main`. Foundation PR #7 was merged into its base
branch `docs/undertow-work-plan`, so **the foundation is not yet in `main`** — that
branch is currently ahead of `main` by the whole foundation commit series.

Open a follow-up PR from `docs/undertow-work-plan` into `main` before starting DSP
work, so delay work doesn't branch off an unmerged stack.

The foundation builds and passes automated tests on Linux and Windows. **There is no
delay processing yet.** The knobs on the mockup are explicitly labelled placeholders.

## Issue-per-task

Issues #1–#5 in this repo are the ordered work plan. Implement one at a time. Note
that #1 (sound and controls) and #2 (framework/build) are effectively complete; their
acceptance boxes still need the maintainer's sign-off rather than silent closure.

## Musical spec

The defaults in [docs/design.md](docs/design.md) were researched by an assistant and
**await the maintainer's approval**. Approve, amend or reject each one before
treating it as spec. Don't implement a decision they rejected, and don't let approved
defaults drift from the code without updating that file.

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

Two registered tests, both driving the real `src/Plugin.cpp` through DPF's headless
`PluginExporter`:

```sh
ctest --preset linux-debug -R plugin_passthrough        # VST3 mode
ctest --preset linux-debug -R standalone_monitor_gate   # JACK/Standalone mode
```

Non-obvious test facts:

- **`foundation_gui` is NOT a ctest test.** It builds with the project but is run by
  hand, because it needs a display/OpenGL context: `xvfb-run -a build/linux-debug/foundation_gui`.
  It renders 30 frames, asserts the 390x510 geometry constraint, opens no audio device.
  `--view` keeps the window open for inspection. `STORMSURGE_CAPTURE=/abs/path.ppm`
  writes a PPM — supported **only** in the GUI harness build, not in the plugin binary.
- `UndertowFoundation selftest` is DPF's own bundled self-test. It exists only on
  non-Windows, because `DPF_RUNTIME_TESTING` is compiled out for MSVC.
- The harness overrides global `operator new`/`delete` and asserts **zero allocations
  inside the audio callback**. Any allocation reachable from `run()` fails the test.
  When you add a delay, this test will fail if you allocate delay buffers in
  `prepareToPlay`/`activate` rather than in `init`.
- The harness fakes the format via a `getPluginFormatName()` definition of its own.
  `src/Plugin.cpp` and `src/Editor.cpp` both branch on that string to detect
  standalone. `PluginExporter` needs `d_nextBufferSize` / `d_nextSampleRate` set
  before construction.
- Don't `include "DistrhoPluginInfo.h"` anywhere. DPF's `DistrhoPluginChecks.h`
  already pulls it in.

## Live-monitoring safety invariants

The standalone monitor gate is the one safety-critical behaviour in the codebase.
`tests/FoundationTests.cpp` enforces all of these — keep them passing if you touch it:

- Standalone always starts muted, including on every `activate()` (device restart).
- A **new instance never inherits** consent from another instance.
- `setState()` must not restore monitoring; consent is not persisted.
- Invalid parameter values (e.g. NaN) must **fail safe to muted**.
- VST3 must never attenuate, clip or limit to hide instability. With feedback capped
  at 0.90 and a non-boosting filter the loop is stable, so the remaining peak risk is
  dry+wet summing, which the output level control addresses.
- Never remove `SKIP_NATIVE_AUDIO_FALLBACK`. Falling back to a native device would
  open the user's microphone without consent.

Also: bypassing the effect must never unmute the monitor, and a ringing delay tail is
the opposite of the monitor's fail-safe mute. Keep the two mechanisms separate.

## DSP boundaries

Keep effect processing host-independent: no host, window, device or filesystem
dependencies, so the plugin and standalone share one implementation.

Follow the constraints already in the spec, and expect tests for them:

- No allocation, blocking locks, file access or analysis in the audio callback.
- Prepare delay buffers in `init`/`prepareToPlay`, sized from the agreed maximum
  delay time.
- Sample-rate and buffer-size changes must be handled without unstable state.
- Time changes use a read/write crossfade, so no pitch shift; automation must not
  produce clicks.
- Smooth parameter changes. Test extremes and transitions, not only steady settings.
- Report tail length to the host as the maximum delay time.

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

## Licensing

Own code is **GPL-3.0-only**; every source file carries
`// SPDX-License-Identifier: GPL-3.0-only`.

- DPF was chosen over JUCE specifically because JUCE is AGPLv3/commercial. Do not
  reintroduce AGPL-or-weaker dependencies without raising it first.
- Adding any dependency means: an entry in `THIRD_PARTY_NOTICES.md` **and** a
  `configure_file(...)` copy into `bin/notices/` in `CMakeLists.txt`.
- `assets/concepts/` and `assets/ui/` are Gemini-generated and **gitignored**
  ([assets/README.md](assets/README.md) and [assets/SOURCES.md](assets/SOURCES.md)
  are tracked; the images are not). Redistribution rights are unconfirmed, so they
  must not be embedded or committed. The UI is drawn procedurally in code. The mockup
  also looks newer than intended; weathering is accepted later work.

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
- Bitwig 6.0.11 is the target host but has **not** been verified in the real DAW.
- Don't claim zero monitoring latency, and don't claim a platform is "verified"
  without pointing at actual CI output.
- pluginval 1.0.4 passed at strictness 5, but DPF emits teardown reference-count
  warnings in that validator. Investigate before calling it warning-free.
- Numerical tests are not listening evidence. Real-music playtesting decides whether
  Undertow sounds good; the maintainer does that, not the tests.