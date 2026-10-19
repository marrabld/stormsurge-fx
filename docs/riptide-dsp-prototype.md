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

There is no audition renderer or plugin integration yet. Any private guitar recording
used for future listening should stay local unless the maintainer explicitly approves
sharing it.
