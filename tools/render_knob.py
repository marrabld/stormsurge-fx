#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render original worn knob and footswitch materials with fixed lighting.

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


def noise(x, y):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

    def corner(a, b):
        value = ((a * 374761393 + b * 668265263) ^ 0x554E4454) & 0xFFFFFFFF
        value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
        return (value ^ (value >> 16)) / 0xFFFFFFFF

    top = corner(ix, iy) * (1 - fx) + corner(ix + 1, iy) * fx
    bottom = corner(ix, iy + 1) * (1 - fx) + corner(ix + 1, iy + 1) * fx
    return top * (1 - fy) + bottom * fy


def weather(colour, x, y, radius, crevice=0.0):
    # Broad residue and darker seams survive downsampling. Fine grit alone
    # disappears at normal pedal size, leaving hardware looking factory-new.
    residue = noise(x * 0.16, y * 0.16)
    grit = noise(x * 1.7, y * 1.7)
    edge = max(0.0, min(1.0, (radius - 21) / 10))
    dirt = 0.045 + 0.20 * residue**2 + edge * residue * 0.23 + crevice * 0.24
    dirt += max(0, grit - 0.72) * 0.45
    # Warm grey deposits, not uniform brown tint over every reflection.
    return tuple(c * (1 - dirt) + deposit * dirt for c, deposit in zip(colour, (46, 40, 29)))


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

    if alpha == 1.0:
        colour = weather(colour, x, y, radius, (1 - ridge) * 0.5 if radius > 31 else 0)
        if radius < 29:
            # A few shallow handling scuffs keep the centre polished but used.
            for cx, cy, length, slope in ((-12, 7, 7, 0.3), (9, -13, 5, -0.6),
                                          (6, 17, 8, 0.25), (-17, -8, 4, -0.4)):
                distance = abs(y - cy - (x - cx) * slope)
                if abs(x - cx) < length and distance < 0.20:
                    colour = tuple(c * 0.78 for c in colour)
                elif abs(x - cx) < length and distance < 0.36:
                    colour = tuple(c * 0.93 + 16 for c in colour)
    return (*colour, alpha)


def footswitch(x, y):
    radius = math.hypot(x, y)
    angle = math.atan2(y, x)
    shadow = max(0, math.hypot(x - 2.5, y - 5) - 32)
    alpha = 0.55 * math.exp(-shadow**2 / 17)
    colour = (3, 8, 11)
    if radius < 34:
        steel = 90 + 30 * (-0.4 * math.cos(angle) - 0.7 * math.sin(angle))
        steel += 25 * math.exp(-((radius - 32.8) / 0.6) ** 2)
        colour, alpha = (steel, steel, steel * 0.96), 1.0

    # Flat facets distinguish the mounting nut from the round moving plunger.
    projections = [x * math.cos(i * math.pi / 3) + (y - 1.5) * math.sin(i * math.pi / 3)
                   for i in range(6)]
    facet = max(range(6), key=projections.__getitem__)
    if projections[facet] < 28:
        light = -0.5 * math.cos(facet * math.pi / 3) - 0.7 * math.sin(facet * math.pi / 3)
        steel = 151 + 65 * light
        if projections[facet] > 26.5:
            steel = 94 + 55 * light
        colour, alpha = (steel * 1.02, steel * 1.01, steel), 1.0

    if radius < 25:
        steel = 99 + 38 * math.cos(angle + 2.2) + 14 * math.sin(radius * 15)
        colour, alpha = (steel, steel, steel * 0.97), 1.0
    if radius < 23.5:
        steel = 179 + 60 * math.cos(angle + 2.2)
        if radius < 21:
            steel = 36 + 15 * math.cos(angle + 2.2)
        colour = (steel * 1.02, steel * 1.01, steel)

    cap_y = y + 2.5
    cap_radius = math.hypot(x, cap_y)
    cap_angle = math.atan2(cap_y, x)
    if cap_radius < 19:
        steel = 145 + 70 * math.cos(cap_angle + 2.2)
        if cap_radius < 17.3:
            steel = 174 + 44 * abs(math.cos(cap_angle + 2.2)) ** 5
            steel -= 27 * (cap_radius / 17.3) ** 2
            steel += 1.4 * math.sin(cap_radius * 28)
        colour, alpha = (steel * 1.02, steel * 1.01, steel), 1.0
    if alpha == 1.0:
        crevice = 0.7 if 19 <= cap_radius and radius < 25 else 0.1
        colour = weather(colour, x + 7, y - 13, radius, crevice)
    return (*colour, alpha)


def chunk(kind, data):
    payload = kind + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload))


def render(surface=material, filename="undertow-knob.png"):
    rows = bytearray()
    for py in range(SIZE):
        rows.append(0)
        for px in range(SIZE):
            total = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SAMPLES):
                for sx in range(SAMPLES):
                    x = ((px + (sx + 0.5) / SAMPLES) / SIZE - 0.5) * EXTENT
                    y = ((py + (sy + 0.5) / SAMPLES) / SIZE - 0.5) * EXTENT
                    *colour, alpha = surface(x, y)
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
    destination = Path(__file__).resolve().parents[1] / "assets/runtime" / filename
    destination.write_bytes(png)


if __name__ == "__main__":
    render()
    render(footswitch, "undertow-footswitch.png")
