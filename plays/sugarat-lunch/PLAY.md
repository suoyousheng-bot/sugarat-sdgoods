# Lunch Shuffle

Shake your badge to pick lunch from ten pixel-art meals, with an 8-bit chime when the result appears.

Community play: Sugarat 玩法社区 #35 (`sugarat_lunch`), published version 0.1.6.
Source version in this directory: 0.1.6.

## About the play

# Lunch Shuffle

Shake your badge to pick one of ten lunches. Pixel-art dishes roll by with slowing 8-bit ticks, then a short chime reveals the result.

After the reveal, shake again or tap anywhere on the screen for a different dish. The next draw leaves out the previous result. Adjust the volume in the badge's Control Center.

The meal list, artwork, and sounds are built in. The play runs offline.

## Requirements

Requires a Sugarat-compatible SDGOODS ESP32-S3 round badge with a QMI8658 motion sensor and touchscreen.

## Make your own (做同款)

1. Download the attachment `sugarat-lunch.tar.gz` of release `v0.3.1`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.1/sugarat-lunch.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-lunch --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_lunch.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
