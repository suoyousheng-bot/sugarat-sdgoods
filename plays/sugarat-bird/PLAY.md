# Pocket Bird

Tap to flap and guide a pixel bird through the pipes: the speed rises every 5 points, and your best score is saved automatically.

Community play: Sugarat 玩法社区 #28 (`sugarat_bird`), published version 0.1.0.
Source version in this directory: 0.1.0.

## About the play

# Pocket Bird

A pixel-style pipe-dodging mini game on the round screen.

## How to play

- Press **Play** to start, then **tap the screen** to flap; holding counts as a single flap
- Each pipe you pass scores 1 point; every 5 points raises the speed by one level, up to level 8
- Hitting a pipe, the top, or the ground ends the run; **Try again** starts a new one right away
- Your best score (up to 9999) is saved automatically and survives restarts

## Details

- Swiping down from the top edge opens the control center and pauses the game; tap **Resume** after closing it. The game also pauses when the screen sleeps, so the bird never falls while you are away
- Physics runs on a fixed time step, so flight feels the same regardless of frame rate
- Fully offline, silent, with an English UI

Flight and pipe rules are adapted from the Apache-2.0-licensed SDGOODS Flappy example; the pixel bird, round-screen UI, and saving are maintained by Sugarat.

## Requirements

A Sugarat device (SDGoods ESP32-S3 round screen) with a 360×360 touch screen. No network or extra resource package required.

## Make your own (做同款)

1. Download the attachment `sugarat-bird.tar.gz` of release `v0.3.0`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.0/sugarat-bird.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-bird --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_bird.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
