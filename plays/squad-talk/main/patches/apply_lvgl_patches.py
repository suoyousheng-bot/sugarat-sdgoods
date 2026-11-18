# SPDX-License-Identifier: MIT
# Modified by Sugarat: apply the retained LVGL patches to the pinned local component.
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[2]
lvgl = root / 'components/lvgl'
if not lvgl.is_dir():
    # Repository builds take LVGL from the kit's shared components.
    lvgl = root.parents[1] / 'shared/components/lvgl'
for name in ['gifdec.c', 'gifdec.h', 'lv_gif.c']:
    source = root / 'main/patches/lvgl-8.3.11-gif-psram' / name
    destination = lvgl / 'src/extra/libs/gif' / name
    if source.read_bytes() != destination.read_bytes():
        shutil.copyfile(source, destination)
    if source.read_bytes() != destination.read_bytes():
        raise RuntimeError('LVGL patch verification failed: ' + name)
