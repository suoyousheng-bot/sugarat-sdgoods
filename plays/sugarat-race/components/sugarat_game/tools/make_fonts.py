#!/usr/bin/env python3
"""Regenerate the text and number fonts of Sugarat Watch or Sugarat Game for LVGL 8.3.

The style follows the component directory that contains this script. Text fonts hold
printable ASCII, tools/common-hanzi.txt and every non-ASCII character in string literals
of the component sources and of the --sources files or directories, so run it after an
application adds text that shows missing glyphs. Requires Pillow and the LVGL 8.3
scripts/built_in_font directory (SourceHanSansSC-Normal.otf, Montserrat-Medium.ttf,
unscii-8.ttf). Icon fonts are fixed and are not rewritten.

usage: python3 tools/make_fonts.py --font-dir <lvgl>/scripts/built_in_font --sources ../../main
"""
from pathlib import Path
import argparse
import re

from PIL import Image, ImageDraw, ImageFont

COMPONENT = Path(__file__).resolve().parents[1]
HAN, MONT, UNSCII = "SourceHanSansSC-Normal.otf", "Montserrat-Medium.ttf", "unscii-8.ttf"
SS = 4
NUM = "0123456789:%.,-+/° "
EXTRA = "·…—“”‘’，。！？、：；（）《》【】℃°～"

# aa: (font file, pixel size, stroke in quarter pixels). pixel: (Chinese px, Chinese scale, Latin scale).
# glyphs "common" adds tools/common-hanzi.txt; "sources" uses only string literals.
STYLES = {
    "sugarat_watch": [
        dict(name="sw_font_text_14", kind="text", aa=(HAN, 14, 1), glyphs="common", fallback="sw_font_icon_16"),
        dict(name="sw_font_text_16", kind="text", aa=(HAN, 16, 2), glyphs="common", fallback="sw_font_icon_16"),
        dict(name="sw_font_text_20", kind="text", aa=(HAN, 20, 4), glyphs="common", fallback="sw_font_icon_20"),
        dict(name="sw_font_text_28", kind="text", aa=(HAN, 28, 7), glyphs="sources", fallback="sw_font_icon_28"),
        dict(name="sw_font_num_24", kind="num", aa=(MONT, 24, 4), fallback="sw_font_text_20"),
        dict(name="sw_font_num_40", kind="num", aa=(MONT, 40, 6), fallback="sw_font_text_28"),
        dict(name="sw_font_num_80", kind="num", aa=(MONT, 80, 9), fallback="sw_font_text_28"),
    ],
    "sugarat_game": [
        dict(name="sg_font_s", kind="text", pixel=(16, 1, 2), glyphs="common", fallback="sg_font_icon_16"),
        dict(name="sg_font_m", kind="text", pixel=(12, 2, 2), glyphs="common", fallback="sg_font_icon_24"),
        dict(name="sg_font_l", kind="text", pixel=(12, 3, 3), glyphs="sources", fallback="sg_font_icon_32"),
        dict(name="sg_font_num_m", kind="num", pixel=(12, 3, 4), fallback="sg_font_l"),
        dict(name="sg_font_num_l", kind="num", pixel=(12, 3, 7), fallback="sg_font_l"),
    ],
}


def collect(sources, common):
    chars = {chr(c) for c in range(0x20, 0x7F)} | set(EXTRA)
    files = []
    for src in sources:
        if src.is_file():
            files.append(src)
        elif src.is_dir():
            files += [p for p in src.rglob("*") if p.suffix in (".c", ".h", ".cpp", ".hpp") and "fonts" not in p.parts]
    for path in files:
        for literal in re.findall(r'"((?:[^"\\\n]|\\.)*)"', path.read_text(encoding="utf-8", errors="ignore")):
            chars.update(ch for ch in literal if ord(ch) > 0x7F and not 0xE000 <= ord(ch) <= 0xF8FF)
    if common:
        for line in (COMPONENT / "tools/common-hanzi.txt").read_text(encoding="utf-8").splitlines():
            if not line.startswith("#"):
                chars.update(ch for ch in line if ord(ch) > 0x7F)
    return sorted(c for c in chars if ord(c) <= 0xFFFF)


def render_aa(font_big, ch, stroke):
    x0, y0, x1, y1 = font_big.getbbox(ch, anchor="ls", stroke_width=stroke)
    if x1 <= x0 or y1 <= y0:
        return None
    x0, y0 = (x0 // SS) * SS, (y0 // SS) * SS
    x1, y1 = -(-x1 // SS) * SS, -(-y1 // SS) * SS
    image = Image.new("L", (x1 - x0, y1 - y0), 0)
    ImageDraw.Draw(image).text((-x0, -y0), ch, font=font_big, anchor="ls", fill=255, stroke_width=stroke, stroke_fill=255)
    small = image.reduce(SS)
    bbox = small.getbbox()
    if not bbox:
        return None
    return small.crop(bbox), x0 // SS + bbox[0], -(y0 // SS + bbox[3])


_pixel_fonts = {}


def render_pixel(font_dir, ch, cjk_px, cjk_scale, latin_scale):
    """Return (1-bit block image or None, ofs_x, ofs_y, advance) on a baseline shared by Chinese and Latin."""
    if ord(ch) < 0x80:
        key, size, threshold, adv, scale = (UNSCII, 8), 8, 128, 8, latin_scale
    else:
        key, size, threshold, adv, scale = (HAN, cjk_px), cjk_px, 110, cjk_px + 1, cjk_scale
    font = _pixel_fonts.setdefault(key, ImageFont.truetype(str(font_dir / key[0]), key[1]))
    if ch == " ":
        return None, 0, 0, (4 if size == 8 else adv) * scale
    canvas = Image.new("L", (size * 3, size * 3), 0)
    ImageDraw.Draw(canvas).text((size, size * 2), ch, font=font, anchor="ls", fill=255)
    canvas = canvas.point(lambda v: 255 if v >= threshold else 0)
    bbox = canvas.getbbox()
    if not bbox:
        return None, 0, 0, adv * scale
    glyph = canvas.crop(bbox).resize(((bbox[2] - bbox[0]) * scale, (bbox[3] - bbox[1]) * scale), Image.Resampling.NEAREST)
    return glyph, (bbox[0] - size) * scale, (size * 2 - bbox[3]) * scale, adv * scale


def pack(image, bpp):
    values = [v >> (8 - bpp) for v in image.getdata()]
    per = 8 // bpp
    while len(values) % per:
        values.append(0)
    out = bytearray()
    for i in range(0, len(values), per):
        byte = 0
        for v in values[i:i + per]:
            byte = (byte << bpp) | v
        out.append(byte)
    return out


def write_font(path, name, glyphs, bpp, line_height, base_line, fallback, origin):
    """Write an LVGL 8 lv_font_fmt_txt font. glyphs: sorted (code point, advance*16, (image, ofs_x, ofs_y) or None)."""
    bitmap, dscs = bytearray(), []
    for _, adv, g in glyphs:
        if g:
            image, ox, oy = g
            dscs.append((len(bitmap), adv, image.width, image.height, ox, oy))
            bitmap += pack(image, bpp)
        else:
            dscs.append((len(bitmap), adv, 0, 0, 0, 0))
    codes = [g[0] for g in glyphs]
    start = codes[0]
    offsets = [c - start for c in codes]
    lines = [f"/* Generated by {origin}; do not edit. */", '#include "lvgl.h"', ""]
    if fallback:
        lines += [f"extern const lv_font_t {fallback};", ""]
    lines.append("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {")
    lines += ["    " + ",".join(f"0x{b:02x}" for b in bitmap[i:i + 24]) + "," for i in range(0, len(bitmap), 24)]
    lines += ["};", "", "static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {",
              "    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},"]
    lines += [f"    {{.bitmap_index = {i}, .adv_w = {a}, .box_w = {w}, .box_h = {h}, .ofs_x = {x}, .ofs_y = {y}}},"
              for i, a, w, h, x, y in dscs]
    lines += ["};", "", "static const uint16_t unicode_list[] = {"]
    lines += ["    " + ",".join(f"0x{o:x}" for o in offsets[i:i + 16]) + "," for i in range(0, len(offsets), 16)]
    lines += ["};", "", "static const lv_font_fmt_txt_cmap_t cmaps[] = {",
              f"    {{.range_start = 0x{start:x}, .range_length = {offsets[-1] + 1}, .glyph_id_start = 1,",
              f"     .unicode_list = unicode_list, .glyph_id_ofs_list = NULL, .list_length = {len(offsets)},",
              "     .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY},", "};", "",
              "static lv_font_fmt_txt_glyph_cache_t cache;", "",
              "static const lv_font_fmt_txt_dsc_t font_dsc = {",
              "    .glyph_bitmap = glyph_bitmap, .glyph_dsc = glyph_dsc, .cmaps = cmaps,",
              f"    .kern_dsc = NULL, .kern_scale = 0, .cmap_num = 1, .bpp = {bpp},",
              "    .kern_classes = 0, .bitmap_format = 0, .cache = &cache,", "};", "",
              f"const lv_font_t {name} = {{",
              "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,",
              f"    .line_height = {line_height}, .base_line = {base_line},",
              "    .subpx = LV_FONT_SUBPX_NONE, .underline_position = -2, .underline_thickness = 1,",
              f"    .dsc = &font_dsc, .fallback = {('&' + fallback) if fallback else 'NULL'}, .user_data = NULL,", "};", ""]
    path.write_text("\n".join(lines))
    return len(bitmap)


def generate(font_dir, spec, chars):
    glyphs = []
    if "aa" in spec:
        source, size, stroke = spec["aa"]
        big = ImageFont.truetype(str(font_dir / source), size * SS)
        one = ImageFont.truetype(str(font_dir / source), size)
        for ch in chars:
            g = None if ch == " " else render_aa(big, ch, stroke)
            glyphs.append((ord(ch), round(one.getlength(ch) * 16 + stroke * 16 / SS), g))
        bpp = 4
    else:
        cjk_px, cjk_scale, latin_scale = spec["pixel"]
        for ch in chars:
            image, ox, oy, adv = render_pixel(font_dir, ch, cjk_px, cjk_scale, latin_scale)
            glyphs.append((ord(ch), adv * 16, (image, ox, oy) if image else None))
        bpp = 1
    tops = [g[2][2] + g[2][0].height for g in glyphs if g[2]]
    bottoms = [-g[2][2] for g in glyphs if g[2]]
    if "aa" in spec and spec["kind"] == "text":
        line_height, base_line = round(spec["aa"][1] * 1.3), round(spec["aa"][1] * 0.24)
    else:
        pad = spec["pixel"][1] if "pixel" in spec and spec["kind"] == "text" else 0
        base_line = max(0, max(bottoms)) + pad
        line_height = max(tops) + base_line + pad
    return write_font(COMPONENT / "src/fonts" / f"{spec['name']}.c", spec["name"], glyphs, bpp, line_height, base_line,
                      spec.get("fallback"), "tools/make_fonts.py"), len(glyphs)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--font-dir", type=Path, required=True, help="LVGL 8.3 scripts/built_in_font directory")
    parser.add_argument("--sources", nargs="*", default=[], type=Path, help="application files or directories with UI text")
    args = parser.parse_args()
    style = COMPONENT.name
    if style not in STYLES:
        raise SystemExit(f"run this script from components/sugarat_watch or components/sugarat_game, not {style}")
    missing = [f for f in (HAN, MONT, UNSCII) if not (args.font_dir / f).is_file()]
    if missing:
        raise SystemExit(f"{args.font_dir} lacks {', '.join(missing)}")
    sources = [COMPONENT / "src", COMPONENT / "include", COMPONENT / "examples", *args.sources]
    sets = {"common": collect(sources, True), "sources": collect(sources, False)}
    total = 0
    for spec in STYLES[style]:
        chars = sets[spec["glyphs"]] if spec["kind"] == "text" else sorted(NUM)
        size, count = generate(args.font_dir, spec, chars)
        total += size
        print(f"{spec['name']}: {count} glyphs, {size / 1024:.1f} KiB")
    print(f"{style}: {total / 1024:.0f} KiB of text glyph bitmaps")


if __name__ == "__main__":
    main()
