<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Riptide DSP core prototype

This is an experimental, host-independent pitch-down core for issue #58. It is not
connected to a plugin or audio device, and has not been approved as a musical result.
Its controls are pitch shift (0 to -12 semitones) and shifted-signal blend. It handles
planar mono or stereo audio.

## Current algorithm and tradeoff

The prototype uses two overlapping, interpolated delay read heads. Their delay sweeps
create the downward shift; a 40 ms sweep span is used to reduce weak low-note regions
seen with a shorter span. This is a simple granular method, not a high-quality
polyphonic pitch shifter. It can produce modulation, transient smearing, and
frequency-dependent dips. Passing synthetic single-note and power-chord tests does
not establish acceptable playing feel or sound quality.

At 48 kHz, the read heads look back from 48 samples (1 ms) up to 1,968 samples (41 ms).
At 44.1 kHz the range is 44 to 1,808 samples (about 1–41 ms). The read position moves
within that range; this is **not** a fixed plugin latency or a claim about perceived
latency. The substantial delay span is an important live-playing tradeoff and must be
judged by the maintainer before plugin integration.

## Checks

Run `ctest --preset linux-debug -R '^riptide_core$'` or the corresponding
`linux-sanitized` test. The headless test checks exact dry routing at zero shift and
zero blend, non-finite sample/parameter handling, mono/stereo and in-place processing,
no ordinary C++ allocations in selected processing calls, lifecycle reset, variable
block sizes, and spectral components for a -5 semitone note and a -12 semitone power
chord. Its spectral helper self-checks against a known amplitude. These are numerical
checks only; they are not listening tests, a complete artifact/latency assessment, or
evidence of DAW/device behavior.

## Listening to the prototype

`riptide_audition` renders a local WAV through the same core the tests use. It has no
plugin, editor, audio device or network access. Build and run it on Linux:

```sh
cmake --build --preset linux-debug --target riptide_audition
build/linux-debug/riptide_audition take.wav take-drop5.wav --shift -5
build/linux-debug/riptide_audition take.wav take-octave.wav --shift -12 --blend 30
```

Input may be mono or stereo PCM 16/24/32-bit or float32. Output is always float32 at
the input's rate and length. Shift is 0 to -12 semitones; blend is 0 to 100 percent of
the shifted signal. A 100 percent blend at -5 semitones is the whole-guitar drop tune;
a low blend at -12 is the blended lower layer. Write float32 so rendered peaks above
full scale are preserved rather than clipped.

The tool does not add a fixed latency, but the algorithm's moving read heads still
span roughly 1–41 ms as described above. Rendering a file successfully is not a
listening result: judge warble, attack smearing, chord handling and playing feel
against the private passage chosen for issue #39. Keep that recording local.

`ctest --preset linux-debug -R '^riptide_audition$'` checks file round-trips, malformed
input, dry identity and the expected -5 semitone frequency. Those are numerical checks,
not sound-quality evidence.

There is still no Riptide plugin or editor integration. Any private guitar recording
used for listening should stay local unless the maintainer explicitly approves
sharing it.
