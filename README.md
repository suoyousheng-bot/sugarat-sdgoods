---
description: "Build the Sugarat device launcher used by desktop multiple-application installation."
kind: "reference"
---

# Sugarat launcher

English | [中文](README.zh.md)

The Sugarat launcher displays installed community plays on the SDGOODS ESP32-S3 round badge and opens one independent firmware at a time. Applications return by selecting the factory launcher and restarting. It uses a 10 MiB application pool with sixteen dynamically sized OTA slots; each slot reserves 56 KiB for its icon and publication metadata. Screen, touch, wireless, exit and power behavior require physical acceptance.

## Build

Use Python from an existing ESP-IDF 5.5.4 environment, its SDK and tool cache. The output directory must not already contain firmware sources:

```sh
python firmware-plays/sugarat-launcher/build.py \
  --sdk /absolute/path/to/esp-idf \
  --tools /absolute/path/to/espressif-tools \
  --output /absolute/path/to/new-build-directory
```

The builder assembles the bundled hardware foundation and reviewed UI/Wi-Fi components, builds the firmware, and packages the launcher and standard ESP-IDF bootloader into Hardware Studio with their SHA-256 catalog. It does not open serial ports. The committed build partition table only supplies linker size limits; the desktop installer generates and validates the live sixteen-slot table. Never flash the build table directly as a multi-application layout.

## Installation and compatibility

[Hardware Studio](../../packages/experimental/hardware-studio/README.md#community-firmware-installation) owns USB detection, immutable community downloads, mode selection, backup access, overwrite acceptance, Flash writes and verification. First-time launcher installation changes partitions and system settings. It cannot convert arbitrary existing firmware into a managed application. Existing applications must have a separately reviewed launcher-compatible image.

The standard bundled bootloader does not restore the partition table from the A/B journal automatically. The launcher records table updates in that journal, and the desktop refuses inconsistent table/journal state; interrupted writes require inspection or full-backup recovery. No automatic rollback is promised. High application-data storage is retained rather than reformatted; an absent or damaged filesystem is reported by its owning firmware and is not silently erased.

Desktop-installed slots carry a `SGAPP001` record in their final 56 KiB: a 24-byte header contains the magic, little-endian JSON and JPEG lengths, a CRC-32 over both payloads, and one reserved word; UTF-8 JSON identifies the embedded project, community play, title and category, followed by the published baseline JPEG. The launcher checks lengths, CRC and project identity before using metadata. Older SDGOODS launchers may run the application without displaying this title or icon; their `SDGCOVER` format is separate. Community plays remain independent ESP-IDF images, not modules loaded into the launcher process.

## Source and licenses

`main/` contains the launcher, slot allocation and journal implementation. Board support comes from Hardware Studio's pinned product template; UI and Wi-Fi sources come from its reviewed component library. The upstream [LICENSE](LICENSE), [NOTICE](NOTICE), [LICENSING.md](LICENSING.md) and [TRADEMARK.md](TRADEMARK.md) accompany the project. Product copy uses Sugarat; `SDGOODS_LAUNCHER` remains the embedded identity that managed applications recognize.
