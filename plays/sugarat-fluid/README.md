---
description: "Build and play Sugarat Magic Bottle solo or pour between two SDGoods round badges."
---

# Sugarat Magic Bottle

English | [中文](README.zh.md)

## Summary

Magic Bottle turns the SDGoods round badge into a motion-controlled liquid toy. Solo play supports four colors and an adjustable quantity of up to 600 particles. Two devices can pair locally and pour mixed liquid back and forth without a router or internet. The interface is English and the application is silent. It uses the bundled ESP-IDF 5.5.4 BSP and LVGL 8.3.11 and publishes one `sugarat_fluid.bin`. Compilation, host checks and physical-device evidence are recorded separately in [validation.json](validation.json).

## Contents

- [Play](#play)
- [Build](#build)
- [Host checks](#host-checks)
- [Device and launcher](#device-and-launcher)
- [Sources and customization](#sources-and-customization)

## Play

In Solo, tilt or shake the bottle, tap the central screen for Ocean, Starlight, Amber or Forest, and hold the central screen to reset to 80%. Swipe right across the central screen to open the full-screen menu; its 60-pixel buttons fit the round display and their touch areas cover the gaps between them. Fill cycles through 100%, empty, 40% and 80% from the default 80%. Color changes recolor the practice bottle; Reset restores 80% of the selected color. The arc on the right rim is the fill gauge, with ticks at each quarter; the percentage beside it appears for 2.5 seconds after the amount changes and stays visible while paired. Only the color preference survives restart.

For two-device play:

1. Before pairing, leave liquid in one bottle and choose Fill until the other is empty. Different starting colors allow mixing.
2. On one device, swipe right, choose Play with friend, then Host room. On the other, choose Find friend, check the displayed room number, and select Join this room. Next room skips the displayed room.
3. The host selects Accept friend. Both close the menu automatically. Hold both bottles upright once to enable transfer.
4. The top edge of the screen is the bottle mouth, marked by a gap in the glass and a flared neck. An upright receiver shows pale arrows flowing into the mouth. Tilt the other device until its mouth points below horizontal to pour; amber arrows pointing out of the mouth mean the friend is not catching yet, and a prompt appears if a bottle still needs its first upright hold or is empty; more inversion increases the flow. Hold it sideways or upright to stop. Swap the postures to pour back without pairing again.
5. Full receivers, empty sources, both bottles pouring, menus, stale motion and system pause stop further transfer. A small transfer already in flight can settle. Menu access uses right swipe or a central tap while paired.

Players arrange the devices themselves; the firmware does not measure their relative position or check physical alignment. The pouring bottle's liquid is drawn toward the mouth, runs out through the neck, and air bubbles glug back in. The receiver shows a stream in the sender's color entering the mouth: it falls under the receiver's measured gravity, thins as it speeds up, breaks into droplets near the surface and splashes. Arriving liquid joins the simulation only where the stream lands, keeping the sender's color, and blends toward the mixed color over a few seconds; sloshing speeds the blending. There is no audio or multiplayer recipe mode.

The host owns both bottles' integer quantities and four ingredient amounts. Each update requires acknowledgement before the next transfer, so retries do not add liquid twice. The guest can briefly show the preceding snapshot while a transfer is in flight. After 1.5 seconds without a valid peer packet, transfer stops and the menu shows Transfer stopped. Bring the same pair back together and hold them upright to resume after synchronization. A restarted device needs a new session. New solo bottle explicitly ends the session and creates a fresh 480-unit practice bottle; session liquid is not saved across exit or power loss. Invitations expire after 10 seconds.

Nearby uses ESP-NOW on `CONFIG_BOTTLE_RADIO_CHANNEL` (default 6, configurable from 1 to 11 in menuconfig); both devices must use the same channel. Wi-Fi starts only when hosting or searching and stops on Solo, application exit or power-off. Pairing pins peer addresses and fresh room identities but does not provide cryptographic peer authentication or encrypted application data. Use it for local play, not confidential data. Radio initialization failure shows Radio unavailable and leaves Solo available. Radio performance, memory headroom, battery life, physical gestures and real two-device transfer require hardware acceptance.

## Build

Activate ESP-IDF 5.5.4 with the ESP32-S3 toolchain. From the resource-kit root, run:

```sh
idf.py -C plays/sugarat-fluid -B /tmp/sugarat-fluid-build build
```

The application image is `/tmp/sugarat-fluid-build/sugarat_fluid.bin`. Board wiring, Flash DIO at 80 MHz and Octal PSRAM remain owned by the bundled BSP. The performance build keeps assertions enabled. Particle positions use internal RAM; render palettes, particle snapshots, the neighbour-pair cache and pixel frames use PSRAM. Three pixel buffers separately hold the frame being rendered, the newest complete frame and the frame being displayed. Only ownership exchange holds the shared lock; rendering happens outside it. Empty polls reserve no display slot. With no text, touch or system overlay, the bundled strip sender presents pixels directly; overlays use the LVGL path.

## Host checks

With CMake, Ninja, a C compiler and Pillow available, run:

```sh
python3 plays/sugarat-fluid/tests/check_host.py --output /tmp/sugarat-fluid-host
```

Frame handoff checks cover an unfinished producer frame, replacement by newer frames and presentation timing when no frame is ready. The real solver runs 2400 changing-gravity and shake steps and empty/full refill sequences with AddressSanitizer and UndefinedBehaviorSanitizer. Two instances of the actual session engine exchange lossy datagrams to check bidirectional emptying, ingredient conservation, capacity, stale/reordered packets, invitation expiry, pause and reconnect. The real LVGL view renders themes and all menu states, verifies displayed glyphs and label bounds, and compares the owner-local text snapshot. Host rendering does not verify physical motion, touch, power, panel refresh or physical speaker sound.

## Device and launcher

Standalone deployment requires a device-specific write plan; the build partition table is not evidence of the connected device layout. Launcher installation consumes the application BIN and lets the installer choose its address. The bundled `sdgoods_launcher` component provides `sdgoods_device_mode.h` for mode detection, `sdgoods_cc.h` for the control center and `sdgoods_app_sdk.h` for `sdgoods_return_to_launcher()`. Magic Bottle binds its pause/resume hooks through `sdgoods_app_shell_bind()` and uses the control center’s mode-aware Power/Exit action. It stores its theme in a private NVS namespace and does not currently call `sdgoods_appdata_*`. The application retains these BSP interfaces; switching applications restarts the device. Standalone testing does not establish launcher compatibility. Follow the [deployment instructions](../../../hardware-kits/sugarat-sdgoods/references/deployment.md) before writing.

The application is silent and has no audio task or sound control. The control center pauses simulation and flushes the pending theme before its actions. Screen suspension freezes the simulation. Solo theme changes are saved after 700 ms without another change, before starting the radio, or when system controls open; power-off also requests a save. Abrupt power loss before a save can lose the latest theme. The private `sugarat_fluid` NVS namespace stores `theme`; any sound preference left by an earlier firmware is ignored. Storage failure remains visible; the application never erases NVS to recover. Unavailable or stale motion readings freeze the liquid and show a restart instruction.

## Sources and customization

The solver comes from MIT-licensed [FluidBox](https://github.com/V4C38/esp32-fluidbox), pinned by [upstream.json](upstream.json), with its [license](main/fluid/LICENSE) preserved. The circular wall, renderer and device integration are maintained here. BSP and Watch components retain their own license files and notices. Product copy uses Sugarat.

Change physics coefficients in `main/fluid/config.h`, palettes in `main/fluid/render.c`, presentation in `main/apps/bottle_view.c`, quantities in `main/fluid/liquid.c`, and session rules in `main/fluid/link.c`. The worker owns the simulation and session; radio callbacks enqueue datagrams and LVGL reads published snapshots. Snapshot revisions and ingredient totals are validated before replacing guest quantities. Regenerate Watch fonts with its documented generator when adding Chinese text. Keep `project(sugarat_fluid)` stable so updates retain application identity. The four themes use procedural colors and require no separate resource image.
