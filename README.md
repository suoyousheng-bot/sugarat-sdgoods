---
description: "Build, flash and maintain the Sugarat device launcher, the factory firmware that hosts community plays."
kind: "reference"
---

# Sugarat launcher

English | [中文](README.zh.md)

The Sugarat launcher displays installed community plays on the SDGOODS ESP32-S3 round badge and opens one independent firmware at a time. Applications return by selecting the factory launcher and restarting. It uses a 10 MiB application pool with sixteen dynamically sized OTA slots; each slot reserves 56 KiB for its icon and publication metadata. Screen, touch, wireless, exit and power behavior require physical acceptance.

## Build

This branch is a complete ESP-IDF project: every component is in `components/`, so the build downloads nothing. Run `set-target` once per clone. Use ESP-IDF 5.5.4 exactly; other releases are untested on this board. The project name must stay `SDGOODS_LAUNCHER`, because installed applications and the desktop recognize the launcher by it.

```sh
idf.py set-target esp32s3
idf.py build
```

The result is `build/SDGOODS_LAUNCHER.bin`. It must fit the 3 MiB factory partition; the build reports the remaining space. `idf.py menuconfig` → Sugarat Launcher sets the community service URL, product and hardware revision IDs, and the serial test input.

## Flash and test

Flash only the application onto a badge that already runs the launcher; `<port>` is its serial port:

```sh
idf.py -p <port> app-flash monitor
```

`app-flash` writes `SDGOODS_LAUNCHER.bin` to the factory partition at `0x10000` and keeps the bootloader, the live partition table, the journal, installed plays, Wi-Fi settings and application data. Close the Sugarat desktop or anything else that holds the serial port first.

Never run `idf.py flash` or `idf.py erase-flash` on a badge. `flash` also writes the build partition table, which has one 10 MiB `ota_0` row instead of the live sixteen-slot table; installed plays are then unrecognized and the desktop refuses the device. `erase-flash` removes everything, including Wi-Fi settings and application data. Before risky tests, make a full backup with the desktop's offline backup.

A badge without the launcher is set up once from the Sugarat desktop: install any community play in multiple-application mode. That writes the bootloader, the sixteen-slot table, the first journal and the bundled launcher; then `app-flash` replaces the launcher with your build.

With `LAUNCHER_SERIAL_TEST_INPUT` enabled (the development default), the serial console accepts the synthetic taps, swipes and screenshots documented in `main/launcher_console.c`. Production builds disable it.

## Change the launcher

New Chinese UI text needs glyphs in the bundled fonts, or it renders as blanks. Regenerate them from the launcher sources with Pillow and the `scripts/built_in_font` directory of LVGL v8.3.11 (for example from `git clone --depth 1 -b v8.3.11 https://github.com/lvgl/lvgl`):

```sh
cd components/sugarat_watch
python3 tools/make_fonts.py --font-dir <lvgl-v8.3.11>/scripts/built_in_font --sources ../../main
```

Submit changes as pull requests to the `launcher` branch; maintainers review and merge them. Describe the physical checks in the pull request: home, app grid, download and install, uninstall, return from a play, Wi-Fi settings, and restart during installation.

## Installation and compatibility

The Sugarat desktop owns USB detection, immutable community downloads, mode selection, backup access, overwrite acceptance, Flash writes and verification. First-time launcher installation changes partitions and system settings. It cannot convert arbitrary existing firmware into a managed application. Existing applications must have a separately reviewed launcher-compatible image.

The standard bundled bootloader does not restore the partition table from the A/B journal automatically. The launcher records table updates in that journal, and the desktop refuses inconsistent table/journal state; interrupted writes require inspection or full-backup recovery. No automatic rollback is promised. High application-data storage is retained rather than reformatted; an absent or damaged filesystem is reported by its owning firmware and is not silently erased.

Desktop-installed slots carry a `SGAPP001` record in their final 56 KiB: a 24-byte header contains the magic, little-endian JSON and JPEG lengths, a CRC-32 over both payloads, and one reserved word; UTF-8 JSON identifies the embedded project, community play, title and category, followed by the published baseline JPEG. The launcher checks lengths, CRC and project identity before using metadata. Older SDGOODS launchers may run the application without displaying this title or icon; their `SDGCOVER` format is separate. Community plays remain independent ESP-IDF images, not modules loaded into the launcher process.

## Source and licenses

`main/` contains the launcher, slot allocation and journal implementation. `components/sdgoods_board`, `sdgoods_launcher`, `lvgl` (8.3.11) and `jpegenc` come unchanged from the SDGOODS product template in `assets/templates` on the `main` branch. `sugarat_watch`, `sugarat_pixel`, `sugarat_wifi` and `sugarat_wifi_ui` come from `assets/components` on `main`; the `sugarat_watch` text fonts are regenerated from the launcher sources. The upstream [LICENSE](LICENSE), [NOTICE](NOTICE), [LICENSING.md](LICENSING.md) and [TRADEMARK.md](TRADEMARK.md) accompany the project. Product copy uses Sugarat; `SDGOODS_LAUNCHER` remains the embedded identity that managed applications recognize.
