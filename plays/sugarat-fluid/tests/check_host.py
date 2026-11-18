#!/usr/bin/env python3
"""Build the real solver and LVGL view with sanitizers, check motion, and render four themes."""
import argparse
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = options.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    build = out / 'build'
    subprocess.run(['cmake', '-S', str(root / 'tests/host'), '-B', str(build), '-G', 'Ninja'], check=True)
    subprocess.run(['cmake', '--build', str(build), '-j', '6'], check=True)
    subprocess.run([str(build / 'fluid-frames')], check=True)
    subprocess.run([str(build / 'fluid-link')], check=True)
    subprocess.run([str(build / 'fluid-host'), str(out)], check=True)
    assert (out / 'ui-text.txt').read_text() == (root / 'tests/expected/ui-text.txt').read_text(), 'UI text changed; review the owning view and snapshot.'
    overview = Image.new('RGB', (760, 760), '#171a20')
    mask = Image.new('L', (360, 360))
    ImageDraw.Draw(mask).ellipse((0, 0, 359, 359), fill=255)
    for theme in range(4):
        frame = Image.open(out / f'theme-{theme}.ppm').convert('RGB')
        frame.save(out / f'theme-{theme}.png')
        overview.paste(frame, (10 + theme % 2 * 380, 10 + theme // 2 * 380), mask)
    overview.save(out / 'overview.png')
    print('PASS: owner-local UI text snapshot; preview:', out / 'overview.png')


if __name__ == '__main__':
    main()
