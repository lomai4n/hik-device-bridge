# SPDX-License-Identifier: Apache-2.0
"""Build a source-only local review candidate from an explicit file allowlist."""
import hashlib
import io
from pathlib import Path
import tarfile

VERSION = "0.1.0-preview.1"
FILES = (
    ".gitignore", "AGENTS.md", "CMakeLists.txt", "README.md", "LICENSE",
    "docs/ROADMAP.md", "docs/SOURCES.md", "docs/VALIDATION.md",
    "docs/SDK-LICENSE.md", "docs/RELEASE.md",
    "include/hik_bridge/bridge.hpp", "src/bridge.cpp", "src/main.cpp",
    "src/password.cpp", "src/sdk_probe.cpp", "tests/bridge_tests.cpp",
    "tests/cli_tests.py", "tests/symbol_fixture.cpp", "tools/package_source.py",
)
ROOT = Path(__file__).resolve().parents[1]


def main():
    # No recursive archive: unrelated untracked files must never enter the candidate.
    selected = []
    for name in FILES:
        path = ROOT / name
        if not path.is_file() or any(part.is_symlink() for part in [path, *path.parents]):
            raise ValueError("missing or unsafe allowlisted source")
        selected.append((name, path.read_bytes()))
    output_dir = ROOT / "work"
    if output_dir.is_symlink():
        raise ValueError("unsafe output directory")
    output_dir.mkdir(exist_ok=True)
    output = output_dir / f"hik-device-bridge-{VERSION}-source.tar.gz"
    if output.is_symlink():
        raise ValueError("unsafe output file")
    with tarfile.open(output, "w:gz") as archive:
        for name, data in selected:
            info = tarfile.TarInfo(f"hik-device-bridge-{VERSION}/" + name)
            info.size, info.mode, info.mtime = len(data), 0o644, 0
            archive.addfile(info, io.BytesIO(data))
    with tarfile.open(output, "r:gz") as archive:
        entries = archive.getmembers()
        expected = [f"hik-device-bridge-{VERSION}/" + name for name in FILES]
        if [entry.name for entry in entries] != expected or not all(entry.isfile() for entry in entries):
            raise ValueError("unexpected archive contents")
        for entry, (_, data) in zip(entries, selected):
            if archive.extractfile(entry).read() != data:
                raise ValueError("archive content mismatch")
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    checksum = output.with_name(output.name + ".sha256")
    if checksum.is_symlink():
        raise ValueError("unsafe checksum file")
    checksum.write_text(digest + "  " + output.name + "\n")
    print(str(output.relative_to(ROOT)))
    print(digest)


if __name__ == "__main__":
    main()
