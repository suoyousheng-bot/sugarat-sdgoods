---
description: "Build and customize the English Pocket Bird game for the SDGOODS round badge."
---

# Sugarat Pocket Bird

English | [中文](README.zh.md)

## Summary

Tap to keep the bird flying through pipes on the 360×360 SDGOODS round badge. Each passed pipe earns one point; every five points increases difficulty, up to level eight. The game keeps a best score up to 9999, offers instant retries and runs offline with an English interface. It uses ESP-IDF 5.5.4, bundled LVGL 8.3.11 and the Sugarat Game theme. Build and host evidence is recorded in [validation.json](validation.json); device behavior remains unverified.

## Contents

- [Play](#play)
- [Build](#build)
- [Host checks](#host-checks)
- [Persistence and failures](#persistence-and-failures)
- [Launcher and device scope](#launcher-and-device-scope)
- [Sources and customization](#sources-and-customization)

## Play

Press `Play`, then tap the central screen to flap. A held touch gives one flap. Touching a pipe, the top or the ground ends the run; `Try again` starts a fresh score while retaining the best. The flight field is 240×168 pixels at (60,120), with English controls and text inside the round screen's safe rectangle. The application is silent and starts no networking or audio service.

A top-edge swipe opens the BSP control center and pauses flight. Closing it leaves a `Resume` button. Screen sleep and a scheduling gap over 250 ms also pause the game; resume explicitly to avoid falling while returning to the screen. Fixed 8 ms simulation steps make physics independent of draw cadence. Rendering targets 30 updates/s while playing; paused and ready screens do not redraw continuously. Host checks do not establish the panel's actual frame rate or touch latency.

## Build

Activate ESP-IDF 5.5.4 with the ESP32-S3 toolchain, then run from the resource-kit root:

```sh
idf.py -C plays/sugarat-bird -B /tmp/sugarat-bird-build build
```

The application image is `/tmp/sugarat-bird-build/sugarat_bird.bin`. Publish only this application BIN. Keep `project(sugarat_bird)` stable because the launcher uses the project name as application identity. Board wiring, Flash DIO 80 MHz, Octal PSRAM and the reference partition table remain owned by the bundled BSP. Build-generated bootloader and partition images are not application installation resources.

## Host checks

With CMake, Ninja, a C compiler and Pillow available, run:

```sh
python3 plays/sugarat-bird/tests/check_host.py --output /tmp/sugarat-bird-host
```

The real model, storage layer, controller and LVGL view run with AddressSanitizer and UndefinedBehaviorSanitizer and a 32 KiB LVGL heap. Checks cover frame-rate independence, collisions, scoring, a 45-pipe run, difficulty limits, highest-score restoration, storage failures, pointer input, overlay/sleep pause, explicit resume and repeated screen disposal. English glyphs and label bounds are checked through score 9999. The owner-local [text snapshot](tests/expected/ui-text.txt) covers ready, playing, paused, game-over, save-error and maximum-score screens. The command exports `preview-en.png` from the actual LVGL view; it is not a device screenshot.

## Persistence and failures

The private NVS namespace `sugarat_bird` stores one `u32` key, `best`. A higher score is saved when the run ends, controls open or normal power-off is requested; commit and readback must both succeed. No frame writes storage. A lower score never replaces a higher saved value. Abrupt power loss during a run can lose its latest best score.

Unreadable or out-of-range stored scores disable writes for that run; the game stays playable and displays `Best not saved`. A write, commit or readback failure keeps the in-memory best, displays the same notice and permits another attempt after a later run or normal shutdown. The application never erases NVS or formats storage as recovery. A successful host mock does not establish physical Flash durability.

## Launcher and device scope

The bundled `sdgoods_launcher` SDK supplies mode detection, the control center and `sdgoods_return_to_launcher()`. The application binds shell pause/resume callbacks and leaves the control center's mode-aware `Exit`/`Power` action intact. Application switching restarts the firmware; no application code writes the partition table, otadata or a fixed installation address. Highest-score data is independent of the selected application slot.

No serial inspection, flashing or physical device test is part of this development delivery. Launcher installation, opening, Exit, reopening with the best retained, coexistence with other applications, and physical display/input/power behavior still require device acceptance. Follow the [deployment instructions](../../../hardware-kits/sugarat-sdgoods/references/deployment.md) when device work is authorized.

## Sources and customization

The flight and pipe rules derive from the Apache-2.0 [SDGOODS Flappy example](https://github.com/SDGOODS/SDGOODS-ESP32S3/blob/main/main/apps/ui_flappy.c); [upstream.json](upstream.json) pins the inspected source. The local kit's template manifest pins the BSP foundation. Deotaland maintains the isolated time-based model, English round-screen presentation, pixel bird, highest-score storage and host checks. Retain the foundation's license and notices when distributing the application.

Tune flight and difficulty in `main/bird/bird_model.c`, layout and sprites in `main/bird/bird_view.c`, and lifecycle integration in `main/apps/ui_helloapp.c`. The view uses the bundled Game palette, fonts, buttons and frame components. Application text is English; the BSP is fixed to English as well. No remote backend or separate resource image is needed.
