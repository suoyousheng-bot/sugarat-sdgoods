"""Generate the original 4-bit LVGL texture for the Sugarat Beat round screen."""

from bisect import bisect_right
import math
import random
import sys
from pathlib import Path

SIDE = 360
LEVELS = (30, 38, 48, 58, 68, 78, 88, 98, 108, 118, 128, 138, 148, 158, 168, 178)
MIDPOINTS = tuple((a + b) / 2 for a, b in zip(LEVELS, LEVELS[1:]))
DITHER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))


def texture():
    rng = random.Random(81)
    image = bytearray(c for level in LEVELS for c in (level, level, level, 255))
    grit = [[rng.uniform(-7, 7) for _ in range(25)] for _ in range(25)]
    for y in range(SIDE):
        line = bytearray()
        for x in range(SIDE):
            u, v = (x - 179.5) / 180, (y - 179.5) / 180
            falloff = min(1.35, u * u + v * v)
            haze = 51 * math.exp(-7 * (v - .15) ** 2)
            brushed = 3.5 * math.sin(y / 1.15) + 1.9 * math.sin(y / 4.9)
            cloud = 6 * math.sin(6 * u - 1.8 * v) * math.cos(8 * v + 4 * u)
            luminance = 115 + haze - 75 * falloff + brushed + cloud
            col, dx = divmod(x, 15)
            row, dy = divmod(y, 15)
            upper = grit[row][col] + (grit[row][col + 1] - grit[row][col]) * dx / 15
            lower = grit[row + 1][col] + (grit[row + 1][col + 1] - grit[row + 1][col]) * dx / 15
            luminance += upper + (lower - upper) * dy / 15 + rng.uniform(-6, 6)
            shade = bisect_right(MIDPOINTS, luminance + (DITHER[y % 4][x % 4] - 7.5) * .6)
            if x % 2:
                line[-1] |= shade
            else:
                line.append(shade << 4)
        image.extend(line)
    return image


def main():
    image = texture()
    lines = [", ".join(f"0x{byte:02x}" for byte in image[i:i + 20]) + ","
             for i in range(0, len(image), 20)]
    Path(sys.argv[1]).write_text(
        '#include "lvgl.h"\n\n'
        'static const LV_ATTRIBUTE_MEM_ALIGN uint8_t backdrop_pixels[] = {\n    '
        + '\n    '.join(lines) + '\n};\n\n'
        'const lv_img_dsc_t beat_backdrop = {\n'
        '    .header = {.cf = LV_IMG_CF_INDEXED_4BIT, .w = 360, .h = 360},\n'
        '    .data_size = sizeof(backdrop_pixels),\n'
        '    .data = backdrop_pixels,\n};\n'
    )


if __name__ == "__main__":
    main()
