#!/usr/bin/env python3
"""Build sanitizer checks, replay protocol/UI behavior and verify owner-local expected UI output."""
from pathlib import Path
import argparse
import difflib
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=REPO / '/tmp/sugarat-cover')
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = output / 'host'
    command = ['cmake', '-S', str(ROOT / 'tests/host'), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug']
    if args.sdk:
        command.append('-DESP_IDF_ROOT=' + str(args.sdk.resolve()))
    subprocess.run(command, check=True)
    subprocess.run(['cmake', '--build', str(build), '-j', '8'], check=True)
    with tempfile.TemporaryDirectory(prefix='core-', dir=output) as temp:
        subprocess.run([str(build / 'cover-core-tests'), str(ROOT / 'assets/sample.jpg'), temp], check=True)
    ui = output / 'ui'
    ui.mkdir(exist_ok=True)
    actual = output / 'ui-text.actual.txt'
    subprocess.run([str(build / 'cover-ui-tests'), str(ROOT / 'assets/sample.jpg'), str(ui), str(actual)], check=True)
    expected = ROOT / 'tests/expected/ui-text.txt'
    if args.record:
        expected.write_bytes(actual.read_bytes())
    elif not expected.exists() or expected.read_bytes() != actual.read_bytes():
        before = expected.read_text() if expected.exists() else ''
        print(''.join(difflib.unified_diff(before.splitlines(True), actual.read_text().splitlines(True), fromfile=str(expected), tofile=str(actual))))
        raise SystemExit('UI output differs; review it before explicitly recording')
    for path in ui.glob('*.bmp'):
        Image.open(path).save(path.with_suffix('.png'))
    print('PASS: sanitizer checks and bilingual UI expected output')

if __name__ == '__main__':
    main()
