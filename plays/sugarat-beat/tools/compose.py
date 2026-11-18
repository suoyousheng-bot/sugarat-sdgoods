"""Render the original Kinetic Circuit soundtrack as 16 kHz mono PCM and WAV."""

import argparse
import math
import random
import struct
import wave
from pathlib import Path

RATE = 16000
BEAT = RATE // 2
FRAMES = 30 * RATE
TAU = math.tau


def compose():
    mix = [0.0] * FRAMES
    noise = random.Random(127)

    def voice(start, length, sample):
        for i in range(min(int(length * RATE), FRAMES - start)):
            mix[start + i] += sample(i / RATE)

    def synth(start, midi, length, gain, octave=0):
        hz = 440 * 2 ** ((midi + octave * 12 - 69) / 12)

        def sample(t):
            envelope = min(t / .007, 1, (length - t) / .043)
            phase = TAU * hz * t
            return gain * envelope * (
                math.sin(phase) + .27 * math.sin(phase * 2) + .11 * math.sin(phase * 3)
            )

        voice(start, length, sample)

    roots = (45, 41, 48, 43)
    phrases = (
        (72, 76, 79, 74, 76, 72, 69, 71),
        (72, 77, 81, 76, 77, 72, 69, 72),
        (76, 79, 84, 79, 74, 76, 72, 76),
        (74, 79, 83, 81, 79, 76, 74, 71),
    )

    for beat in range(60):
        at = beat * BEAT
        voice(at, .26, lambda t: .83 * math.exp(-t * 19)
              * math.sin(TAU * (53 * t + .86 * (1 - math.exp(-t * 93)))))
        if beat % 2:
            voice(at, .14, lambda t: .31 * math.exp(-t * 32)
                  * (.73 * noise.uniform(-1, 1) + .27 * math.sin(TAU * 184 * t)))
        for offbeat in (BEAT // 2, BEAT * 3 // 4):
            voice(at + offbeat, .046, lambda t: .13 * math.exp(-t * 95)
                  * noise.uniform(-1, 1) * math.cos(TAU * 5160 * t))
        if beat < 2:
            synth(at, 88 if beat else 81, .14, .19)
        else:
            bar, step = divmod(beat - 2, 4)
            chord = bar % len(roots)
            for half, transpose in enumerate((0, 12 if step >= 2 else 7)):
                synth(at + half * BEAT // 2, roots[chord] + transpose, .20, .25)
            for half in range(2):
                note = phrases[chord][step * 2 + half]
                offset = at + half * BEAT // 2
                synth(offset, note, .19, .20)
                synth(offset + 3 * BEAT // 4, note, .14, .054)

    pcm = bytearray()
    for index, value in enumerate(mix):
        fade = min(1, index / 32, (FRAMES - index) / 4800)
        pcm.extend(struct.pack("<h", round(8600 * math.tanh(value * 1.35) * fade)))
    return pcm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wav", type=Path, help="also write a listenable WAV")
    args = parser.parse_args()
    pcm = compose()
    path = Path(__file__).resolve().parents[1] / "main/music/kinetic-circuit.pcm"
    path.write_bytes(pcm)
    if args.wav:
        with wave.open(str(args.wav), "wb") as output:
            output.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
            output.writeframes(pcm)


if __name__ == "__main__":
    main()
