#!/usr/bin/env python3
"""Build real Pocket Bird/LVGL with sanitizers, exercise input and render English screens."""
import argparse
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
build = out / 'build'
subprocess.run(['cmake', '-S', str(root / 'tests/host'), '-B', str(build), '-G', 'Ninja'], check=True)
subprocess.run(['cmake', '--build', str(build), '-j', '6'], check=True)
subprocess.run([str(build / 'bird-host'), str(out)], check=True)
assert (out / 'ui-text.txt').read_text() == (root / 'tests/expected/ui-text.txt').read_text(), 'English UI changed; review and update its owner-local snapshot.'
mask = Image.new('L', (360, 360))
ImageDraw.Draw(mask).ellipse((0, 0, 359, 359), fill=255)
overview = Image.new('RGB', (760, 760), '#10151d')
for i, name in enumerate(['ready', 'playing', 'paused', 'over', 'save-error']):
    frame = Image.open(out / (name + '.ppm')).convert('RGB')
    image = Image.new('RGB', (360, 360), '#10151d')
    image.paste(frame, (0, 0), mask)
    image.save(out / (name + '.png'))
    if i < 4:
        overview.paste(image, (10 + i % 2 * 380, 10 + i // 2 * 380))
overview.save(out / 'preview-en.png')
print('PASS: English output snapshot and actual LVGL preview:', out / 'preview-en.png')
