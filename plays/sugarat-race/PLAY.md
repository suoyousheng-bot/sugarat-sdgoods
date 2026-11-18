# Round Race

Tilt-controlled kart racing against three rivals and a rolling marble maze, with sound. Race a friend's badge nearby; no router or internet needed.

Community play: Sugarat 玩法社区 #30 (`sugarat_race`), published version 0.2.1.
Source version in this directory: 1.0.0.

This source is newer than the community release 0.2.1; build it to get 1.0.0.

## About the play

# Round Race

Two tilt-controlled games for the Sugarat round screen: a three-lap kart race and a marble maze. Play alone or race a friend's badge nearby without a router or internet.

- **Tilt Racer**: tilt left or right to steer while the car accelerates by itself. Three laps on Coast Cruise, Red Canyon or Neon Loop. In Solo you start behind three computer rivals; hit the orange chevron boost pads and pass them, but leaving the road or touching a rival costs speed.
- **Marble Run**: tilt in any direction to roll a marble through Jade Garden, Amber Ruins or Ice Corridor. The ball rolls with gravity and bounces off walls; collect three gold checkpoints, then reach the checkered finish.
- **Any hold works**: lie the badge flat, hold it upright or anywhere between, and tap Start here; that posture becomes level.
- **Versus**: both badges pick the same mode and map, tap Versus and Ready. The first badge across the line wins; the other ends at once with Defeat - 2nd and the winner's time.
- **Sound**: engine, boost, contact, overtake, lap, rolling marble, wall knocks, checkpoint chimes, countdown and win or defeat tunes, following the control-centre volume.
- English interface. Times are not saved.

See [play instructions and limits](README.md#play).

## Requirements

SDGoods ESP32-S3 round badge with QMI8658 and 8 MB PSRAM; the speaker is optional. Versus needs two badges running this firmware on the same ESP-NOW channel (default 6).

## Make your own (做同款)

1. Download the attachment `sugarat-race.tar.gz` of release `v0.3.0`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.0/sugarat-race.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-race --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_race.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
