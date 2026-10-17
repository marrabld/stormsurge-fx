# Third-party notices

StormSurge FX's own source is GPL-3.0-only. Dependency licences remain their own.
The project does not use JUCE. Preview embeds one prepared AI-generated enclosure
texture; Foundation still uses procedural artwork only.

## Undertow enclosure texture

The maintainer generated the source using Gemini Nano Banana in a personal account,
from an assistant-supplied text prompt without uploaded third-party reference images,
and approved its use in the plugin. The prepared derivative is supplied under the
project's GPL-3.0-only terms to the extent applicable rights exist. See
[`assets/runtime/NOTICE.md`](assets/runtime/NOTICE.md) for provenance, preparation,
terms links and limitations. That notice accompanies binaries in
`bin/notices/Undertow-artwork.md`. No new image-decoding dependency was added;
DPF's existing NanoVG/stb_image path decodes the embedded PNG.

## DPF and DGL

The DISTRHO Plugin Framework is pinned as the `external/dpf` Git submodule at
`4238e1c7f0351bbe488d79f0899c540543ac7583`.

Copyright (C) 2012-2025 Filipe Coelho <falktx@falktx.com>.
DPF is ISC-licensed. Its VST3 adapter uses DPF's ISC-licensed travesty interface,
not the Steinberg SDK. The JACK-only standalone dynamically loads the installed
JACK client library. Native RtAudio fallback and ASIO are not built here.

See upstream [LICENSE](external/dpf/LICENSE) and
[format licensing notes](external/dpf/LICENSING.md). Individual files include
additional contributor notices. The submodule checkout is unmodified; the root CMake
file disables optional browser definitions affected by an upstream CMake boolean
handling issue at this revision.

Preview VST3 compiles a generated copy of the adapter with a per-instance raw-tempo
bridge inserted before processing. DPF's original BBT conversion substitutes 120 BPM
when host tempo is missing, which cannot implement the approved hold/fallback policy.
The reproducible modification and wrapper are in
[`cmake/PreviewVst3Tempo.cmake`](cmake/PreviewVst3Tempo.cmake) and
[`cmake/PreviewVst3Main.cpp.in`](cmake/PreviewVst3Main.cpp.in). The module verifies the
LF-normalized upstream adapter and wrapper SHA256 hashes before generating anything. Original ISC notices are
retained in the copied source. Foundation and the dependency checkout are not patched.
Keep these build sources with the corresponding source distribution.

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

### Windows OpenGL header dependency

Windows builds additionally use the official `GL/glext.h` from Khronos
OpenGL-Registry commit `6af574a14089ccfee87efe230ebcdd8742859813` and
`KHR/khrplatform.h` from EGL-Registry commit
`db3425b8246136faccb5e2782b5694960bd6edf1`. CMake downloads them with pinned SHA256
verification into the build tree and copies them into the binary notices directory.
They supply declarations; the system provides the actual OpenGL implementation.

The extension header declares SPDX licence MIT, Copyright 2013-2026 The Khronos
Group Inc. The platform header retains its full Khronos MIT-style notice.
The MIT permission and disclaimer applicable to the extension header are:

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
> of the Software, and to permit persons to whom the Software is furnished to do
> so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

### Obtaining the source

For an exact source checkout, select the source commit recorded for the build and
run `git submodule update --init --recursive`. The Git submodule commits pin both
DPF and Pugl. A plain GitHub source zip does not include submodule contents.
Future public releases must identify their source commit, provide corresponding
source/dependency access and include required notices. Current CI artifacts are
development builds, not an approved release.
