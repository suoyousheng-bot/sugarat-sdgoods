# SPDX-License-Identifier: MIT
# Modified by Sugarat: apply the retained LVGL patches to the pinned local component.
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[2]
for name in ['gifdec.c', 'gifdec.h', 'lv_gif.c']:
    source = root / 'main/patches/lvgl-8.3.11-gif-psram' / name
    destination = root / 'components/lvgl/src/extra/libs/gif' / name
    if source.read_bytes() != destination.read_bytes():
        shutil.copyfile(source, destination)
    if source.read_bytes() != destination.read_bytes():
        raise RuntimeError('LVGL patch verification failed: ' + name)
