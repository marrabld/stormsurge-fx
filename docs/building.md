# Building the Undertow foundation

This is a pass-through development shell, not a delay release. Its knobs are
visual placeholders. Nothing is installed into your DAW or system automatically.

## Decisions

- C++20, CMake 3.24+, Ninja/Clang on Linux and Visual Studio 2022 x64 on Windows.
- DPF pinned by Git submodule, with DGL/NanoVG for a procedural pedal face.
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

Bitwig **6.0.11** is the maintainer's target, not yet a verified host result.
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

`ctest` exercises the actual DPF processor through a headless exporter harness
at five rates and eight block sizes, including zero frames, separate/in-place
buffers, stereo independence, unclipped over-range samples and the standalone
monitor gate/restart. It checks C++ allocations during selected processing calls.
This is not proof of every real-time property or an actual VST3-host validation.

DPF's additional GUI/processor smoke test needs a display but never opens audio:

```sh
build/linux-debug/bin/UndertowFoundation selftest
# Headless Linux CI:
xvfb-run -a build/linux-debug/bin/UndertowFoundation selftest
```

`build/linux-debug/foundation_gui` opens the actual pedal editor briefly without
any audio device or JACK server. Set `STORMSURGE_CAPTURE=/absolute/path/face.ppm`
to capture only that editor using DGL's picture export. This environment variable
is supported only by the GUI test target, not by the plugin/standalone binaries.
The PPM can be converted to PNG with an image editor for documentation.

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
