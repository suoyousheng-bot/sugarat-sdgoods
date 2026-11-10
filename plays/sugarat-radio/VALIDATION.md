---
description: "Recorded Sugarat Radio build, desktop-render and device acceptance results."
---

# Sugarat Radio validation

English | [中文](VALIDATION.zh.md)

## Summary

Validation date: 2026-11-10. Level: LV2 plus verified device write and startup. Desktop rendering and ESP-IDF compilation passed; application bytes were written and verified, startup and the English Wi-Fi setup frame were observed. Full LV3 functional acceptance remains pending.

## Recorded checks

| Check | Result |
| --- | --- |
| Hardware resource pack | `kit.py check`: 232 files verified, kit 0.1.0; imported BSP pins match the reviewed board file. |
| Firmware | ESP-IDF 5.5.4, ESP32-S3, LVGL 8.3.11, audio codec 2.5.0; DIO 80 MHz, 32 MiB Flash, Octal PSRAM. |
| Application image | 3,383,936 bytes, factory offset `0x10000`; intended application partition size `0xFE0000`. |
| Image SHA-256 | `ba14a3d58a817f8f5402f4f7f29c216b1acefebcd2e9ed4f1d04df5672124d30` |
| Real LVGL renderer | 156 view rebuilds, 13 skin previews, 10 animated skins, 600 animation frames; layout and motion checks passed. |
| Application flow | English UI text snapshots; setup success/failure navigation, skin changes, 64-station pagination, region changes, persistence failures and cleanup passed with service doubles. |
| Actual player code | 100 attempts plus timeout, supersession, pause/resume, decoder/audio failure and retry-budget checks passed with SDK doubles. |
| Actual catalog code | Curated priority, US and codec filtering, URL deduplication, malformed/incomplete response fallback and station identity checks passed with HTTP doubles. |
| Repository build | `pnpm run build:artifacts` and the subsequent `pnpm run build:check` passed. |
| Device deployment | User requested skipping backup. Actual device: ESP32-S3, 32 MiB Flash, 8 MiB PSRAM. OTA0 at `0x310000` is 5 MiB; the factory image is retained. |
| Written bytes | 3,383,936 bytes at `0x310000`; esptool hash verification passed. Official ESP-IDF OTA tool switched existing otadata at `0xe000`; its write verification passed. |
| Startup | `sugarat_radio` 0.1.0 loaded from `0x310000`; 30-second capture shows 8 MiB PSRAM initialization, 7 Wi-Fi scan results and no panic/reset loop. |
| Device frame | Captured the actual English Wi-Fi setup framebuffer. Physical display/touch and network join remain user/device acceptance. |
| Broad repository checks | `test:docs`: 21 passed. `doc-sync`: 40 passed, 3 stale generated catalogs failed outside this change. `lint` failed on TypeScript diagnostics outside this change. |

## Artifacts and remaining acceptance

Build artifacts are in `.thinkready/sugarat-radio/build-firmware`; rendered PNGs and the overview are in `.thinkready/sugarat-radio/host`. These directories are local build output. The [README](README.md) contains reproducible commands. Network and audio fixtures do not contact live stations or decode real broadcast audio. UI fixtures do not access real Wi-Fi or credentials.

LV3 remains pending: both provisioning paths, actual US network reachability, MP3/AAC sound, physical touch, sleep, power and sustained playback. The actual partition table differs from the factory build template, so deployment preserved the table, factory and all persistent data and used the existing 5 MiB OTA0 slot instead; only that application and the 8 KiB boot selector were written. The hosted curated list URL is unconfigured; station publication review remains separate from source development. This report covers development and device evidence; community firmware publication is not included.
