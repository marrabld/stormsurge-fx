#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render original knob material, without baking a value marker into the light.

Uses only Python's standard library. Not required during normal CMake builds.
The local silver-knob concept is a visual reference, not an image input.
"""

import math
from pathlib import Path
import struct
import zlib


SIZE = 252
SAMPLES = 2
EXTENT = 84.0


def material(x, y):
    # A short cylinder projects below the face. The shadow is transparent so it
    # darkens the actual enclosure texture, rather than a baked background colour.
    shadow_distance = max(0.0, math.hypot(x - 2.5, y - 5.0) - 32.5)
    alpha = 0.48 * math.exp(-shadow_distance**2 / 16.0)
    colour = (3.0, 8.0, 11.0)

    side_radius = math.hypot(x, y - 2.8)
    side_angle = math.atan2(y - 2.8, x)
    ridge = (0.5 + 0.5 * math.cos(side_angle * 72)) ** 3
    if side_radius <= 33.6 + ridge * 0.35:
        light = -0.45 * math.cos(side_angle) - 0.65 * math.sin(side_angle)
        steel = 87 + 46 * light + 51 * ridge
        colour = (steel * 1.02, steel * 1.015, steel)
        alpha = 1.0

    face_y = y + 1.0
    radius = math.hypot(x, face_y)
    angle = math.atan2(face_y, x)
    if radius <= 32.0:
        light = -0.5 * math.cos(angle) - 0.7 * math.sin(angle)
        # Bevel changes slope at the face, catching a narrow machined rim light.
        steel = 151 + 72 * light
        if radius < 30.5:
            steel = 201 + 42 * light
        colour = (steel * 1.015, steel * 1.012, steel)
        alpha = 1.0

    if radius <= 29.5:
        # Concentric tool marks produce opposing broad reflections. Their light
        # stays fixed when the separate pointer is rotated by the live editor.
        reflection = abs(math.cos(angle + 2.25)) ** 7
        dark_lobe = abs(math.sin(angle + 2.25)) ** 4
        steel = 173 + 62 * reflection - 36 * dark_lobe
        steel += 12 * (-0.4 * x - 0.6 * face_y) / 29.5
        grain = 1.5 * math.sin(radius * 24) + 0.8 * math.sin(radius * 47)
        grain += 0.65 * math.sin(x * 93.7 + y * 71.3)
        steel += grain
        steel -= 9 * (radius / 29.5) ** 10
        colour = (steel * 1.02, steel * 1.015, steel)
        alpha = 1.0

    return (*colour, alpha)


def chunk(kind, data):
    payload = kind + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload))


def render():
    rows = bytearray()
    for py in range(SIZE):
        rows.append(0)
        for px in range(SIZE):
            total = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SAMPLES):
                for sx in range(SAMPLES):
                    x = ((px + (sx + 0.5) / SAMPLES) / SIZE - 0.5) * EXTENT
                    y = ((py + (sy + 0.5) / SAMPLES) / SIZE - 0.5) * EXTENT
                    *colour, alpha = material(x, y)
                    for channel in range(3):
                        total[channel] += colour[channel] * alpha
                    total[3] += alpha
            alpha = total[3] / SAMPLES**2
            for channel in range(3):
                value = total[channel] / total[3] if total[3] else 0
                rows.append(round(max(0, min(255, value))))
            rows.append(round(alpha * 255))
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    destination = Path(__file__).resolve().parents[1] / "assets/runtime/undertow-knob.png"
    destination.write_bytes(png)


if __name__ == "__main__":
    render()
