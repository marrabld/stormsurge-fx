#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Collect verified platform archives and corresponding source. Never publishes."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile

from verify_preview_package import require, verify


def assemble(incoming, output, commit, release_file):
    release = json.loads(release_file.read_text())
    version = release["version"]
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    for platform in release["platforms"]:
        source = incoming / f"UndertowPreview-{platform}.zip"
        checksum = (incoming / (source.name + ".sha256")).read_text().split()[0]
        require(hashlib.sha256(source.read_bytes()).hexdigest() == checksum, "Input ZIP checksum mismatch")
        report = verify(source, commit)
        require(report["platform"] == platform and report["release_version"] == version and
                report["plugin_version"] == release["plugin_version"], "Mixed release identities")
        if platform == "linux-x86_64":
            require(report["build_baseline"] == "ubuntu-24.04",
                    "Do not advertise a newer local Linux build as the Ubuntu 24.04 candidate")
        name = f"UndertowPreview-{version}-{platform}.zip"
        shutil.copyfile(source, output / name)
        report["filename"] = name
        reports.append(report)
    source_name = f"stormsurge-fx-{version}-source.tar.gz"
    source = incoming / source_name
    checksum = (incoming / (source_name + ".sha256")).read_text().split()[0]
    require(hashlib.sha256(source.read_bytes()).hexdigest() == checksum, "Source archive checksum mismatch")
    prefix = source_name.removesuffix(".tar.gz")
    with tarfile.open(source, "r:gz") as tar:
        manifest = json.load(tar.extractfile(prefix + "/SOURCE-MANIFEST.json"))
        require(manifest["source_commit"] == commit and manifest["release_version"] == version,
                "Corresponding source differs from binary source")
        names = set(tar.getnames())
        for needed in ("CMakePresets.json", "CMakeLists.txt", "docs/building.md",
                       "external/dpf/distrho/DistrhoPluginMain.cpp",
                       "external/dpf/dgl/src/pugl-upstream/include/pugl/pugl.h"):
            require(prefix + "/" + needed in names, f"Incomplete recursive source: {needed}")
    shutil.copyfile(source, output / source_name)
    asset_names = [report["filename"] for report in reports] + [source_name]
    sums = []
    for name in asset_names:
        sums.append(f"{hashlib.sha256((output / name).read_bytes()).hexdigest()}  {name}\n")
    (output / "SHA256SUMS").write_text("".join(sums))
    candidate = {"schema": 1, "release_version": version, "source_commit": commit,
                 "status": "candidate-not-published", "platforms": reports,
                 "source_archive": source_name,
                 "runtime_testing": {"linux_bitwig": "maintainer listening on earlier development builds",
                                     "windows_daw": "unverified", "mac_daw": "pending Dom"}}
    (output / "CANDIDATE.json").write_text(json.dumps(candidate, indent=2) + "\n")
    print(json.dumps(candidate, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--release", type=Path, default=Path(__file__).resolve().parents[1] / "release/preview.json")
    args = parser.parse_args()
    assemble(args.input, args.output, args.commit, args.release)
