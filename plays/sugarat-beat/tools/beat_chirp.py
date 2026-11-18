"""Synthesize the mascot's original jump chirp for the ESP32 mixer."""

import math
import sys
from pathlib import Path

RATE = 16000
LENGTH = .136


def samples():
    phase = 0.0
    for index in range(round(RATE * LENGTH)):
        time = index / RATE
        ratio = time / LENGTH
        frequency = 420 + 330 * math.sin(math.pi * ratio) + 220 * ratio
        phase += math.tau * frequency / RATE
        envelope = min(1.0, time / .009) * (1.0 - ratio) ** 1.45
        shape = math.sin(phase) + .58 * math.sin(2 * phase) + .24 * math.sin(3 * phase)
        flutter = .85 + .15 * math.sin(math.tau * 27 * time)
        yield round(4450 * envelope * shape * flutter)


def main():
    pcm = list(samples())
    rows = [", ".join(str(value) for value in pcm[i:i + 16]) + "," for i in range(0, len(pcm), 16)]
    Path(sys.argv[1]).write_text(
        '#include <stdint.h>\n\n'
        'const int16_t beat_chirp[] = {\n    ' + '\n    '.join(rows) + '\n};\n'
        'const uint32_t beat_chirp_frames = sizeof(beat_chirp) / sizeof(beat_chirp[0]);\n'
    )


if __name__ == "__main__":
    main()
