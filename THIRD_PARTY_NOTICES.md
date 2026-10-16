# Third-party notices

StormSurge FX's own source is GPL-3.0-only. Dependency licences remain their own.
This foundation does not embed the supplied Gemini images or use JUCE.

## DPF and DGL

The DISTRHO Plugin Framework is pinned as the `external/dpf` Git submodule at
`4238e1c7f0351bbe488d79f0899c540543ac7583`.

Copyright (C) 2012-2025 Filipe Coelho <falktx@falktx.com>.
DPF is ISC-licensed. Its VST3 adapter uses DPF's ISC-licensed travesty interface,
not the Steinberg SDK. The JACK-only standalone dynamically loads the installed
JACK client library. Native RtAudio fallback and ASIO are not built here.

See upstream [LICENSE](external/dpf/LICENSE) and
[format licensing notes](external/dpf/LICENSING.md). Individual files include
additional contributor notices. Upstream sources are unmodified; the root CMake
file disables optional browser definitions affected by an upstream CMake boolean
handling issue at this revision.

## Graphics dependencies

- **Pugl:** the pinned nested submodule at
  `5e2621d714ddf1cb0f86e852f8ba5dffe04aa3a3`. Preserve its
  [COPYING](external/dpf/dgl/src/pugl-upstream/COPYING) and per-file notices.
- **NanoVG and Fontstash:** Copyright Mikko Mononen; zlib-style licences.
- **Fontstash UTF-8 decoder:** Copyright Bjoern Hoehrmann; its permissive notice
  is retained in `fontstash.h`.
- **stb_truetype:** Sean Barrett and contributors; MIT/public-domain alternatives.
- **stb_image:** the vendored version identifies itself as public domain.
- **DejaVu Sans:** embedded DGL font; Bitstream Vera/DejaVu terms, including
  attribution and font naming conditions. See the full font licence.

CMake copies the DPF, Pugl, NanoVG and font licence texts, and the intact helper
headers containing their own notices, into `build/<preset>/bin/notices/`. Keep
that directory, this notice and the project LICENSE with distributed binaries.
System OpenGL, X11 and the selected JACK implementation have their own terms;
they are not bundled by the current build.

## Source availability

For an exact source checkout, select the source commit recorded for the build and
run `git submodule update --init --recursive`. The Git submodule commits pin both
DPF and Pugl. A plain GitHub source zip does not include submodule contents.
Future public releases must identify their source commit, provide corresponding
source/dependency access and include required notices. Current CI artifacts are
development builds, not an approved release.
