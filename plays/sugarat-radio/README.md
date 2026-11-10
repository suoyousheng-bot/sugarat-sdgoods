---
description: "Build and customize the US Sugarat Radio firmware for the ESP32-S3 round badge."
---

# Sugarat Radio

English | [中文](README.zh.md)

## Summary

Sugarat Radio adapts the Apache-2.0 [sdgoods-radio](https://github.com/YeatsLiao/sdgoods-radio) application for the documented ESP32-S3 360×360 badge. It keeps 13 skins and MP3/AAC streaming, uses English defaults, selects US markets explicitly, and reuses Sugarat Wi-Fi setup. See [PLAY.md](PLAY.md) for behavior and configurable parameters, and [VALIDATION.md](VALIDATION.md) for the verified delivery level.

## Contents

- [Build](#build)
- [Host verification](#host-verification)
- [Source and distribution](#source-and-distribution)

<a id="build"></a>
## Build

Activate ESP-IDF 5.5.4 with the ESP32-S3 toolchain before running these commands from the resource-kit root. The component manager resolves the committed lockfile, LVGL 8.3.11 and Espressif audio codec 2.5.0. No USB device is required.

```sh
idf.py -C plays/sugarat-radio -B /tmp/sugarat-radio-build build
```

The build emits `sugarat_radio.bin`, bootloader, partition table and `flash_args` in that build directory. This is a standalone factory application; the committed partition table describes its intended layout, not the connected device. Do not use upstream platform launcher binaries or flash scripts. Deployment requires inspecting the actual target, Flash capacity and boot layout, notifying the user, and accepting reset/overwrite effects before writing. Source development and compilation do not require opening a serial port.

<a id="host-verification"></a>
## Host verification

With the SDK still activated, a native C compiler and Pillow installed, run:

```sh
python3 plays/sugarat-radio/tests/check_host.py --output /tmp/sugarat-radio-host
```

The runner compiles actual LVGL/application/setup UI code with deterministic network, storage and audio fixtures, checks owner-local [UI text snapshots](tests/expected/ui-text.json), and renders `sugarat-radio-overview.png`. It also executes the actual player retry/cleanup code and directory parser with SDK doubles. Images and tests establish software behavior; they do not establish Wi-Fi reception, sound, physical touch or device startup. Use `--record` only after reviewing intentional UI text changes and the rendered pages.

<a id="source-and-distribution"></a>
## Source and distribution

[upstream.json](upstream.json) pins the imported upstream revision. [LICENSE](LICENSE), [NOTICE](NOTICE), [LICENSING.md](LICENSING.md), [TRADEMARK.md](TRADEMARK.md) and font notices accompany the source. Modified files carry a change notice. Sugarat is the product brand; upstream names remain in code identifiers and legal attribution. No upstream marketplace publishing or prebuilt launcher is included.

A hosted curated station service is not deployed by this project. The default firmware queries the US Radio Browser directory, then uses bundled stream URLs on directory failure. Public stream availability is separate from station approval for a commercial product; the development list is not a representation of a distribution agreement. No iHeartRadio, TuneIn, account or geolocation service is integrated.
