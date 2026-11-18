# Pocket Companion validation

Recorded results for the community release. They describe the recorded build and devices only; a rebuilt or modified image needs its own verification.

## Community release notes

v0.1.0 was flashed to a real device in single-app mode; the write hash and independent verification passed. Startup, serial response and a screen capture were normal, with no crash in 18 seconds. On the host, care, sleep, growth, save validation and failures, save on exit, and touch input are covered under ASan/UBSan. Launcher installation, returning, and coexistence with other apps have not yet been verified on a device.

## Machine-readable record

See [`validation.json`](validation.json).
