# Magic Bottle validation

Recorded results for the community release. They describe the recorded build and devices only; a rebuilt or modified image needs its own verification.

## Community release notes

v0.2.1 passed host sanitizer checks for the real solver, pour stream, menus and two session engines over lossy/reordered packets. Both SDGoods devices were flashed in single-app mode; write hashes verified, 0.2.1 booted with active IMU and 30 fps direct presentation (render 12 ms max). The user played on both devices, including the menu buttons, fill gauge, mouth cues and two-device pouring, and reported no problems. Launcher installation/Exit and battery life were not tested.

## Machine-readable record

See [`validation.json`](validation.json).
