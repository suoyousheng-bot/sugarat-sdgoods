#!/usr/bin/env python3
"""Build the SDGOODS application with an explicitly selected ESP-IDF SDK; never open a serial port."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--tools', type=Path, default=Path.home() / '.espressif')
    parser.add_argument('--output', type=Path, default=REPO / '/tmp/sugarat-cover-build')
    args = parser.parse_args()
    env = {key: value for key, value in os.environ.items()
           if not any(part in key.upper() for part in ('KEY', 'SECRET', 'TOKEN', 'PASSWORD'))}
    env.update(IDF_PATH=str(args.sdk.resolve()), IDF_TOOLS_PATH=str(args.tools.resolve()), IDF_PYTHON_ENV_PATH=sys.prefix)
    export = subprocess.run([sys.executable, str(args.sdk / 'tools/idf_tools.py'), 'export', '--format', 'key-value'],
                            env=env, check=True, capture_output=True, text=True)
    for line in export.stdout.splitlines():
        key, separator, value = line.partition('=')
        if separator:
            env[key] = value.replace('$PATH', env.get('PATH', ''))
    subprocess.run([sys.executable, str(args.sdk / 'tools/idf.py'), '-C', str(ROOT), '-B', str(args.output.resolve()), 'build'], env=env, check=True)

if __name__ == '__main__':
    main()
