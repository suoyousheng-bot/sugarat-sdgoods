#!/usr/bin/env python3
"""Run real card, storage, crypto and LVGL checks and export firmware UI images."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def images(out):
    mask = Image.new('L', (360, 360))
    ImageDraw.Draw(mask).ellipse((0, 0, 359, 359), fill=255)
    for path in out.glob('*.ppm'):
        Image.open(path).save(path.with_suffix('.png'))
    assert ImageChops.difference(Image.open(out / 'long-fields-start.png'), Image.open(out / 'long-fields-scrolled.png')).getbbox(), 'Long fields did not scroll'
    pages = [('home', 'HOME'), ('drawer', 'SWIPE UP'), ('qr', 'VCARD QR'), ('nearby', 'NEARBY'), ('verify', 'VERIFY'), ('edit-url', 'EDIT ON PHONE')]
    board = Image.new('RGB', (1230, 860), '#eceee8')
    for index, (page, title) in enumerate(pages):
        x, y = 25 + index % 3 * 410, 15 + index // 3 * 425
        board.paste(Image.open(out / f'{page}.png'), (x, y), mask)
        ImageDraw.Draw(board).text((x + 110, y + 380), title, fill='#26382b')
    board.save(out / 'overview.png')
    top = Image.new('RGB', (1230, 470), '#eceee8')
    for x, name, title in [(25, 'home', 'HOME'), (435, 'drawer', 'SWIPE UP'), (845, 'qr', 'VCARD QR')]:
        top.paste(Image.open(out / f'{name}.png'), (x, 20), mask)
        ImageDraw.Draw(top).text((x + 110, 410), title, fill='#26382b')
    top.save(out / 'home-drawer-qr.png')
    frames = []
    for prefix in ['drawer-open', 'drawer-snap']:
        for index in range(12):
            frame = Image.new('RGB', (400, 410), '#eceee8')
            frame.paste(Image.open(out / f'{prefix}-{index:02d}.png'), (20, 15), mask)
            ImageDraw.Draw(frame).text((118, 386), 'SUGARAT / SWIPE UP', fill='#26382b')
            frames.append(frame)
    frames.extend([frames[-1].copy() for _ in range(18)])
    frames[0].save(out / 'drawer-swipe.gif', save_all=True, append_images=frames[1:], duration=40, loop=0, disposal=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    if not os.environ.get('IDF_PATH'):
        raise SystemExit('Set IDF_PATH to the installed ESP-IDF 5.5.4 checkout')
    destination = args.output.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='run-', dir=destination))
    subprocess.run(['python3', str(ROOT / 'tools/prepare_assets.py'), '--check'], check=True)
    with (out / 'build.log').open('w') as log:
        subprocess.run(['cmake', '-S', str(ROOT / 'tests/host'), '-B', str(out / 'build'), '-G', 'Ninja'], stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run(['cmake', '--build', str(out / 'build'), '-j', '6'], stdout=log, stderr=subprocess.STDOUT, check=True)
    with tempfile.TemporaryDirectory(prefix='card-store-', dir=out) as directory:
        core = subprocess.run([str(out / 'build/card-core'), directory], capture_output=True, text=True, check=True)
    http = subprocess.run([str(out / 'build/card-http')], capture_output=True, text=True, check=True)
    language = subprocess.run([str(out / 'build/card-language')], capture_output=True, text=True, check=True)
    ui = subprocess.run([str(out / 'build/card-ui'), str(out)], capture_output=True, text=True, check=True)
    expected = ROOT / 'tests/expected/ui-text.txt'
    if args.record:
        expected.write_bytes((out / 'ui-text.txt').read_bytes())
    elif expected.read_bytes() != (out / 'ui-text.txt').read_bytes():
        raise SystemExit(f'UI text differs: review {out / "ui-text.txt"}')
    images(out)
    result = core.stdout + http.stdout + language.stdout + ui.stdout + 'PASS: owner-local visible-text snapshot\n'
    (out / 'validation.txt').write_text(result)
    print(result)
    print(out)


if __name__ == '__main__':
    main()
