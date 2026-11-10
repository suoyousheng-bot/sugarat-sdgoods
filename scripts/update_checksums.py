#!/usr/bin/env python3
"""Regenerate the release inventory after intentionally updating distributed resources."""

import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main():
    """Write one sorted checksum entry per shipped file, excluding local generated trees."""
    files = []
    for path in ROOT.iterdir():
        if path.is_symlink():
            raise ValueError(f"Release symlink: {path}")
        if path.is_file() and path.name != "checksums.sha256":
            if path.name.startswith(".env") or path.suffix in (".bin", ".pyc"):
                raise ValueError(f"Local data does not belong in the kit root: {path}")
            files.append(path)
    for name in ("agents", "assets", "boards", "licenses", "references", "scripts"):
        for path in (ROOT / name).rglob("*"):
            if path.is_symlink():
                raise ValueError(f"Release symlink: {path}")
            if path.is_file() and "__pycache__" not in path.parts:
                files.append(path)
    entries = []
    for path in sorted(files):
        checksum = hashlib.sha256(path.read_bytes()).hexdigest()
        entries.append(f"{checksum}  {path.relative_to(ROOT).as_posix()}\n")
    (ROOT / "checksums.sha256").write_text("".join(entries), encoding="utf-8")
    print(f"Recorded {len(entries)} release files")


if __name__ == "__main__":
    main()
