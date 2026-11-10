#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate three original, quiet 8-second PCM compositions without external media."""
from pathlib import Path
import argparse
import math
import struct
import wave

RATE, SECONDS = 16000, 8
MELODIES = [
    [60, 67, 64, 72, 69, 64, 67, 62, 65, 69, 72, 67, 64, 62, 67, 60],
    [62, 69, 65, 74, 72, 65, 69, 67, 64, 71, 67, 76, 74, 67, 65, 62],
    [57, 64, 60, 67, 69, 60, 64, 62, 55, 62, 59, 65, 67, 59, 62, 57],
]


def main():
    """Write deterministic 16 kHz mono WAV files, with sample peaks below 7000."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--format", choices=["c", "wav"], default="c")
    args = parser.parse_args()
    directory = args.output
    generated = {}
    compositions = []
    for track, notes in enumerate(MELODIES):
        samples = []
        for i in range(RATE * SECONDS):
            t = i / RATE
            beat = min(15, int(t * 2))
            local = t - beat * .5
            frequency = 440 * 2 ** ((notes[beat] - 69) / 12)
            envelope = min(1, local / .012, (.5 - local) / .04) * math.exp(-5 * local)
            note = math.sin(2 * math.pi * frequency * local) + .2 * math.sin(4 * math.pi * frequency * local)
            bass = math.sin(2 * math.pi * (130.8128 if track != 2 else 110) * t) * .12
            fade = min(1, t / .04, (SECONDS - t) / .25)
            samples.append(round((note * envelope * .8 + bass) * fade * 6500))
        assert max(abs(x) for x in samples) < 7000
        compositions.append(samples)
    if args.format == "wav":
        import io
        for track, samples in enumerate(compositions):
            buffer = io.BytesIO()
            with wave.open(buffer, "wb") as audio:
                audio.setnchannels(1); audio.setsampwidth(2); audio.setframerate(RATE)
                audio.writeframes(struct.pack(f"<{len(samples)}h", *samples))
            generated[f"track-{track}.wav"] = buffer.getvalue()
    else:
        generated["media_tracks.h"] = (
            '/* Generated original demo PCM. */\n#pragma once\n#include <stdint.h>\n#include <stddef.h>\n'
            '#define MEDIA_TRACK_COUNT 3\ntypedef struct { const int16_t *samples; size_t count; } media_track_t;\n'
            'extern const media_track_t media_tracks[MEDIA_TRACK_COUNT];\nextern const uint32_t media_durations_ms[MEDIA_TRACK_COUNT];\n').encode()
        arrays = [f"static const int16_t track_{track}[] = {{\n" +
                  "\n".join("    " + ",".join(str(n) for n in samples[i:i + 24]) + "," for i in range(0, len(samples), 24)) + "\n};"
                  for track, samples in enumerate(compositions)]
        generated["media_tracks.c"] = ('#include "media_tracks.h"\n' + "\n".join(arrays) +
            '\nconst media_track_t media_tracks[MEDIA_TRACK_COUNT] = {' +
            ','.join(f'{{track_{i},{len(samples)}}}' for i, samples in enumerate(compositions)) + '};\n' +
            'const uint32_t media_durations_ms[MEDIA_TRACK_COUNT] = {8000,8000,8000};\n').encode()
    conflicts = [name for name, data in generated.items()
                 if (directory / name).exists() and (directory / name).read_bytes() != data]
    if conflicts:
        raise SystemExit("Refusing to replace custom audio files: " + ", ".join(conflicts))
    directory.mkdir(parents=True, exist_ok=True)
    for name, data in generated.items():
        (directory / name).write_bytes(data)
    print("Generated 3 original 8-second tracks; 16 kHz mono PCM; peaks below 7000")


if __name__ == "__main__":
    main()
