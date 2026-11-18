---
description: "Build and use Squad Talk, an English ESP-NOW push-to-talk application for two SDGOODS round badges."
---

# Sugarat Squad Talk

English | [中文](README.zh.md)

## Summary

Squad Talk connects two SDGOODS ESP32-S3 round badges directly over ESP-NOW. Choose the same channel, wait for `BUDDY ONLINE`, hold `HOLD TO TALK`, and release to listen. The English cream/lavender interface uses a large fingerprint hold target, rounded bold text and state-specific fills. The application needs no router, phone, account or server. [validation.json](validation.json) separates firmware build, host tests and device acceptance.

## Contents

- [Use](#use)
- [Build](#build)
- [Host checks](#host-checks)
- [Communication and failures](#communication-and-failures)
- [Hardware and sources](#hardware-and-sources)
- [Dev Note](#dev-note)

## Use

Install the same application version on both badges. Swipe left on the screen background for the next logical channel and right for the previous one; channels 01–08 wrap around and each boot starts on 01. Start outside the fingerprint button and below the top control-center strip. Each swipe changes one channel on release; taps, short drags and predominantly vertical movement do not change channels. The first matching peer is selected automatically and remains selected until it leaves or the channel changes. This release supports one pair per group. `FIND BUDDY` means that no matching device has responded; it does not indicate a radio connection.

Hold the fingerprint button to request a turn. `CALLING...` waits for the peer's acknowledgement; `RELEASE TO LISTEN` indicates transmission. The receiver displays `LISTENING...` and plays the incoming speech. Both devices require a fresh press after a collision, timeout or paused conversation. A lower device address wins simultaneous requests. Each turn is limited to 30 seconds; a missing end packet releases the receiver after 600 ms without audio. Discovery expires after 3 seconds without a valid peer packet.

A drag that starts on the fingerprint button never changes channels; moving outside its ellipse cancels speech until a fresh press. A lost touch, opening the control center, screen suspension, or application exit releases transmission. Closing the control center or waking never resumes an old held press. Top-edge swipe retains the platform's volume, brightness and Exit/Power controls. Fatal audio or radio errors disable speech and display `RESTART APP`; tapping the button restarts the firmware. The fingerprint is a touch illustration; it does not scan or identify fingerprints.

## Build

Activate an existing ESP-IDF 5.5.4 environment with the ESP32-S3 toolchain, then run from the resource-kit root:

```sh
idf.py -C plays/squad-talk -B /tmp/squad-talk-build build
```

Deliver only `/tmp/squad-talk-build/squad_talk.bin`. The project name `squad_talk` is the stable launcher identity; application version is 0.1.3. Preserve the platform partition table, bootloader configuration, power latch and launcher Exit entry. Device inspection must establish the actual writable application address before flashing. The application never changes partitions, otadata or another application's data.

## Host checks

With CMake, Ninja, a C compiler and Pillow available, run:

```sh
python3 plays/squad-talk/tests/check_host.py --output /tmp/squad-talk
```

The command runs the real protocol, independent ADPCM blocks, LVGL view and touch controller under AddressSanitizer and UndefinedBehaviorSanitizer. Two simulated peers exercise 200 alternating turns, simultaneous requests, group filtering, duplicate packets, early release, peer expiry and malformed packets. Real LVGL pointer events check hold, release, drag-out, swipe directions, channel wraparound, rejected short/diagonal/top-edge drags, speech/swipe isolation and overlay/sleep cancellation. The owner-local [English output snapshot](tests/expected/ui-text.txt) covers eight states. The exported `preview-en.png` renders the actual view; it is not a device screenshot or evidence of wireless range, audio quality or end-to-end latency.

## Communication and failures

All logical groups share radio channel 6. Firmware initializes the Wi-Fi radio in station mode without joining an access point; it does not start a hotspot or IP server. Discovery uses broadcasts; speech and turn-control messages use the selected peer's address. Logical grouping filters application packets and does not encrypt speech. This build is for a nearby demonstration pair, not confidential communications or a guaranteed coverage distance.

Speech uses independently decodable 16 ms, 16 kHz mono IMA ADPCM blocks with a 24-byte versioned header. Packet lengths, version, group, codec state, peer boot identity and conversation identity are validated before playback. Duplicate or late sequence numbers are dropped; a lost block does not corrupt subsequent decoder state. The radio worker serializes sends; microphone and Wi-Fi callbacks only copy bounded data into queues. Full audio queues drop audio, while release/pause input uses separately synchronized state. A missing send completion stops the radio instead of associating a late completion with a new packet.

The application-local audio component buffers 64 ms before playback, waits at most 80 ms for a short burst, and buffers 48 ms after an underrun. One playback writer awaits audio-task acknowledgement when discarding a previous conversation. Startup warms the microphone once; it only transmits captured audio while the peer has granted a held turn. Speech peaks are limited to the documented badge speaker range. Audio playback (priority 8), microphone capture (9) and radio delivery (7) run on CPU 1, above the CPU 0 UI (6). Level changes redraw only the activity bars; the fingerprint target stays static. During a turn, `FLOW` logs report transmitted blocks, cumulative receive/loss/drop counts, queued samples and maximum worker-loop gap. These settings require dual-device acoustic and range acceptance. Wi-Fi credentials are neither requested nor stored; channel selection is volatile and no audio is saved.

## Hardware and sources

The project is initialized from the verified local SDGOODS kit 0.2.1: ESP-IDF 5.5.4, LVGL 8.3.11, 360×360 ST77916, CST816 touch, 32 MiB Flash and 8 MiB Octal PSRAM. The microphone and speaker retain their BSP I2S wiring. This application adapts the bundled Game controls with a cream/lavender/lime palette, a 220×350 px elliptical fingerprint target whose lower half extends beyond the screen and Nunito Black English glyphs. Text fits the physical circular display; pointer tests distinguish the ellipse from its rectangular corners. [Nunito's OFL license](assets/fonts/OFL.txt) covers the bundled source and generated subsets; regenerate them with `python3 plays/squad-talk/tools/make_ui_fonts.py`. [upstream.json](upstream.json) pins the inspected atomic14 walkie-talkie reference; [its MIT notice](licenses/atomic14-MIT.txt) and the foundation notices remain distributed. The two-peer session protocol, independent codec blocks and hardware adaptation are maintained in this application.

Follow the kit's [deployment instructions](../../../hardware-kits/sugarat-sdgoods/references/deployment.md). A full private backup is skipped only with the user's explicit instruction. Launcher installation, standalone flashing, written-byte verification, startup, touch, microphone, speaker, half-duplex conversation, range and power behavior are separate acceptance results. A successful firmware build or simulated conversation does not establish device behavior.

## Dev Note

None.
