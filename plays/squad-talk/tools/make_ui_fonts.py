#!/usr/bin/env python3
"""Build the application-local rounded English font faces from bundled OFL Nunito."""
from pathlib import Path
import importlib.util
from PIL import ImageFont

root = Path(__file__).resolve().parents[1]
component = root / 'components/sugarat_game'
spec = importlib.util.spec_from_file_location('game_fonts', component / 'tools/make_fonts.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
font_path = root / 'assets/fonts/Nunito-Variable.ttf'
for size in [14, 18, 22, 26, 30]:
    big = ImageFont.truetype(str(font_path), size * helper.SS)
    one = ImageFont.truetype(str(font_path), size)
    big.set_variation_by_axes([1000])
    one.set_variation_by_axes([1000])
    glyphs = []
    for code in range(32, 127):
        ch = chr(code)
        glyph = None if ch == ' ' else helper.render_aa(big, ch, 0)
        glyphs.append((code, round(one.getlength(ch) * 16), glyph))
    path = component / 'src/fonts' / ('squad_font_' + str(size) + '.c')
    helper.write_font(path, 'squad_font_' + str(size), glyphs, 4, round(size * 1.3), round(size * .24), None, 'tools/make_ui_fonts.py; Nunito, SIL OFL 1.1')
    path.write_text("/* Copyright 2014 The Nunito Project Authors. SPDX-License-Identifier: OFL-1.1 */\n" + path.read_text())
    print(path.name)
