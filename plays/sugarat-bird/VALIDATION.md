# Pocket Bird validation

Recorded results for the community release. They describe the recorded build and devices only; a rebuilt or modified image needs its own verification.

## Community release notes

v0.1.0 was flashed to 0x10000 on a real device in single-app mode; the write hash and an independent byte-by-byte comparison passed, the startup log was normal, and no fatal errors appeared during 10 seconds of observation. On the host, collision and scoring, 45 consecutive pipes, the difficulty cap, best-score saving and failures, touch input, and overlay and sleep pauses are covered under ASan/UBSan. Launcher installation, returning, and coexistence with other apps have not yet been verified on a device.

## Machine-readable record

See [`validation.json`](validation.json).
