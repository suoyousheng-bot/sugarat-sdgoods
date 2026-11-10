#!/usr/bin/env python3
"""Inspect and install the offline Sugarat SDGOODS sources without device access."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tarfile
import tempfile
from urllib.parse import quote, urlsplit


ROOT = Path(__file__).resolve().parents[1]


def json_file(path):
    """Read one UTF-8 resource record."""
    return json.loads(path.read_text(encoding="utf-8"))


def digest(data):
    """Return the SHA-256 of exact distributed bytes."""
    return hashlib.sha256(data).hexdigest()


def child(root, name):
    """Reject noncanonical paths and symlink traversal before accessing a resource."""
    parts = PurePosixPath(name).parts
    if (not parts or PurePosixPath(name).is_absolute() or "\\" in name
            or ":" in name or any(p in (".", "..") for p in parts)
            or "/".join(parts) != name):
        raise ValueError(f"Invalid resource path: {name}")
    target = root.joinpath(*parts)
    for path in (root, *reversed(target.parents), target):
        if path.is_symlink():
            raise ValueError(f"Symlink is not a resource directory or file: {path}")
    return target


def template_files(root, lock):
    """Validate the archive, each source member and the complete template inventory."""
    record = json_file(child(root, lock["template"]["manifest"]))
    archive = child(root, lock["template"]["archive"])
    raw = archive.read_bytes()
    if len(raw) != record["archiveBytes"] or digest(raw) != record["archiveSha256"]:
        raise ValueError("Template archive size or SHA-256 differs from its manifest")
    expected = {entry["path"]: entry for entry in record["files"]}
    if len(expected) != len(record["files"]):
        raise ValueError("Duplicate paths in template manifest")
    result = {}
    with tarfile.open(archive, "r:gz") as stream:
        for entry in stream:
            child(root, entry.name)
            if not entry.isfile() or entry.name not in expected or entry.name in result:
                raise ValueError(f"Unexpected template member: {entry.name}")
            data = stream.extractfile(entry).read()
            spec = expected[entry.name]
            if len(data) != spec["bytes"] or digest(data) != spec["sha256"]:
                raise ValueError(f"Template source differs from manifest: {entry.name}")
            result[entry.name] = data
    if set(result) != set(expected) or sum(map(len, result.values())) != record["unpackedBytes"]:
        raise ValueError("Template source inventory is incomplete")
    return result


def verify(root):
    """Check shipped files and archive contents; perform no downloads or writes."""
    tracked = set()
    for line in (root / "checksums.sha256").read_text(encoding="utf-8").splitlines():
        checksum, name = line.split("  ", 1)
        if not re.fullmatch(r"[0-9a-f]{64}", checksum) or name in tracked:
            raise ValueError(f"Invalid checksum entry: {name}")
        path = child(root, name)
        if not path.is_file() or digest(path.read_bytes()) != checksum:
            raise ValueError(f"Resource missing or SHA-256 mismatch: {name}")
        tracked.add(name)
    for path in (root / "assets").rglob("*"):
        if path.is_symlink() or (path.is_file() and path.relative_to(root).as_posix() not in tracked):
            raise ValueError(f"Unlisted asset: {path.relative_to(root)}")
    lock = json_file(root / "resources.lock.json")
    template_files(root, lock)
    return lock, len(tracked)


def dependencies(catalog, ids):
    """Resolve dependencies and reject a mixture of incompatible UI styles."""
    ordered, active = [], set()

    def add(component_id):
        if component_id in active:
            raise ValueError(f"Component dependency cycle: {component_id}")
        if component_id in ordered:
            return
        if component_id not in catalog:
            raise ValueError(f"Unknown component: {component_id}; run the components command")
        active.add(component_id)
        for dependency in catalog[component_id]["dependencies"]:
            add(dependency)
        active.remove(component_id)
        ordered.append(component_id)

    for component_id in ids:
        add(component_id)
    if {"sdgoods-ui-watch", "sdgoods-ui-game"}.issubset(ordered):
        raise ValueError("Choose one UI style; packaged application cases require Watch")
    return ordered


def component_files(root, lock, ids):
    """Plan dependency-closed source copies without editing application entry points."""
    selected = dependencies(lock["components"], ids)
    files = {}
    for component_id in selected:
        source = child(root, lock["components"][component_id]["sourceDirectory"])
        for path in sorted(source.rglob("*")):
            if path.is_symlink():
                raise ValueError(f"Component symlink: {path}")
            if path.is_file():
                name = path.relative_to(source).as_posix()
                data = path.read_bytes()
                if name in files and files[name] != data:
                    raise ValueError(f"Component source conflict: {name}")
                files[name] = data
    return selected, files


def destination_path(directory):
    """Retain symlink evidence instead of resolving it away before checking parents."""
    target = Path(os.path.abspath(directory))
    for parent in (*reversed(target.parents), target):
        if parent.is_symlink():
            raise ValueError(f"Destination contains a symlink: {parent}")
    return target


def initialize(root, directory, name, ids):
    """Prepare sources in staging, then create a new project without replacing a directory."""
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,30}", name):
        raise ValueError("Project name must be 1-31 ASCII letters, digits or underscores, starting with a letter")
    lock, _ = verify(root)
    selected, extra = component_files(root, lock, ids)
    files = template_files(root, lock)
    for filename in ("CMakeLists.txt", "main/apps/ui_helloapp.c"):
        if b"SUGARAT_TEMPLATE" not in files[filename]:
            raise ValueError(f"Template project-name anchor missing: {filename}")
        files[filename] = files[filename].replace(b"SUGARAT_TEMPLATE", name.encode("ascii"))
    if set(files).intersection(extra):
        raise ValueError("Selected components conflict with the foundation")
    files.update(extra)
    target = destination_path(directory)
    if target.exists():
        raise ValueError(f"Destination already exists; sources preserved: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{target.name}-", dir=target.parent))
    try:
        for filename, data in files.items():
            path = child(staging, filename)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        # Reserve the destination exclusively before moving files into its empty directory.
        target.mkdir()
        try:
            for entry in staging.iterdir():
                entry.rename(target / entry.name)
        except OSError as error:
            raise ValueError(f"Project initialization incomplete at {target}; inspect retained files before retrying: {error}") from error
    finally:
        shutil.rmtree(staging)
    return {"project": str(target), "components": selected, "next": "Read component READMEs; add required CMake dependencies and application wiring; then build with ESP-IDF 5.5.4."}


def install(root, directory, ids):
    """Preflight all conflicts, then add files exclusively; report any partial IO effects."""
    lock, _ = verify(root)
    selected, files = component_files(root, lock, ids)
    target = destination_path(directory)
    if not (target / "CMakeLists.txt").is_file() or not (target / "components/sdgoods_board").is_dir():
        raise ValueError("Choose an existing SDGOODS ESP-IDF project, not the resource-kit directory")
    names = set(files)
    for style, opposite in (("sugarat_watch", "sdgoods-ui-game"), ("sugarat_game", "sdgoods-ui-watch")):
        if (target / "components" / style).exists() and opposite in selected:
            raise ValueError("The project already contains the other UI style; reconcile it explicitly")
    for filename in names:
        path = child(target, filename)
        for parent in path.parents:
            if parent == target:
                break
            if parent.exists() and not parent.is_dir():
                raise ValueError(f"Destination parent is not a directory: {parent}")
        if path.exists() and (not path.is_file() or path.read_bytes() != files[filename]):
            raise ValueError(f"Customized file preserved; no component files written: {path}")
    written = []
    try:
        for filename, data in sorted(files.items()):
            path = child(target, filename)
            if path.exists():
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("xb") as output:
                written.append(filename)
                output.write(data)
    except OSError as error:
        raise ValueError(f"Component installation incomplete: {error}; retained files: {json.dumps(written)}") from error
    return {"components": selected, "writtenFiles": written, "next": "Read component READMEs and wire CMake, initialization, callbacks and ownership explicitly."}


def prompt(repository, ref, task):
    """Generate one pasteable sentence for an explicitly supplied GitHub destination."""
    url = urlsplit(repository)
    if (url.scheme != "https" or url.netloc != "github.com" or url.query or url.fragment
            or not re.fullmatch(r"/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/?", url.path)):
        raise ValueError("Use a GitHub repository URL: https://github.com/OWNER/REPOSITORY")
    if not ref.strip() or any(ord(c) < 32 for c in ref):
        raise ValueError("Choose a non-empty release tag or commit reference")
    if not task.strip():
        raise ValueError("Describe the requested application")
    repo = repository.rstrip("/").removesuffix(".git")
    link = f"{repo}/blob/{quote(ref, safe='')}/AGENTS.md"
    request = " ".join(task.split())
    return f"请先阅读 {link}，获取同一版本的完整资源包，按其中的 Skill 为我的 SDGOODS ESP32-S3 圆形徽章开发：{request}；先检查项目和环境，保留已有代码，设备重启或覆盖固件前取得我的授权，并分别报告编译、烧录和真机验证结果。"


def main():
    """Expose read, source-installation and share-text operations; never access devices."""
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check", help="Verify packaged sources and SHA-256 checksums")
    listing = commands.add_parser("components", help="List components, or read one integration record")
    listing.add_argument("--id")
    for command in ("init", "add"):
        sub = commands.add_parser(command, help="Create a new firmware project" if command == "init" else "Add source components without replacing customized files")
        sub.add_argument("--directory", type=Path, required=True)
        sub.add_argument("--components", nargs="+", default=[] if command == "init" else None, required=command == "add")
        if command == "init":
            sub.add_argument("--name", required=True)
    share = commands.add_parser("prompt", help="Print the one-sentence user handoff for a published repository")
    share.add_argument("--repo", required=True)
    share.add_argument("--ref", required=True)
    share.add_argument("--task", required=True)
    args = parser.parse_args()
    try:
        if args.command == "prompt":
            print(prompt(args.repo, args.ref, args.task))
            return 0
        if args.command == "check":
            lock, count = verify(ROOT)
            result = {"status": "verified", "files": count, "kitVersion": lock["kitVersion"], "deviceAccess": False}
        elif args.command == "components":
            lock, _ = verify(ROOT)
            if args.id:
                dependencies(lock["components"], [args.id])
                result = {"id": args.id, **lock["components"][args.id]}
            else:
                result = [{"id": key, "version": value["version"], "scope": value["scope"], "dependencies": value["dependencies"]} for key, value in lock["components"].items()]
        elif args.command == "init":
            result = initialize(ROOT, args.directory, args.name, args.components)
        else:
            result = install(ROOT, args.directory, args.components)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        print(f"Sugarat kit: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
