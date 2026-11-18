# Sugarat Beat

Hop across a monochrome course as a spiny sea urchin. Tap when the launch pad glows to clear gaps in time with the music.

Community play: Sugarat 玩法社区 #36 (`sugarat_beat`), published version 0.1.3.
Source version in this directory: 0.1.3.

## About the play

Sugarat Beat is an offline rhythm jumper for the Sugarat round badge. The sea urchin bounces across dark cylinders to Kinetic Circuit, an original 30-second electronic track at 120 BPM. Green light marks the moment to tap. A long jump clears the marked gap and gives the urchin a short chirp.

Tap to start. After the two-beat count-in, tap around the landing beat when TAP NOW appears to jump over the next gap. The course has 18 hazards. If you miss, tap once to start over. Swipe down from the top to open the volume control.

## Requirements

Sugarat round badge (SDGOODS ESP32-S3, 360 × 360 touchscreen, 32 MiB flash). The game runs offline and uses the built-in speaker.

## Make your own (做同款)

1. Download the attachment `sugarat-beat.tar.gz` of release `v0.3.2`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.2/sugarat-beat.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-beat --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_beat.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
