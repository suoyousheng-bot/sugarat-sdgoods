# Sugarat Voice Recorder

Record voice notes and play them back on your round-screen Sugarat badge, entirely offline.

Community play: Sugarat 玩法社区 #38 (`sugarat_recorder`), published version 0.1.4.
Source version in this directory: 0.1.4.

## About the play

Tap the microphone to record. Hold the stop button for 1.2 seconds to save, or let the recorder stop automatically at 90 seconds. The saving screen shows the clip duration and an animated waveform. Wait for **Recording saved** before powering off.

Swipe up and open **Recordings** to play or manage your clips. Press and hold a recording to delete it after confirmation. New recordings are stored locally as mono WAV files.

## Requirements

A Sugarat SDGOODS ESP32-S3 round-screen badge with a writable appdata partition. No internet connection is required to record or play audio.

## Make your own (做同款)

1. Download the attachment `sugarat-recorder.tar.gz` of release `v0.3.3`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.3/sugarat-recorder.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-recorder --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_recorder.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
