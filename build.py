#!/usr/bin/env python3
"""Assemble, build and package the Sugarat launcher without accessing a device."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tarfile

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
STUDIO = REPO / "packages/experimental/hardware-studio"


def main():
    """Use the bundled board foundation and reviewed component sources with an explicit SDK."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    work = args.output.resolve()
    firmware = work / "firmware"
    if firmware.exists():
        raise ValueError("Choose a new output directory; existing firmware sources are preserved")
    firmware.mkdir(parents=True)
    with tarfile.open(STUDIO / "src/product-templates/sdgoods-esp32s3-basic-v1.tar.gz") as archive:
        archive.extractall(firmware, filter="data")
    shutil.rmtree(firmware / "main")
    shutil.copytree(ROOT / "main", firmware / "main")
    for name in ["CMakeLists.txt", "sdkconfig.defaults", "partitions.csv", "version.txt"]:
        shutil.copy2(ROOT / name, firmware / name)
    for catalog, component in [("sdgoods-ui-watch", "sugarat_watch"), ("sdgoods-pixel-characters", "sugarat_pixel"),
                               ("wifi-provisioning", "sugarat_wifi"), ("sdgoods-wifi-ui", "sugarat_wifi_ui")]:
        shutil.copytree(STUDIO / f"firmware-components/{catalog}/components/{component}", firmware / f"components/{component}")
    sys.path.insert(0, str(REPO / "examples"))
    from case_tools.sdk_build import build_case
    build_case(work, "firmware", args.sdk, args.tools)
    build = work / "build-firmware"
    project = json.loads((build / "project_description.json").read_text())
    if project["project_name"] != "SDGOODS_LAUNCHER" or project["target"] != "esp32s3":
        raise ValueError("Expected the SDGOODS ESP32-S3 launcher")
    destination = STUDIO / "src/community-firmware"
    files = []
    for name, source in [("sugarat-platform-bootloader.bin", build / "bootloader/bootloader.bin"),
                         ("sugarat-launcher.bin", build / "SDGOODS_LAUNCHER.bin")]:
        data = source.read_bytes()
        if name == "sugarat-launcher.bin" and len(data) > 0x300000:
            raise ValueError("Launcher exceeds the factory partition")
        (destination / name).write_bytes(data)
        files.append(dict(name=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    (STUDIO / "src/community-launcher-catalog.ts").write_text(
        "/** Fingerprints of the product launcher and its standard ESP-IDF 5.5.4 bootloader. */\n"
        + "export const launcherFiles = " + json.dumps(files, indent=2) + " as const\n")


if __name__ == "__main__":
    main()
