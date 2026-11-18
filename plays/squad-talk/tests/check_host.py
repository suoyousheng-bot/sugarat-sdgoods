#!/usr/bin/env python3
"""Check the Squad Talk protocol/codec, actual LVGL input and English screen snapshot."""
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
build = out / 'host-build'
subprocess.run(['cmake', '-S', str(root / 'tests/host'), '-B', str(build), '-G', 'Ninja'], check=True)
subprocess.run(['cmake', '--build', str(build), '-j', '6'], check=True)
subprocess.run([str(build / 'squad-host'), str(out)], check=True)
assert (out / 'ui-text.txt').read_text() == (root / 'tests/expected/ui-text.txt').read_text(), 'Review the changed English screen text before updating the snapshot.'
mask = Image.new('L', (360, 360))
ImageDraw.Draw(mask).ellipse((0, 0, 359, 359), fill=255)
board = Image.new('RGB', (1160, 810), '#E8DAF2')
draw = ImageDraw.Draw(board)
for i, name in enumerate(['waiting', 'ready', 'talking', 'listening', 'paused', 'error']):
    image = Image.open(out / (name + '.ppm')).convert('RGB')
    tile = Image.new('RGB', (360, 360), '#E8DAF2')
    tile.paste(image, (0, 0), mask)
    tile.save(out / (name + '.png'))
    board.paste(tile, (10 + i % 3 * 385, 35 + i // 3 * 390))
    draw.text((25 + i % 3 * 385, 15 + i // 3 * 390), name.upper(), fill='#282138')
board.save(out / 'preview-en.png')
print('PASS: owner-local English text snapshot and actual LVGL preview:', out / 'preview-en.png')
