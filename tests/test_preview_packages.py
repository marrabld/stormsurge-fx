# SPDX-License-Identifier: GPL-3.0-only
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_preview_package import elf_dependencies, pe_dependencies, verify


def elf():
    data = bytearray(4096)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HH", data, 16, 3, 62)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HH", data, 54, 56, 2)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, len(data), len(data), 4096)
    struct.pack_into("<IIQQQQQQ", data, 120, 2, 4, 512, 512, 512, 64, 64, 8)
    for index, pair in enumerate(((5, 1024), (1, 1), (10, 12), (0, 0))):
        struct.pack_into("<qQ", data, 512 + index * 16, *pair)
    data[1024:1035] = b"\0libc.so.6\0"
    return bytes(data)


def pe(dependency):
    data = bytearray(1024)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 128)
    data[128:134] = b"PE\0\0\x64\x86"
    struct.pack_into("<H", data, 134, 1)
    struct.pack_into("<H", data, 148, 240)
    struct.pack_into("<H", data, 152, 0x20B)
    struct.pack_into("<I", data, 260, 16)
    struct.pack_into("<II", data, 272, 0x1000, 40)
    struct.pack_into("<IIII", data, 400, 512, 0x1000, 512, 512)
    struct.pack_into("<I", data, 524, 0x1040)
    name = dependency.encode() + b"\0"
    data[576:576 + len(name)] = name
    return bytes(data)


class PackageTests(unittest.TestCase):
    def test_binary_dependency_inspection(self):
        self.assertEqual(elf_dependencies(elf())["runtime_dependencies"], ["libc.so.6"])
        self.assertEqual(pe_dependencies(pe("VCRUNTIME140.dll"))["runtime_dependencies"], ["VCRUNTIME140.dll"])

    def test_debug_and_unreviewed_dependencies_rejected(self):
        for name in ("VCRUNTIME140D.dll", "MSVCP140_1D.dll", "ucrtbased.dll", "unreviewed.dll"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                pe_dependencies(pe(name))

    def test_not_32_bit(self):
        data = bytearray(pe("KERNEL32.dll"))
        data[132:134] = b"\x4c\x01"
        with self.assertRaises(ValueError):
            pe_dependencies(data)

    def write_package(self, root, extra=None, commit="a" * 40, configuration="Release"):
        path = Path(root) / "candidate.zip"
        metadata = {"schema": 1, "configuration": configuration, "source_commit": commit,
                    "platform": "linux-x86_64", "release_version": "0.4.0-preview.1",
                    "plugin_version": "0.4.0", "source_archive": "stormsurge-fx-0.4.0-preview.1-source.tar.gz"}
        with zipfile.ZipFile(path, "w") as archive:
            archive.writestr("UndertowPreview/BUILD.json", json.dumps(metadata))
            for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "READ-ME-FIRST.md", "RELEASE-NOTES.md",
                         "CONTROLS-AND-LIMITATIONS.md", "notices/DPF-LICENSE.txt", "notices/Undertow-artwork.md"):
                archive.writestr("UndertowPreview/" + name, "fixture")
            executable = zipfile.ZipInfo("UndertowPreview/UndertowPreview.vst3/Contents/x86_64-linux/UndertowPreview.so")
            executable.external_attr = 0o100755 << 16
            archive.writestr(executable, elf())
            if extra:
                archive.writestr(extra, "unintended")
        return path

    def test_valid_package(self):
        with tempfile.TemporaryDirectory() as root:
            self.assertEqual(verify(self.write_package(root), "a" * 40)["platform"], "linux-x86_64")

    def test_unsafe_paths_and_unintended_payloads(self):
        for extra in ("../escape", "UndertowPreview/../escape", "C:/escape", "UndertowPreview/UndertowFoundation.exe"):
            with self.subTest(extra=extra), tempfile.TemporaryDirectory() as root, self.assertRaises(ValueError):
                verify(self.write_package(root, extra))

    def test_debug_and_mixed_source_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(ValueError):
                verify(self.write_package(root, configuration="Debug"))
            with self.assertRaises(ValueError):
                verify(self.write_package(root), "b" * 40)


if __name__ == "__main__":
    unittest.main()
