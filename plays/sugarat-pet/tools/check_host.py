#!/usr/bin/env python3
"""Check pinned epet sources, run real care/storage/LVGL code and compare English output."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = options.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    record = json.loads((root / 'upstream.json').read_text())
    for name, expected in record['files'].items():
        actual = hashlib.sha256((root / record['importedDirectory'] / name).read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f'Pinned epet source differs: {name}')
    subprocess.run(['cmake', '-S', str(root / 'tests/host'), '-B', str(out / 'build'), '-G', 'Ninja'], check=True)
    subprocess.run(['cmake', '--build', str(out / 'build'), '-j', '6'], check=True)
    subprocess.run([str(out / 'build/pet-family-tests')], check=True)
    subprocess.run([str(out / 'build/pet-nvs-tests')], check=True)
    subprocess.run([str(out / 'build/pet-social-tests')], check=True)
    subprocess.run([str(out / 'build/pet-host'), str(out)], check=True)
    subprocess.run([str(out / 'build/pet-host'), str(out), 'legacy'], check=True)
    expected = root / 'tests/expected/ui-text.txt'
    if (out / 'ui-text.txt').read_bytes() != expected.read_bytes():
        raise SystemExit('English UI output changed; review the view and its owner-local text snapshot.')
    overview = Image.new('RGB', (1160, 1640), '#f5f0e6')
    draw = ImageDraw.Draw(overview)
    mask = Image.new('L', (360, 360))
    ImageDraw.Draw(mask).ellipse((0, 0, 359, 359), fill=255)
    pages = [('home', 'HOME'), ('controls', 'SWIPE UP'), ('feeding', 'FEED'), ('starter', 'FIRST FRIEND'),
             ('family', 'FAMILY'), ('nearby', 'NEARBY'), ('verify', 'VERIFY CODE'), ('linked', 'SOCIAL'),
             ('egg', 'TEND EGG'), ('hatch-ready', 'READY'), ('hatch-result', 'NEW FAMILY MEMBER'), ('stats', 'CARE STATS')]
    for index, (name, title) in enumerate(pages):
        frame = Image.open(out / f'{name}.ppm').convert('RGB')
        frame.save(out / f'{name}.png')
        x, y = 10 + index % 3 * 390, 10 + index // 3 * 410
        overview.paste(frame, (x, y), mask)
        draw.text((x + 100, y + 375), title, fill='#514738')
    overview.save(out / 'overview.png')
    frames = []
    names = ['Ember', 'Leaf', 'Cloud', 'Sprout', 'Glow', 'Willow']
    small_mask = mask.resize((240, 240))
    for clip in range(4):
        for step in range(4):
            image = Image.new('RGB', (780, 600), '#f5f0e6')
            pen = ImageDraw.Draw(image)
            pen.text((20, 12), 'SUGARAT  /  SDGoods firmware rendering', fill='#514738')
            pen.text((20, 34), ['IDLE', 'BLINK', 'FEED', 'HAPPY'][clip], fill='#514738')
            for kind, name in enumerate(names):
                source = Image.open(out / f'character-{kind}-{clip}-{step}.ppm').convert('RGB').resize((240, 240), Image.Resampling.NEAREST)
                x, y = 10 + kind % 3 * 260, 60 + kind // 3 * 270
                image.paste(source, (x, y), small_mask)
                pen.text((x + 100, y + 245), name, fill='#514738')
            frames.append(image)
    frames[0].save(out / 'animated-family.gif', save_all=True, append_images=frames[1:], duration=220, loop=0, disposal=2)
    tray_frames = []
    for direction in ['open', 'close']:
        for step in range(12):
            source = Image.open(out / f'controls-{direction}-{step}.ppm').convert('RGB')
            card = Image.new('RGB', (400, 410), '#f5f0e6')
            card.paste(source, (20, 10), mask)
            ImageDraw.Draw(card).text((120, 384), 'SWIPE UP / ICON CONTROLS', fill='#514738')
            tray_frames.append(card)
        tray_frames.extend([tray_frames[-1].copy() for _ in range(14)])
    tray_frames[0].save(out / 'controls-swipe.gif', save_all=True, append_images=tray_frames[1:], duration=40, loop=0, disposal=2)
    characters = Image.new('RGB', (1080, 720), '#f5f0e6')
    for kind in range(6):
        source = Image.open(out / f'character-{kind}-0-0.ppm').convert('RGB')
        characters.paste(source, (kind % 3 * 360, kind // 3 * 360), mask)
    characters.save(out / 'characters.png')
    print('PASS: owner-local text snapshot, twelve LVGL pages, icon-tray motion and six-character animation:', out / 'animated-family.gif')



if __name__ == '__main__':
    main()
