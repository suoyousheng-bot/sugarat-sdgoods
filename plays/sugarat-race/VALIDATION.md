# Round Race validation

Recorded results for the community release. They describe the recorded build and devices only; a rebuilt or modified image needs its own verification.

## Community release notes

v0.2.1 passed host sanitizer checks for three-lap races, rival races with boost pads, flat and upright tilt calibration, marble rolling and maze collisions, cached maze frames, the sound peak limit, lossy peer start, replay rejection and the first-finisher win rule; the six reference frames match pixel for pixel. Both SDGoods devices were flashed in single-app mode; write hashes verified, 0.2.1 booted with the speaker ready and the IMU at 100 Hz, rendering about 20 fps in racing and 25 fps in the maze. The user played Solo racing and the marble maze, and briefly tested a two-device Versus race, and reported no problems. Launcher installation/Exit and battery life were not tested.

## Machine-readable record

See [`validation.json`](validation.json).
