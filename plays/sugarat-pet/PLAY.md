# Pocket Companion

Raise Ember, a pixel companion, on the round screen: feed, play, pet, clean and put it to sleep. The more time you spend together, the more it grows.

Community play: Sugarat 玩法社区 #27 (`sugarat_pet`), published version 0.1.0.
Source version in this directory: 0.2.2.

This source is newer than the community release 0.1.0; build it to get 0.2.2.

## About the play

# Pocket Companion

Raise Ember, a pixel companion, on the round touch screen.

## How to play

- **Tap Ember**: pet it
- **Feed**: feed it; it refuses when it is full
- **Play**: play together; uses energy and lifts its mood
- **Sleep / Wake up**: switch to the night scene to sleep; Ember wakes up on its own once rested
- **Stats**: view the five stats (hunger, mood, energy, cleanliness, health) and the current level
- **Clean**: clean up droppings

## Growth

Ember grows only while you keep it company in the foreground; each level takes 30 minutes. Levels 1–10 are Baby, 11–30 Growing, and 31–50 Adult. Growth pauses while you are away from the app or the control center is open, so Ember never starves while the device is off. Neglect lowers health to a recoverable floor of 20; this version has no death.

## Saving

Progress is stored in a dedicated NVS save with two checksummed records. It saves automatically after actions, and also before returning to the launcher and before power-off. A sudden power loss may lose progress made since the last successful save.

Fully offline with an English UI. The care logic is based on the MIT-licensed epet; Ember is an original pixel character.

## Requirements

A Sugarat device (SDGoods ESP32-S3 round screen) with a 360×360 touch screen. No network or extra resource package required.

## Make your own (做同款)

1. Download the attachment `sugarat-pet.tar.gz` of release `v0.3.0`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.0/sugarat-pet.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-pet --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_pet.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
