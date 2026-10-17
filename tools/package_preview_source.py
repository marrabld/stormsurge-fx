#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Create corresponding source from tracked files, including recursive submodules."""

import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile


def git(root, *arguments):
    return subprocess.check_output(["git", *arguments], cwd=root)


def package(root, destination):
    root = root.resolve()
    if git(root, "status", "--porcelain", "--untracked-files=no", "--ignore-submodules=none").strip():
        raise ValueError("Corresponding source must come from a clean committed checkout")
    modules = git(root, "submodule", "status", "--recursive").decode().splitlines()
    if any(not line.startswith(" ") for line in modules):
        raise ValueError("Initialize every recursive submodule at its recorded revision")
    commit = git(root, "rev-parse", "HEAD").decode().strip()
    timestamp = int(git(root, "show", "-s", "--format=%ct", "HEAD"))
    version = json.loads((root / "release/preview.json").read_text())["version"]
    prefix = f"stormsurge-fx-{version}-source"
    entries = git(root, "ls-files", "--stage", "--recurse-submodules", "-z").split(b"\0")
    paths = []
    for entry in entries:
        if not entry:
            continue
        index_info, path = entry.split(b"\t", 1)
        mode, blob, stage = index_info.decode().split()
        if stage != "0" or mode not in ("100644", "100755", "120000"):
            raise ValueError(f"Unsupported/uninitialized source entry: {entry!r}")
        paths.append((path.decode(), mode))
    manifest = {
        "schema": 1, "release_version": version, "source_commit": commit,
        "source_url": f"https://github.com/marrabld/stormsurge-fx/tree/{commit}",
        "submodules": [{"commit": line[1:].split()[0], "path": line[1:].split()[1]} for line in modules],
        "tracked_file_count": len(paths),
        "build_instructions": "docs/building.md", "recursive_dependencies_included": True,
    }
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / f"{prefix}.tar.gz"
    # Fixed metadata and gzip time make the source archive byte-reproducible.
    with archive.open("wb") as output, gzip.GzipFile(filename="", fileobj=output, mode="wb", mtime=timestamp, compresslevel=9) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
            for path, mode in sorted(paths):
                info = tar.gettarinfo(str(root / path), f"{prefix}/{path}")
                info.uid = info.gid = 0
                info.uname = info.gname = ""
                info.mtime = timestamp
                info.pax_headers = {}
                info.mode = 0o755 if mode == "100755" else 0o644
                if mode == "120000":
                    if not info.issym():
                        raise ValueError(f"Expected tracked symlink: {path}")
                    tar.addfile(info)
                else:
                    if not info.isfile():
                        raise ValueError(f"Expected tracked regular file: {path}")
                    with (root / path).open("rb") as source:
                        tar.addfile(info, source)
            data = (json.dumps(manifest, indent=2) + "\n").encode()
            info = tarfile.TarInfo(f"{prefix}/SOURCE-MANIFEST.json")
            info.size, info.mtime, info.mode = len(data), timestamp, 0o644
            tar.addfile(info, io.BytesIO(data))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (destination / (archive.name + ".sha256")).write_text(f"{digest}  {archive.name}\n")
    print(json.dumps(manifest, indent=2))
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    package(args.source, args.output)
