# Undertow Preview installation and first use

This is an **experimental preview**, not a supported production release. Use a
disposable project, keep an untouched copy of important sessions, and start at low
volume. No installer runs and no microphone/audio device is opened by unpacking
these files. The download contains plugins only, not the unfinished standalone app.

## Choose a download

| Platform | Contents | Requirements and evidence |
| --- | --- | --- |
| Linux x86_64 | VST3 | Ubuntu 24.04 build baseline; earlier development builds listened to in Bitwig 6.1.3 on Omarchy |
| Windows x86_64 | VST3 | Intended for Windows 10/11 x64; Windows Server 2022 CI builds/tests, no real Windows DAW playback evidence yet |
| macOS universal | VST3 and AUv2 | Apple Silicon/Intel, macOS 13 deployment target; native macOS 15 CI checks; Dom's real-DAW testing pending |

Keep `SHA256SUMS`, `CANDIDATE.json` and the matching source archive with the
downloads. `BUILD.json` inside each plugin ZIP identifies its source commit and
matching source archive. The source includes recursive DPF/Pugl dependencies and
build instructions. Licences and notices accompany every binary ZIP.

## Linux

Quit your DAW. Copy the complete `UndertowPreview.vst3` directory into
`~/.vst3/`, or a user VST3 location configured in your DAW. Do not copy just the
inner `.so`, and do not overwrite another version without keeping a backup.

The candidate is built on Ubuntu 24.04, not an older universal Linux baseline.
It needs a compatible glibc/libstdc++ plus X11/XWayland and desktop OpenGL runtime
libraries. It does not bundle/replace your system libraries. DPF may also link
the system D-Bus library when it detects it. The
direct dependencies and required symbol versions are recorded in `CANDIDATE.json`.
Older distributions may not load it. A newer Arch-built binary is not substituted
for the documented Ubuntu candidate. A VST3 host does not need JACK just to use it.

## Windows

Use a 64-bit x86_64 DAW. This is a Release build, not one of the older debug CI
downloads. It requires Microsoft's current x64 Visual C++ v14 redistributable.
If absent, obtain it from the [official Microsoft page](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
Nothing here installs that prerequisite or ships debug CRT DLLs.

Quit your DAW. Copy the complete `UndertowPreview.vst3` directory to its configured
VST3 plugin location. The conventional shared location is
`C:\Program Files\Common Files\VST3\`, which may need administrator approval.
Use a user/custom location if your DAW supports it; do not elevate privileges just
to work around an unexplained loading error. Rescan plugins and report failures.
This candidate is not a Windows code-signed installer.

## macOS

Both CPU architectures are in the same bundles. Use VST3 in a VST3-compatible DAW;
Logic Pro and GarageBand use the `.component` Audio Unit. Quit the DAW before
copying the whole appropriate bundle to the user plugin location:

- `~/Library/Audio/Plug-Ins/VST3/UndertowPreview.vst3`
- `~/Library/Audio/Plug-Ins/Components/UndertowPreview.component`

These are **ad-hoc signed, not Developer ID signed or Apple-notarized**. The
signature does not establish developer identity. Gatekeeper or the host may block
a downloaded plugin. Do not disable system security. Stop and send the exact
warning if blocked. `MAC-TESTING.md` in the Mac ZIP has Dom's test checklist.

## First use in any DAW

1. Rescan and add **Undertow Preview** to a stereo test track or stereo bus. It is
   not the old Foundation pass-through plugin. Mono and mono-to-stereo plugin
   layouts are not implemented; route mono guitar through a stereo path in the DAW.
2. The initial sound is 400 ms delay, 35% feedback and 20% Mix. Reverb Amount starts
   at zero. Try a short recorded phrase followed by silence.
3. Turn up Reverb Amount to add the wash. Effect bypass stops new wet input but
   lets existing tails fade. The LED indicates effect state, not monitoring consent.
4. Save/reopen a disposable project and check your settings. Audio buffers/tails
   are not saved. Device/sample-rate reactivation clears them.
5. When exporting, **extend the render region with explicit silence** and listen
   through the complete tail. VST3 automatic tail reporting is inaccurate; do not
   rely on a host's automatic tail option. AU export/suspension also needs testing.

There is no hidden limiter or automatic level compensation. Feedback and output
boost can produce substantial level. See `CONTROLS-AND-LIMITATIONS.md` for controls,
tempo fallback, routing and further checks. Windows and Mac/other-DAW runtime
compatibility must not be inferred from a successful build or loader test.

Report OS/chip, DAW/version, `BUILD.json` commit, settings, exact error text and
steps at <https://github.com/marrabld/stormsurge-fx/issues>.
