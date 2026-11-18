#!/usr/bin/env python3
"""Generate embedded editor text and the application's LVGL font subsets."""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font-dir', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    html = (ROOT / 'main/card/editor.html').read_text()
    expected = json.dumps(html, ensure_ascii=False) + '\n'
    output = ROOT / 'main/card/editor.inc'
    if args.check:
        if not output.exists() or output.read_text() != expected:
            raise SystemExit('editor.inc differs; run prepare_assets.py to regenerate it')
        print('PASS: embedded editor matches editor.html')
        return
    output.write_text(expected)
    if not args.font_dir:
        return
    component = ROOT / 'components/sugarat_watch'
    spec = importlib.util.spec_from_file_location('watch_fonts', component / 'tools/make_fonts.py')
    fonts = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fonts)
    fonts.COMPONENT = component
    common = fonts.collect([ROOT / 'main'], True)
    total = 0
    for original in fonts.STYLES['sugarat_watch']:
        item = dict(original)
        if item['kind'] != 'text':
            continue
        chars = common
        if item['name'] in ('sw_font_text_16', 'sw_font_text_28'):
            item['bpp'] = 2
            chars = sorted(set(common) | {chr(code) for code in range(0x4e00, 0xa000)})
        size, count = fonts.generate(args.font_dir, item, chars)
        print(f"{item['name']}: {count} glyphs, {size} bitmap bytes", flush=True)
        total += size
    print(f'Total text bitmap bytes: {total}')


if __name__ == '__main__':
    main()
