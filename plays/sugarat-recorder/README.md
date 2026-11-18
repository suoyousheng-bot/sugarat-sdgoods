# Sugarat Voice Recorder

Record voice notes and play them back on your round-screen Sugarat badge, entirely offline. See [PLAY.md](PLAY.md) for controls and the source download.

## Build the firmware

Activate ESP-IDF 5.5.4 and run `idf.py build` from this directory. The target is ESP32-S3 and the application image is `build/sugarat_recorder.bin`.

The recorder saves 16 kHz, 16-bit mono WAV files in the shared appdata partition. It requires a writable, formatted appdata volume.

## Test the screen on a desktop

The test uses LVGL and the badge's touch gesture code with simulated audio and files. Run these commands from this directory.

```sh
cmake -S tests/host -B /tmp/sugarat-recorder-host
cmake --build /tmp/sugarat-recorder-host
/tmp/sugarat-recorder-host/recorder-host /tmp/sugarat-recorder-host
```

See [VALIDATION.md](VALIDATION.md) for the verification scope.
