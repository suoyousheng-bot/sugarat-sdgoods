# lz Real-Time 3D Character

The 3D character lz rendered in real time on the round screen: drag to turn, tap to say hello, with an intro animation and sound at boot.

Community play: Sugarat 玩法社区 #22 (`sugarat-play-3`), published version 1.0.0.
Source version in this directory: 1.0.0.

## About the play

## About

lz is a 3D character rendered in real time on the SDGOODS round-screen badge: an 800-triangle model, a 256×256 texture and skeletal animation at about 30 frames per second. Every boot first plays an intro animation with sound, then transitions to the 3D character.

## Controls

Drag left or right to turn a full 360°, with inertia after you let go; drag up or down to tilt, which springs back on release; tap to play the hello, listen and talk animations in turn; swipe down from the top to open the control center.

## Firmware file

The release file is an ESP-IDF application image (2,884,656 bytes). The bootloader, partition table and boot selection are shared SDGOODS platform components supplied by the installer. Installing through the multi-app launcher needs 2.81 MiB of application space.

## Verification status

Runs as single-app firmware on one device; the boot animation, touch turning and animation playback work. Installing as a launcher app and returning to the launcher are still to be confirmed by testing.

## Requirements

SDGOODS ESP32-S3 360×360 round touch-screen badge with 32 MiB flash and 8 MiB PSRAM. No network required. Only the application image is provided; the shared bootloader, partition table and boot selection are supplied by the installer.

## Make your own (做同款)

1. Download the attachment `lz-3d.tar.gz` of release `v0.3.0`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.0/lz-3d.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id lz-3d --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/lz_3d.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
