# Building Undertow

The build includes both the unchanged pass-through **Foundation** and the audible
**Undertow Preview**. See [preview.md](preview.md) before listening to the latter.
Neither is a public release. Nothing is installed into your DAW automatically.

## Decisions

- C++20, CMake 3.24+, Ninja/Clang on Linux and Visual Studio 2022 x64 on Windows.
- DPF pinned by Git submodule, with DGL/NanoVG for controls and an embedded enclosure texture.
- VST3 and a JACK-only standalone. No native audio fallback or ASIO in this step.
- The experimental foundation has its own plugin identity, `UtF0`, distinct from
  the future actual Undertow effect. Do not rely on this shell for real sessions.

The maintainer approved DPF after the licence review. Current JUCE's open-source
terms are AGPLv3, not plain GPLv3, so the earlier JUCE proposal was not adopted.
See [technology decision](technology.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).

## Linux

Install CMake, Ninja, Clang, pkg-config, OpenGL and X11 development libraries,
including Xcursor, Xext and Xrandr. On Ubuntu 24.04 the CI setup uses:

```sh
sudo apt-get install clang cmake ninja-build libgl1-mesa-dev libx11-dev libxcursor-dev libxext-dev libxrandr-dev xvfb
```

On an existing Arch development system, the corresponding packages include
`clang`, `cmake`, `ninja`, `pkgconf`, `libglvnd`, `libx11`, `libxcursor`, `libxext`
and `libxrandr`. Read your package manager's proposed changes before installing.
No dependency-install command is run by CMake.

```sh
git clone --recurse-submodules https://github.com/marrabld/stormsurge-fx.git
cd stormsurge-fx
# Select the desired development branch/commit before configuring.
git submodule update --init --recursive
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Outputs:

```text
build/linux-debug/bin/UndertowFoundation.vst3/
build/linux-debug/bin/UndertowFoundation
build/linux-debug/bin/UndertowPreview.vst3/
build/linux-debug/bin/UndertowPreview
build/linux-debug/bin/notices/
```

`linux-release` builds optimized binaries. `linux-sanitized` enables address and
undefined-behaviour sanitizers; start with its `foundation_tests` target.
Ubuntu 24.04 is the initial CI build baseline, not a claim of universal Linux
binary compatibility. A locally built Arch binary may need newer system libraries.
The current Linux GUI uses X11, including XWayland on Wayland desktops.

## Windows

Install Visual Studio 2022 with Desktop development with C++, its Windows SDK,
CMake 3.24+ and Git. In a developer terminal after recursive checkout:

The first Windows configure downloads two official Khronos OpenGL headers to the
build directory. Their exact commits and SHA256 hashes are pinned in
`cmake/KhronosHeaders.cmake`; matching cached files need no download. No executable
is downloaded or installed. Keep the headers and notices with corresponding source.

```sh
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The VST3 bundle and standalone `.exe` are under `build/windows-debug/bin/`.
These are developer/debug artifacts, not portable release packages. Windows
compilation in CI does not establish playback or device-driver compatibility.

## Try the plugin in Bitwig

The maintainer checked Foundation in Bitwig Studio Producer **6.1.3** on Omarchy.
The new audible preview needs its own real-host acceptance checks in [preview.md](preview.md).
Add `build/linux-debug/bin` to Bitwig's VST plugin locations and rescan, or
manually copy only the `UndertowFoundation.vst3` bundle into a user plugin folder.
Do not overwrite an existing installation without checking it first.

Load **Undertow Foundation** on a quiet test track. It has two input and two output
channels; no mono-to-stereo processing is implemented. Audio should remain
unchanged, including levels above full scale inside a float host. There is no
limiter or safety attenuation. The knobs don't change audio. For this foundation
there are no effect parameters to save, only a fixed foundation schema marker,
and no delay tail/added
processing latency is requested. Host validation and project reopening still need
testing on the actual DAW. Remove the development plugin from real song sessions.

## Standalone limitations and safety

The first standalone requires an already running JACK-compatible server. On
PipeWire systems, launch using `pw-jack build/linux-debug/bin/UndertowFoundation`.
On Windows it requires a compatible installed JACK client/runtime and server;
there is no WASAPI or ASIO device selector yet. Missing JACK fails rather than
falling back to your microphone or another device.

The app registers stereo ports; route them explicitly using your JACK/PipeWire
patchbay. External session managers may remember connections. **The monitor gate
starts muted regardless**, and must be enabled with the footswitch or Space.
It resets to muted on audio reactivation. An output-status parameter keeps the
indicator in sync. Start quietly, check routing, avoid speaker feedback and avoid
doubling direct monitoring. No audio-hardware test is run automatically.

The existing server owns sample rate, buffer size and device choice. In-app device
selection and finished error dialogs are later work under issue #4. This limited
JACK shell is a foundation, not completion of the standalone workflow.

## Automated checks

The independent stereo core and actual Preview processor are tested separately from
Foundation, which remains pass-through. No audio device is opened by these checks:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target dark_delay_tests foundation_tests preview_tests tempo_sync_tests dark_reverb_tests
ctest --preset linux-debug
# Focus only on the delay core:
ctest --preset linux-debug -R '^dark_delay_core$'
ctest --preset linux-debug -R '^preview_'
ctest --preset linux-debug -R '^tempo_sync$'
ctest --preset linux-debug -R '^dark_reverb_core$'
# Match the sanitizer CI job:
cmake --preset linux-sanitized
cmake --build --preset linux-sanitized --target foundation_tests dark_delay_tests preview_tests tempo_sync_tests dark_reverb_tests
ctest --preset linux-sanitized
```

`dark_delay_core` checks timing/decay/darkening, fractional reads, channel separation,
mix/gain, separate and in-place buffers, zero frames, block partitioning, sample-timed
automation, bypass tails, invalid values, reset/reconfiguration and selected C++
allocation paths. These are not listening tests or host integration tests. See
[approved core behaviour and remaining proposals](design.md).

`preview_plugin` and `preview_monitor_gate` compare the actual Preview processor
against the core through DPF's exporter. They test parameter IDs/ranges, reconstructed
instance settings, bypass/automation, muted tails, reactivation/instance isolation,
supported rates, in-place buffers, zero frames and selected allocation paths. The
reconstructed-instance check is not a real VST3 state-stream or Bitwig reopening test.

The core checks also cover left-first alternating ping-pong timing/decay/darkening,
wet input averaging, dry independence and rapid mode/time changes. Preview tests
exercise both modes through the exporter and preserve the original six effect IDs
and standalone monitor IDs. The GUI smoke exercises mode toggles and host feedback.

`tempo_sync` tests the nine note values, independent tempo histories, limits and
eight-second buffers at five rates. Preview tests exercise the raw context extraction
template through the actual exporter, including missing/invalid tempo, stopped
transport, source/limit readouts, note timing in both modes, rapid changes and monitor
isolation. This is not a loaded-VST3 adapter or real Bitwig test.

`dark_reverb_core` checks causal stereo diffusion, relative decay/darkness response,
headroom/transition cases, reset/rate changes, invalid values and ordinary C++
allocation paths. It also tests that the wet insert cannot alter delay feedback.
Preview tests add byte-identical zero-Amount compatibility, an independently prepared
delay/reverb reference chain, dry-only identity, causal wet routing, bypass spillover,
no new reverb excitation during bypass, saved settings and monitor clearing of both
stages in free/sync and Normal/Ping-pong. These do not prove pleasing sound or real
device usability. Preview GUI geometry is 390×675 minimum, not Foundation's 390×510.
`preview_gui --small` runs the control-event harness at that minimum size. Timing
tests cover the Sync switch, all nine Note/Feel combinations, host division updates,
popup dismissal without click-through, coarse/fine tempo scroll and tempo drag
with balanced host edit gestures. Embedded texture decoding is checked too. Use
`preview_gui --standalone` to check monitor UI separation without opening audio.
For capture-only review, `STORMSURGE_CAPTURE_POPUP=note`, `feel` or `tempo` leaves
that popup open after the harness interactions. It has no effect in the plugin.
CMake embeds the tracked PNG using only CMake itself. ImageMagick is needed only
to regenerate that derivative from the maintainer's local original, not to build.

Preview VST3 uses a generated wrapper under `build/<preset>/preview-adapter/` to
preserve raw host tempo validity instead of DPF's synthetic 120 BPM. The checksum-
guarded transformation in `cmake/PreviewVst3Tempo.cmake` must be reviewed on DPF
updates. It edits neither the dependency checkout nor Foundation. Keep the module
and `.cpp.in` wrapper in source distributions; see `THIRD_PARTY_NOTICES.md`.

`ctest` exercises the actual DPF processor through a headless exporter harness
at five rates and eight block sizes, including zero frames, separate/in-place
buffers, stereo independence, unclipped over-range samples and the standalone
monitor gate/restart. It checks C++ allocations during selected processing calls.
This is not proof of every real-time property or an actual VST3-host validation.

On Linux, DPF's additional GUI/processor smoke test needs a display but never opens audio:

```sh
build/linux-debug/bin/UndertowFoundation selftest
# Headless Linux CI:
xvfb-run -a build/linux-debug/bin/UndertowFoundation selftest
xvfb-run -a build/linux-debug/bin/UndertowPreview selftest
xvfb-run -a build/linux-debug/preview_gui
xvfb-run -a build/linux-debug/preview_gui --standalone
```

The bundled DPF `selftest` option is not enabled for MSVC because its thread-test
helper requires POSIX APIs. The project's own headless `ctest` checks still run
on Windows. GUI/device behaviour there remains a separate runtime check.

`build/linux-debug/foundation_gui` opens the actual pedal editor briefly without
any audio device or JACK server. Set `STORMSURGE_CAPTURE=/absolute/path/face.ppm`
to capture only that editor using DGL's picture export. This environment variable
is supported only by the GUI test target, not by the plugin/standalone binaries.
The PPM can be converted to PNG with an image editor for documentation.

To leave the mockup open for inspection without connecting any audio device:

```sh
build/linux-debug/foundation_gui --view
build/linux-debug/preview_gui --view
```

Close the window to exit. On Windows, the debug GUI helper is
`build/windows-debug/Debug/foundation_gui.exe`. This preview does not process or
monitor audio; it displays the same editor used by the plugin.
The Preview helper also exercises real drag/scroll/bypass events and balanced host
edit gestures when run without `--view`. `--standalone` tests the standalone UI's
Space-monitor action, still without opening an audio device. The preview helper's
Windows path is `build/windows-debug/Debug/preview_gui.exe`.

Optional independent VST3 validation uses the separately downloaded pluginval tool:

```sh
pluginval --strictness-level 5 --random-seed 42 --validate build/linux-debug/bin/UndertowFoundation.vst3
```

The first local pluginval 1.0.4 run passed editor/processing/state tests at level 5.
DPF emits teardown warnings about component/connection-point reference counts in
that validator, so this is not a warning-free compatibility claim. Investigate
those diagnostics before a release and retain real Bitwig testing as a separate
acceptance item. The Steinberg validator was not run. pluginval is an external
test tool, not linked into or distributed with StormSurge.

The checked-in workflow builds Linux and Windows and uploads temporary development
artifacts. It never creates a public release, installs a plugin, changes audio
devices or merges a PR. See its results before claiming a platform build passed.
