---
description: "Play tilt-controlled racing and marble time trials on an SDGOODS round badge."
---

# Sugarat Round Race

English | [中文](README.zh.md)

## Summary

Sugarat Round Race provides two tilt-controlled games with three fixed maps each. Players steer automatically accelerating cars or roll a ball through checkpoints to the finish. Menus use horizontal swipes and 210×64-pixel action buttons. The firmware uses the SDGOODS ESP32-S3 360×360 BSP, ESP-IDF 5.5.4 and LVGL 8.3.11 from hardware kit 0.2.1. Build, host checks and physical-device evidence are separate in [validation.json](validation.json).

## Contents

- [Play](#play)
- [Build and check](#build-and-check)
- [Maps and rendering](#maps-and-rendering)
- [Device installation](#device-installation)

<a id="play"></a>
## Play

All on-screen text is English. Swipe left or right on the home page to choose Tilt Racer or Marble Run. Tap Play, then swipe to choose a map. Choose Solo, hold the badge comfortably, flat, upright or anywhere between, and tap Start here; that hold becomes level for both tilt axes. The shared gesture menu remains available through a long press on empty screen space. Long press on map selection returns home; on calibration or the lobby it returns to map selection.

Racing starts after a three-second countdown and runs three laps. Tilt left or right to steer; leaving the road reduces speed. In Solo the player starts behind three computer rivals; the rivals pass a slower car and keep the field close. Each lap has four orange chevron pads that boost the car for 1.8 seconds, and touching a rival costs speed. A clean race without pads usually finishes third or fourth. Marbles uses both tilt axes; the ball rolls with gravity, low rolling resistance and bounces off walls. Collect all three gold checkpoints, then reach the checked finish tile. The white ball is local and the cyan ghost shows the peer when connected. Games and times are not saved after exit or power loss.

The speaker follows the control-centre volume. Racing plays an engine that follows speed, off-road rumble, boost, contact, overtaking, lap and finish sounds; Marbles plays rolling noise that follows ball speed, wall knocks and checkpoint chimes. Both play countdown, start and menu sounds. Volume 0 switches the amplifier off.

For nearby play, both badges run this version and choose the same mode and map; nearby races have no computer rivals. Both badges select Versus and calibrate, then tap Ready. The two compatible nearby lobbies automatically pin each other and show the same room code. Start, readiness and acknowledgement fields repeat in every state packet; positions travel at about 15 Hz. Peers are limited to two. The first badge to reach the finish wins: the other badge ends its race at once, shows Defeat - 2nd with the winner's time and plays a defeat sound, while the winner shows Victory! 1st. The losing badge marks its packets as conceded, so the winner never reads that stop as a faster finish. When both badges really finish, the lower time wins and an exact tie goes to the host.

Opening the menu, opening system controls or losing fresh motion pauses the local simulation and advertises pause to the peer. A peer observes that pause after a received packet. After 1.5 seconds without a valid peer packet, play freezes and reports disconnection. The same live session can resume when traffic returns; a restarted badge requires both players to begin a fresh lobby. Restarting a finished match requires both players to choose Play again.

ESP-NOW uses CONFIG_RACE_RADIO_CHANNEL, default 6 and configurable from 1 to 11. Nearby play requires the same channel and starts Wi-Fi only while its lobby or match is open. Pairing uses fresh nonces and pinned MAC addresses but provides no cryptographic authentication or application encryption. Radio failure leaves single-player available. Two-device radio behavior, timing fairness and physical gestures need hardware acceptance.

<a id="build-and-check"></a>
## Build and check

Activate ESP-IDF 5.5.4 with its ESP32-S3 toolchain. From the repository root:

```sh
idf.py -C plays/sugarat-race -B /tmp/sugarat-race-build build
python3 plays/sugarat-race/tests/check_host.py --output /tmp/sugarat-race-host
```

The application image is /tmp/sugarat-race-build/sugarat_race.bin. Host checks use Clang, AddressSanitizer, UndefinedBehaviorSanitizer and Pillow to exercise all six maps, three-lap finishes, rival races with boost pads, road departure, flat and upright tilt calibration, marble rolling, maze collision and completion, cached maze frames, the sound peak limit, lossy start, stale traffic and replay rejection. Every C reference frame must match the frozen preview pixels. Host checks do not establish radio or physical-device behavior.

<a id="maps-and-rendering"></a>
## Maps and rendering

The original maps, RGB565 backgrounds and pixel sprites are defined by design/maps.json and main/race/generated.h. The portable C renderer is shared by desktop reference images and firmware. The first reference board is [racing tracks](design/previews/racing-tracks-board.png); the second is [marble maps](design/previews/marble-maps-board.png). This implementation does not import Tilt-a-tron or AmoledOS game code or art.

One worker owns physics and session state. Three PSRAM frame buffers exchange ownership under a short mutex; the rendering buffer is never displayed before publication. Static scenes render only when their page, mode, map or phase changes. The static maze layer is cached in PSRAM, and filled rectangles and discs clip each row to the round panel once. The IMU task runs above the display tasks so tilt keeps its 100 Hz rate. Menus, countdowns and system overlays use LVGL; unobscured gameplay can use the bundled direct strip sender. An unusable stream falls back to LVGL. Stale sensor data freezes gameplay and shows a recovery message.

The Game component owns menu controls and fonts. Map names and the maze hint are English labels generated by design/generate_reference.py. The serial diagnostic keys a/b tap the lower/upper large button, [/ ] swipe left/right, and m holds the center. These paths feed the actual BSP input driver. The BSP s command captures the current LVGL screen. Synthetic input and screenshots do not validate a physical finger, IMU motion or panel appearance.

<a id="device-installation"></a>
## Device installation

Follow the [hardware deployment procedure](../../../hardware-kits/sugarat-sdgoods/references/deployment.md). Inspect the current chip, flash, partition table and selected app before writing. A successful build is not permission to replace a launcher. Install only the application BIN at an address established from that inspection; preserve NVS and appdata. Skipping a backup requires explicit user instruction.

The application retains the BSP shell, control center, power behavior and managed-app exit hooks. It does not modify partition tables or select OTA slots. A standalone write does not establish launcher installation compatibility. Refer to validation.json for the actual write, byte verification, startup and unverified acceptance items.
