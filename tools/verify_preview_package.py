#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inspect candidate ZIPs without executing plugins or installing anything."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import struct
import zipfile


WINDOWS_SYSTEM = set("kernel32 user32 gdi32 shell32 ole32 oleaut32 advapi32 comdlg32 comctl32 "
                     "shlwapi dwmapi version imm32 winmm opengl32 wininet ws2_32 uxtheme ntdll "
                     "crypt32 iphlpapi dwrite d2d1 d3d11 dxgi dcomp usp10 bcrypt secur32 setupapi".split())
LINUX_SYSTEM = set("libX11.so.6 libXext.so.6 libXcursor.so.1 libXrandr.so.2 libXfixes.so.3 "
                   "libGL.so.1 libGLX.so.0 libOpenGL.so.0 libstdc++.so.6 libm.so.6 libgcc_s.so.1 "
                   "libc.so.6 libdl.so.2 libpthread.so.0 librt.so.1 ld-linux-x86-64.so.2".split())


def require(condition, message):
    if not condition:
        raise ValueError(message)


def text(data, offset):
    require(0 <= offset < len(data), "String outside binary")
    end = data.index(b"\0", offset)
    return data[offset:end].decode("ascii")


def pe_dependencies(data):
    require(data[:2] == b"MZ", "Windows plugin has no DOS header")
    pe = struct.unpack_from("<I", data, 60)[0]
    require(data[pe:pe + 6] == b"PE\0\0\x64\x86", "Expected Windows x86_64 PE")
    optional = pe + 24
    require(struct.unpack_from("<H", data, optional)[0] == 0x20B, "Expected PE32+ optional header")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    sections = optional + struct.unpack_from("<H", data, pe + 20)[0]

    def offset(rva):
        for index in range(count):
            virtual_size, address, size, start = struct.unpack_from("<IIII", data, sections + index * 40 + 8)
            if address <= rva < address + max(virtual_size, size):
                require(rva - address < size, "PE RVA points outside file-backed data")
                return start + rva - address
        raise ValueError("PE RVA not mapped by a section")

    dependencies = set()
    directories = struct.unpack_from("<I", data, optional + 108)[0]
    for directory, stride, name_position in ((1, 20, 12), (13, 32, 4)):
        if directories <= directory:
            continue
        rva, size = struct.unpack_from("<II", data, optional + 112 + directory * 8)
        if not rva:
            continue
        start = offset(rva)
        for position in range(start, start + size, stride):
            record = data[position:position + stride]
            if record == b"\0" * stride:
                break
            require(len(record) == stride, "Truncated PE import descriptor")
            if directory == 13:
                require(struct.unpack_from("<I", record)[0] & 1, "Unsupported VA-based delay imports")
            name_rva = struct.unpack_from("<I", record, name_position)[0]
            dependencies.add(text(data, offset(name_rva)))
    for name in dependencies:
        lower = name.lower()
        require(lower != "ucrtbased.dll" and not re.match(r"^(msvcp|vcruntime)\d+(?:_\d+)?d(?:[_.]|$)", lower),
                f"Debug CRT in advertised Release plugin: {name}")
        allowed = lower.removesuffix(".dll") in WINDOWS_SYSTEM
        allowed |= lower.startswith(("api-ms-win-", "ext-ms-"))
        allowed |= re.fullmatch(r"(?:msvcp|vcruntime)140(?:_[a-z0-9_]+)?\.dll", lower) is not None
        allowed |= lower == "ucrtbase.dll"
        require(allowed, f"Review unexpected Windows runtime dependency before shipping: {name}")
    return {"runtime_dependencies": sorted(dependencies)}


def elf_dependencies(data):
    require(data[:6] == b"\x7fELF\x02\x01", "Expected 64-bit little-endian ELF")
    require(struct.unpack_from("<HH", data, 16) == (3, 62), "Expected x86_64 shared object")
    start = struct.unpack_from("<Q", data, 32)[0]
    stride, count = struct.unpack_from("<HH", data, 54)
    segments = []
    dynamic = None
    for index in range(count):
        kind, flags, file_offset, address, physical, size, memory, alignment = struct.unpack_from(
            "<IIQQQQQQ", data, start + index * stride)
        if kind == 1:
            segments.append((address, size, file_offset))
        elif kind == 2:
            dynamic = (file_offset, size)
    require(dynamic is not None, "Missing ELF dynamic table")

    def offset(address):
        for virtual, size, file_offset in segments:
            if virtual <= address < virtual + size:
                return file_offset + address - virtual
        raise ValueError("ELF address is not file-backed")

    tags = {}
    needed = []
    for position in range(dynamic[0], sum(dynamic), 16):
        tag, value = struct.unpack_from("<qQ", data, position)
        if tag == 0:
            break
        if tag == 1:
            needed.append(value)
        tags[tag] = value
    strings = offset(tags[5])
    dependencies = sorted(text(data, strings + value) for value in needed)
    require(set(dependencies) <= LINUX_SYSTEM, f"Review unexpected Linux dependencies: {dependencies}")
    versions = set()
    position = offset(tags[0x6FFFFFFE]) if 0x6FFFFFFE in tags else 0
    for index in range(tags.get(0x6FFFFFFF, 0)):
        version, count, filename, aux, next_entry = struct.unpack_from("<HHIII", data, position)
        auxiliary = position + aux
        for item in range(count):
            hash_value, flags, other, name, next_aux = struct.unpack_from("<IHHII", data, auxiliary)
            versions.add(text(data, strings + name))
            auxiliary += next_aux
        position += next_entry
    return {"runtime_dependencies": dependencies, "required_symbol_versions": sorted(versions)}


def mach_dependencies(data):
    require(struct.unpack_from(">II", data) == (0xCAFEBABE, 2), "Expected two-slice universal Mach-O")
    cpus, dependencies = set(), set()
    for index in range(2):
        cpu, subtype, offset, size, align = struct.unpack_from(">IIIII", data, 8 + index * 20)
        cpus.add(cpu)
        require(struct.unpack_from("<I", data, offset)[0] == 0xFEEDFACF, "Expected Mach-O 64-bit slice")
        count = struct.unpack_from("<I", data, offset + 16)[0]
        position = offset + 32
        deployment = None
        for command in range(count):
            kind, length = struct.unpack_from("<II", data, position)
            require(length >= 8 and position + length <= offset + size, "Invalid Mach-O load command")
            if kind in (0xC, 0x80000018, 0x8000001F, 0x80000023):
                name = text(data, position + struct.unpack_from("<I", data, position + 8)[0])
                require(name.startswith(("/System/Library/", "/usr/lib/")), f"Non-system Mac dependency: {name}")
                dependencies.add(name)
            if kind == 0x32:
                deployment = struct.unpack_from("<I", data, position + 12)[0]
            position += length
        require(deployment == 0x000D0000, "Advertised Mac baseline must match macOS 13.0 in both slices")
    require(cpus == {0x01000007, 0x0100000C}, "Expected Intel and Apple Silicon slices")
    return {"runtime_dependencies": sorted(dependencies), "deployment_target": "13.0"}


def verify(archive, expected_commit=None):
    archive = Path(archive)
    with zipfile.ZipFile(archive) as package:
        names = package.namelist()
        require(len(names) == len(set(names)), "Duplicate ZIP entries")
        require(sum(item.file_size for item in package.infolist()) < 512 * 1024 * 1024, "Unexpected ZIP size")
        for name in names:
            path = PurePosixPath(name)
            require(not path.is_absolute() and ".." not in path.parts and "\\" not in name and ":" not in name,
                    "Unsafe ZIP member path")
            require(path.parts[0] in ("UndertowPreview", "__MACOSX"), "Unexpected archive root")
        prefix = "UndertowPreview/"
        metadata = json.loads(package.read(prefix + "BUILD.json"))
        require(metadata["schema"] == 1 and metadata["configuration"] == "Release", "Not a Release package")
        require(re.fullmatch(r"[0-9a-f]{40}", metadata["source_commit"]), "Invalid source identity")
        if expected_commit:
            require(metadata["source_commit"] == expected_commit, "Mixed source commits in release assets")
        require(metadata["source_archive"] == f"stormsurge-fx-{metadata['release_version']}-source.tar.gz",
                "Missing matching corresponding-source archive reference")
        for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "READ-ME-FIRST.md", "RELEASE-NOTES.md",
                     "CONTROLS-AND-LIMITATIONS.md", "notices/DPF-LICENSE.txt", "notices/Undertow-artwork.md"):
            require(prefix + name in names, f"Missing licence/instructions: {name}")
        allowed = {"UndertowPreview.vst3", "notices", "LICENSE", "THIRD_PARTY_NOTICES.md", "BUILD.json",
                   "READ-ME-FIRST.md", "CONTROLS-AND-LIMITATIONS.md", "RELEASE-NOTES.md"}
        platform = metadata["platform"]
        if platform == "macos-universal":
            allowed |= {"UndertowPreview.component", "MAC-TESTING.md", "BUILD.txt"}
            results = {}
            for extension in ("vst3", "component"):
                root = prefix + f"UndertowPreview.{extension}/Contents/"
                binary = root + "MacOS/UndertowPreview"
                results[extension] = mach_dependencies(package.read(binary))
                require(package.getinfo(binary).external_attr >> 16 & 0o111, "Missing Mac executable mode")
                info = plistlib.loads(package.read(root + "Info.plist"))
                require(info["CFBundleVersion"] == metadata["plugin_version"], "Mac bundle version mismatch")
                require(root + "_CodeSignature/CodeResources" in names, "Missing Mac signature resources")
            metadata["binary_checks"] = results
        else:
            binary = prefix + "UndertowPreview.vst3/Contents/"
            if platform == "linux-x86_64":
                binary += "x86_64-linux/UndertowPreview.so"
                metadata["binary_checks"] = elf_dependencies(package.read(binary))
                require(package.getinfo(binary).external_attr >> 16 & 0o111, "Missing Linux executable mode")
            else:
                require(platform == "windows-x86_64", "Unknown release platform")
                binary += "x86_64-win/UndertowPreview.vst3"
                metadata["binary_checks"] = pe_dependencies(package.read(binary))
        for name in names:
            parts = PurePosixPath(name).parts
            if parts[0] == "UndertowPreview" and len(parts) > 1:
                require(parts[1] in allowed, f"Unintended payload in plugin-only package: {name}")
        metadata["archive_sha256"] = hashlib.sha256(archive.read_bytes()).hexdigest()
        return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--commit")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = json.dumps(verify(args.archive, args.commit), indent=2) + "\n"
    if args.report:
        args.report.write_text(result)
    print(result, end="")
