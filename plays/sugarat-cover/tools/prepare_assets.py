#!/usr/bin/env python3
"""Generate baseline JPEG and bilingual subsets from the existing Sugarat font family."""
from pathlib import Path
import argparse
import importlib.util
import sys
from PIL import Image
from fontTools import subset

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font-dir', type=Path, required=True)
    args = parser.parse_args()
    image = Image.open(ROOT / 'assets/sample-source.png').convert('RGB')
    if image.size != (360, 360):
        raise SystemExit('Sample source must match the approved 360px cover')
    image.save(ROOT / 'assets/sample.jpg', quality=95, subsampling=0, progressive=False)
    source = ROOT / 'components/sugarat_watch/tools/make_fonts.py'
    spec = importlib.util.spec_from_file_location('cover_fonts', source)
    fonts = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fonts)
    for item in fonts.STYLES['sugarat_watch']:
        if item['kind'] == 'text':
            family, size, _ = item['aa']
            item['aa'] = (family, size, 1 if size >= 20 else 0)
    sys.argv = [str(source), '--font-dir', str(args.font_dir.resolve()), '--sources', str(ROOT / 'main/cover')]
    fonts.main()
    chars = set(range(32, 127))
    for file in (ROOT / 'main/web').glob('*'):
        if file.suffix in ('.html', '.js'):
            chars.update(map(ord, file.read_text()))
    options = subset.Options()
    options.flavor = 'woff2'
    font = subset.load_font(str(args.font_dir / 'SourceHanSansSC-Normal.otf'), options)
    sub = subset.Subsetter(options=options)
    sub.populate(unicodes=chars)
    sub.subset(font)
    subset.save_font(font, str(ROOT / 'main/web/font.woff2'), options)
    print('Generated baseline sample JPEG and same-family device/web font subsets')

if __name__ == '__main__':
    main()
