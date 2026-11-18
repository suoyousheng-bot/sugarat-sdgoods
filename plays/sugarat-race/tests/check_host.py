#!/usr/bin/env python3
"""Exercise shipped C physics and protocol; compare all six frames with the approved preview pixels."""
import argparse
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageChops


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='race-check-', dir=out) as temporary:
        build = Path(temporary)
        source = root / 'main/race'
        flags = ['clang', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-ffp-contract=off', '-fsanitize=address,undefined', '-I', str(source)]
        subprocess.run(flags + [str(source / f) for f in ['renderer.c', 'game.c', 'link.c', 'synth.c']] + [str(root / 'tests/game_host.c'), '-lm', '-o', str(build / 'game-tests')], check=True)
        subprocess.run([str(build / 'game-tests')], check=True, timeout=120)
        subprocess.run(flags + [str(source / 'renderer.c'), str(root / 'tests/render_host.c'), '-lm', '-o', str(build / 'render-host')], check=True)
        for mode in range(2):
            for index in range(3):
                name = f'{"road" if mode == 0 else "maze"}-{index}-360'
                ppm = build / (name + '.ppm')
                subprocess.run([str(build / 'render-host'), str(mode), str(index), str(ppm)], check=True)
                expected = Image.open(root / 'design/previews' / (name + '.png')).convert('RGB')
                actual = Image.open(ppm).convert('RGB')
                if ImageChops.difference(expected, actual).getbbox() is not None:
                    raise AssertionError('Frozen preview pixels changed: ' + name)
                actual.save(out / (name + '.png'))
        print('PASS: all six reference frames match the frozen preview pixels exactly')


if __name__ == '__main__':
    main()
