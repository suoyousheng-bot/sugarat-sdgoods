---
description: "Play and build Sugarat Beat for the SDGOODS round badge."
---

# Sugarat Beat

English | [中文](README.zh.md)

## Summary

Sugarat Beat is an offline rhythm jumper for the SDGOODS ESP32-S3 round badge. One chart follows the original 30-second electronic track Kinetic Circuit at 120 BPM. A two-beat count-in precedes 18 hazards.

## Contents

- [Play](#play)
- [Build](#build)
- [Verification](#verification)

## Play

Tap to start. The spiny sea urchin hops automatically. When TAP NOW appears, tap just before, on, or just after the landing beat to make a longer hop over the marked gap. Green light marks the launch platform, and a short character chirp plays on a long jump. After a miss, tap once to start again. Swipe down from the top to open the control center and adjust volume. Closing the control center resumes a paused run.

## Build

With ESP-IDF 5.5.4 active, run the following from this folder. The build produces the single application image `build/sugarat_beat.bin` with the soundtrack embedded. Reproduce the soundtrack with Python 3 if you edit the [composition](tools/compose.py).

```sh
python3 tools/compose.py --wav kinetic-circuit.wav
idf.py -B build build
```

The project derives from the bundled [SDGOODS foundation](SUGARAT_TEMPLATE.md). The chart measures time from completed I2S DMA blocks, including while the speaker is muted.

## Verification

The host checks exercise every hazard, a missed beat, retry, and LVGL touch input with a simulated speaker clock. The v0.1.3 flash contents were verified on a Sugarat round badge. The game booted to the title screen, and the author playtested it and confirmed the tap timing. See [VALIDATION.md](VALIDATION.md).
