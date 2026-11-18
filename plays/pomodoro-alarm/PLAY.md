# Pomodoro Alarm

Focus for 25 or 45 minutes, then take a 5-minute break. A round-screen countdown, automatic phase switching and an audible reminder help you keep a steady rhythm.

Community play: Sugarat 玩法社区 #34 (`pomodoro_alarm`), published version 1.1.0.
Source version in this directory: 1.1.0.

## About the play

# Pomodoro Alarm

A focus timer for the Sugarat round-screen badge, with an audible reminder when a focus or break phase ends.

## How to use

- Choose a 25-minute or 45-minute focus session, or a 5-minute break while the timer is stopped.
- Tap Start to begin, Pause to stop temporarily, and Start again to continue.
- Tap Reset to return the current phase to its full duration.
- Follow the large countdown and edge-to-edge progress ring. The screen also shows the phase and completed-focus count.
- At phase completion, the application shows a notice, requests an approximately 6-second audio reminder and starts the next phase automatically. Focus changes to a 5-minute break; a break changes back to the selected focus duration.
- Tap the screen to stop a currently playing reminder. The controls also stop it when used.

## Before you start

The interface is English. The application runs locally without Wi-Fi. It uses the badge's shared audio player for its reminder and restores the previous volume after playback. A generated illustration represents this play's community cover.

Timer progress and completed-focus counts are held in memory and reset after an application restart. This application is not a wake-from-sleep alarm. Physical audio behavior and launcher coexistence have not been independently verified for this publication; see the version's testing information.

## Requirements

Sugarat / SDGOODS ESP32-S3 badge with a 360×360 round touch screen and the built-in speaker. English interface. No Wi-Fi or external accessory is required for the timer. Keep the device powered; this is not a wake-from-sleep alarm.

## Make your own (做同款)

1. Download the attachment `pomodoro-alarm.tar.gz` of release `v0.3.4`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.4/pomodoro-alarm.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id pomodoro-alarm --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/pomodoro_alarm.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
