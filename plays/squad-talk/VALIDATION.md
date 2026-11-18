# Squad Talk validation

Recorded results for the community release. They describe the recorded build and devices only; a rebuilt or modified image needs its own verification.

## Community release notes

Version 0.1.3: one SDGOODS ESP32-S3 badge updated with application-only flashing and byte verification; startup and audio/ESP-NOW initialization confirmed. Device framebuffer screenshots confirm the new UI. An on-device synthetic right swipe confirms CH 01 to CH 08 wraparound; this bypasses the physical touch panel. Real-finger swipe feel, dual-device voice, range and launcher installation have not been re-tested for this version. Host checks cover the codec/session, 200 alternating turns, both swipe directions and wraparound, ellipse hit testing, speech/swipe isolation, rejected short/diagonal/top-edge drags, cancellation, eight English UI states and small audio-level redraws. Version 0.1.2 has prior user-confirmed continuous bidirectional voice acceptance.

## Machine-readable record

See [`validation.json`](validation.json).
