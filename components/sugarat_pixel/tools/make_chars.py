#!/usr/bin/env python3
"""Draw the Sugarat pixel character library and encode it for LVGL.

Every character is a 32x32 sprite built from simple pixel shapes, outlined
automatically, then combined with a shared face system that produces each
mood. Output: 4-bit indexed frames (src/sp_chars.c, include/sp_chars.h) and,
when a path is given, a PNG proof sheet. Requires Pillow.

usage: python3 tools/make_chars.py [proof.png]
"""
from pathlib import Path
import sys

from PIL import Image, ImageDraw

COMPONENT = Path(__file__).resolve().parents[1]
N = 32
MOODS = ["IDLE", "HAPPY", "SURPRISED", "SAD", "SLEEP", "LOVE", "TALK", "WINK"]
FRAMES = MOODS + ["BLINK"]


def c(h):
    return h.lstrip("#").upper()


INK = c("#2A2038")
WHITE = c("#FFFFFF")
HEART = c("#FF4F7B")
TEAR = c("#7CC7FF")
SNORE = c("#9AA4FF")
SKIN = c("#FFD9B8")
SKIN_SHADE = c("#F5BE98")
CHEEK = c("#FF9AA8")


class Sprite:
    def __init__(self):
        self.p = {}

    def put(self, x, y, col):
        if 0 <= x < N and 0 <= y < N:
            self.p[(x, y)] = col

    def rect(self, x, y, w, h, col):
        for j in range(y, y + h):
            for i in range(x, x + w):
                self.put(i, j, col)

    def rrect(self, x, y, w, h, r, col):
        for j in range(y, y + h):
            for i in range(x, x + w):
                dx = max(x + r - i - .5, 0, i + .5 - (x + w - r))
                dy = max(y + r - j - .5, 0, j + .5 - (y + h - r))
                if dx * dx + dy * dy <= r * r + .35:
                    self.put(i, j, col)

    def ellipse(self, cx, cy, rx, ry, col):
        for j in range(N):
            for i in range(N):
                if ((i + .5 - cx) / rx) ** 2 + ((j + .5 - cy) / ry) ** 2 <= 1.0:
                    self.put(i, j, col)

    def tri_up(self, x, y, h, col, lean=0):
        """Triangle with apex at (x, y) widening downward over h rows."""
        for k in range(h):
            self.rect(x - k // 2 + (lean * k) // h, y + k, k + 1, 1, col)

    def recolor(self, frm, to, region=None):
        for key, val in list(self.p.items()):
            if val == frm and (region is None or region(*key)):
                self.p[key] = to

    def pattern(self, x, y, rows, col):
        for j, row in enumerate(rows):
            for i, ch in enumerate(row):
                if ch != ".":
                    self.put(x + i, y + j, col)

    def outline(self, col=INK):
        filled = set(self.p)
        for (x, y) in list(filled):
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                q = (x + dx, y + dy)
                if q not in filled and 0 <= q[0] < N and 0 <= q[1] < N:
                    self.p[q] = col

    def copy(self):
        s = Sprite()
        s.p = dict(self.p)
        return s


class Face:
    """Where the shared face system draws eyes, mouth and cheeks."""

    def __init__(self, ex1, ex2, ey, mx, my, cheeks=True, eye=INK, mouth=True, cheek=CHEEK,
                 highlight=True, zx=25, zy=2, cheek_y=None, cheek_dx=3):
        self.__dict__.update(locals())
        del self.__dict__["self"]


def draw_face(s, f, mood):
    eye = f.eye

    def open_eye(x, tall=3, y=None):
        y = f.ey if y is None else y
        s.rect(x, y, 2, tall, eye)
        if f.highlight:
            s.put(x + 1, y, WHITE if eye == INK else INK)

    def arc_eye(x, up=True):
        if up:
            s.put(x - 1, f.ey + 2, eye); s.put(x, f.ey + 1, eye); s.put(x + 1, f.ey + 1, eye); s.put(x + 2, f.ey + 2, eye)
        else:
            s.put(x - 1, f.ey + 1, eye); s.put(x, f.ey + 2, eye); s.put(x + 1, f.ey + 2, eye); s.put(x + 2, f.ey + 1, eye)

    def heart(x):
        s.pattern(x - 2, f.ey - 1, ["XX.XX", "XXXXX", ".XXX.", "..X.."], HEART)
        s.put(x - 1, f.ey - 1, WHITE)

    mx, my = f.mx, f.my
    smile = lambda: (s.put(mx, my, INK), s.put(mx + 1, my + 1, INK), s.put(mx + 2, my + 1, INK), s.put(mx + 3, my, INK))
    if mood in ("IDLE", "TALK"):
        open_eye(f.ex1); open_eye(f.ex2)
    elif mood == "BLINK":
        s.rect(f.ex1, f.ey + 2, 2, 1, eye); s.rect(f.ex2, f.ey + 2, 2, 1, eye)
    elif mood == "HAPPY":
        arc_eye(f.ex1); arc_eye(f.ex2)
    elif mood == "SURPRISED":
        open_eye(f.ex1, 4, f.ey - 1); open_eye(f.ex2, 4, f.ey - 1)
    elif mood == "SAD":
        open_eye(f.ex1, 2, f.ey + 1); open_eye(f.ex2, 2, f.ey + 1)
        s.put(f.ex1, f.ey - 1, eye); s.put(f.ex1 + 1, f.ey - 2, eye)
        s.put(f.ex2, f.ey - 2, eye); s.put(f.ex2 + 1, f.ey - 1, eye)
        s.rect(f.ex1 - 1, f.ey + 3, 1, 2, TEAR)
    elif mood == "SLEEP":
        arc_eye(f.ex1, False); arc_eye(f.ex2, False)
        s.pattern(f.zx, f.zy, ["XXX", "..X", ".X.", "XXX"], SNORE)
    elif mood == "LOVE":
        heart(f.ex1 + 1); heart(f.ex2 + 1)
    elif mood == "WINK":
        arc_eye(f.ex1); open_eye(f.ex2)
    if f.cheeks and mood in ("IDLE", "HAPPY", "LOVE", "WINK", "TALK", "SLEEP", "BLINK"):
        cy = f.cheek_y if f.cheek_y is not None else f.ey + 3
        s.rect(f.ex1 - f.cheek_dx, cy, 2, 1, f.cheek); s.rect(f.ex2 + f.cheek_dx, cy, 2, 1, f.cheek)
    if not f.mouth:
        return
    if mood in ("IDLE", "LOVE", "BLINK"):
        smile()
    elif mood == "WINK":
        smile(); s.rect(mx + 1, my + 2, 2, 1, HEART)
    elif mood == "HAPPY":
        s.rect(mx, my, 4, 2, INK); s.rect(mx + 1, my + 1, 2, 1, HEART)
    elif mood == "TALK":
        s.rect(mx, my, 4, 3, INK); s.rect(mx + 1, my + 2, 2, 1, HEART)
    elif mood == "SURPRISED":
        s.rect(mx + 1, my, 2, 2, INK)
    elif mood == "SAD":
        s.put(mx, my + 1, INK); s.put(mx + 1, my, INK); s.put(mx + 2, my, INK); s.put(mx + 3, my + 1, INK)
    elif mood == "SLEEP":
        s.rect(mx + 1, my, 2, 1, INK)


# ------------------------------------------------------------------ people

def chibi_body(s, shirt, pants=c("#3B4160"), arms=SKIN, x=10, w=12):
    s.rect(x + 2, 28, 3, 2, pants); s.rect(x + w - 5, 28, 3, 2, pants)
    s.rrect(x, 22, w, 7, 2, shirt)
    s.rect(x - 2, 23, 2, 3, arms); s.rect(x + w, 23, 2, 3, arms)


def boy():
    s = Sprite()
    chibi_body(s, c("#4F8DF5"))
    s.rect(15, 22, 2, 2, WHITE)
    s.rrect(7, 6, 18, 17, 6, SKIN)
    s.rect(6, 14, 1, 3, SKIN); s.rect(25, 14, 1, 3, SKIN)
    s.rect(7, 7, 18, 4, c("#6B4A3A")); s.rect(7, 11, 2, 3, c("#6B4A3A")); s.rect(23, 11, 2, 2, c("#6B4A3A"))
    s.rrect(7, 3, 18, 6, 3, c("#E8574A")); s.rect(7, 7, 18, 2, c("#E8574A"))
    s.rect(18, 8, 9, 2, c("#B83B32"))
    s.rect(14, 4, 4, 2, WHITE)
    s.outline()
    return s, Face(11, 19, 13, 14, 18)


def girl():
    s = Sprite()
    hair, bow = c("#5A3A4A"), c("#FF5C8A")
    s.rect(12, 28, 2, 2, SKIN); s.rect(18, 28, 2, 2, SKIN)
    s.rrect(9, 21, 14, 8, 3, c("#FF8FB0")); s.rect(9, 26, 14, 2, c("#FF8FB0"))
    s.rect(14, 22, 4, 1, WHITE)
    s.rect(7, 22, 2, 3, SKIN); s.rect(23, 22, 2, 3, SKIN)
    s.ellipse(5, 13, 3, 4.5, hair); s.ellipse(27, 13, 3, 4.5, hair)
    s.rrect(7, 5, 18, 18, 7, hair)
    s.rrect(8, 9, 16, 13, 5, SKIN)
    s.pattern(8, 8, ["XXXXXXXXXXXXXXXX", "XXX.XXXXXX.XXXXX", "X....XX......XX."], hair)
    s.rect(7, 12, 1, 8, hair); s.rect(24, 12, 1, 8, hair)
    s.pattern(3, 7, ["XX.XX", ".XXX.", "XX.XX"], bow); s.pattern(24, 7, ["XX.XX", ".XXX.", "XX.XX"], bow)
    s.outline()
    return s, Face(11, 19, 13, 14, 18)


def astronaut():
    s = Sprite()
    suit, grey, visor = c("#F2F4F8"), c("#C9CFDC"), c("#8FB4FF")
    s.rect(11, 28, 3, 2, grey); s.rect(18, 28, 3, 2, grey)
    s.rrect(9, 21, 14, 8, 3, suit)
    s.rect(13, 23, 2, 2, c("#E8574A")); s.rect(17, 23, 2, 2, c("#4F8DF5"))
    s.rect(6, 22, 3, 4, suit); s.rect(23, 22, 3, 4, suit)
    s.ellipse(16, 13, 11.5, 10.5, suit)
    s.rect(5, 14, 2, 4, grey); s.rect(25, 14, 2, 4, grey)
    s.rrect(9, 8, 14, 12, 4, visor)
    s.rrect(10, 9, 12, 10, 3, SKIN)
    s.rect(21, 1, 1, 3, grey); s.rect(20, 0, 3, 1, c("#FFD45C"))
    s.outline()
    s.rect(11, 9, 2, 1, WHITE)
    return s, Face(12, 18, 12, 14, 16, cheek_dx=2)


def wizard():
    s = Sprite()
    robe, trim, hat = c("#6B4BD8"), c("#FFD45C"), c("#5A3CC0")
    s.rrect(8, 21, 16, 9, 3, robe); s.rect(8, 28, 16, 2, robe)
    s.rect(15, 21, 2, 9, trim)
    s.rect(6, 23, 2, 3, SKIN); s.rect(24, 23, 2, 3, SKIN)
    s.rect(27, 12, 1, 18, c("#8A5A3C")); s.rect(26, 10, 3, 3, c("#5CF2FF"))
    s.rrect(8, 9, 16, 14, 5, SKIN)
    s.rect(8, 10, 2, 6, c("#E3E6F0")); s.rect(22, 10, 2, 6, c("#E3E6F0"))
    s.tri_up(15, 0, 9, hat, lean=-2)
    s.rect(9, 8, 14, 1, hat)
    s.rect(5, 9, 22, 2, hat)
    s.rect(9, 8, 14, 1, trim)
    s.pattern(13, 3, [".X.", "XXX", ".X."], trim)
    s.outline()
    return s, Face(11, 19, 13, 14, 18, zx=2, zy=1)


def ninja():
    s = Sprite()
    cloth, band = c("#2F3142"), c("#E8574A")
    s.rect(11, 28, 3, 2, cloth); s.rect(18, 28, 3, 2, cloth)
    s.rrect(9, 21, 14, 8, 3, cloth)
    s.rect(9, 24, 14, 2, band)
    s.rect(7, 22, 2, 3, cloth); s.rect(23, 22, 2, 3, cloth)
    s.rrect(7, 5, 18, 18, 7, cloth)
    s.rrect(9, 11, 14, 6, 2, SKIN)
    s.rect(7, 9, 18, 2, band)
    s.pattern(1, 7, ["XX....", ".XXX..", "...XXX", "..XX..", "XX...."], band)
    s.rect(15, 9, 2, 2, c("#B8BFD6"))
    s.outline()
    return s, Face(11, 19, 12, 14, 18, cheeks=False, mouth=False)


# ------------------------------------------------------------------ animals

def cat():
    s = Sprite()
    fur, dark, muzzle, pink = c("#FFB25B"), c("#E8893A"), c("#FFF1DE"), c("#FF9AA8")
    s.rect(10, 27, 4, 3, fur); s.rect(18, 27, 4, 3, fur)
    s.rrect(9, 22, 14, 7, 3, fur)
    s.rect(25, 18, 2, 8, fur); s.rect(27, 16, 2, 3, fur)
    for i in range(7):
        s.rect(6, 4 + i, 1 + i, 1, fur); s.rect(26 - i, 4 + i, 1 + i, 1, fur)
    s.put(7, 8, pink); s.put(7, 9, pink); s.put(8, 9, pink)
    s.put(25, 8, pink); s.put(25, 9, pink); s.put(24, 9, pink)
    s.rrect(4, 8, 24, 16, 5, fur)
    s.rect(14, 9, 4, 1, dark); s.rect(15, 10, 2, 1, dark)
    s.rect(4, 15, 3, 1, dark); s.rect(25, 15, 3, 1, dark)
    s.rrect(11, 17, 10, 6, 3, muzzle)
    s.rect(15, 17, 2, 1, pink)
    s.rect(10, 23, 12, 1, c("#5CC8FF"))
    s.outline()
    return s, Face(10, 20, 13, 14, 18, cheek_dx=4)


def shiba():
    s = Sprite()
    fur, cream, inner = c("#E89A4E"), c("#FFF1DE"), c("#FFC79A")
    s.rect(10, 27, 4, 3, cream); s.rect(18, 27, 4, 3, cream)
    s.rrect(9, 22, 14, 7, 3, fur); s.rect(12, 23, 8, 5, cream)
    s.rrect(23, 19, 5, 5, 2, fur); s.rect(24, 20, 2, 2, cream)
    for i in range(6):
        s.rect(7 + i // 3, 3 + i, 2 + i, 1, fur); s.rect(24 - i - i // 3, 3 + i, 2 + i, 1, fur)
    s.rect(9, 6, 2, 3, inner); s.rect(22, 6, 2, 3, inner)
    s.rrect(5, 8, 22, 16, 6, fur)
    s.rrect(8, 15, 16, 9, 4, cream)
    s.rect(11, 11, 2, 1, cream); s.rect(19, 11, 2, 1, cream)
    s.rect(15, 16, 2, 1, INK)
    s.outline()
    return s, Face(11, 19, 13, 14, 18, cheek_dx=3)


def rabbit():
    s = Sprite()
    fur, pink, shade = c("#FBF7FF"), c("#FFB3C7"), c("#E6DDF0")
    s.rect(11, 28, 4, 2, fur); s.rect(17, 28, 4, 2, fur)
    s.rrect(9, 22, 14, 7, 3, fur); s.rect(13, 24, 6, 4, shade)
    s.rrect(9, 0, 5, 12, 2, fur); s.rrect(18, 0, 5, 12, 2, fur)
    s.rect(11, 2, 1, 8, pink); s.rect(20, 2, 1, 8, pink)
    s.rrect(6, 8, 20, 16, 7, fur)
    s.rect(15, 16, 2, 1, pink)
    s.outline()
    return s, Face(11, 19, 13, 14, 18)


def panda():
    s = Sprite()
    white, black = c("#FFFFFF"), c("#33303F")
    s.rect(10, 27, 4, 3, black); s.rect(18, 27, 4, 3, black)
    s.rrect(9, 21, 14, 8, 3, white)
    s.rect(7, 22, 3, 4, black); s.rect(22, 22, 3, 4, black)
    s.ellipse(8, 7, 3.2, 3.2, black); s.ellipse(24, 7, 3.2, 3.2, black)
    s.rrect(5, 7, 22, 17, 7, white)
    s.ellipse(11.5, 14.5, 3, 3.5, black); s.ellipse(20.5, 14.5, 3, 3.5, black)
    s.rect(15, 17, 2, 1, black)
    s.outline()
    return s, Face(11, 20, 13, 14, 19, eye=WHITE, cheek_dx=4)


def chick():
    s = Sprite()
    yellow, orange, wing = c("#FFD447"), c("#FF9A3C"), c("#F5BD2E")
    s.rect(12, 28, 1, 2, orange); s.rect(19, 28, 1, 2, orange)
    s.rect(11, 29, 3, 1, orange); s.rect(18, 29, 3, 1, orange)
    s.ellipse(16, 18, 11, 10.5, yellow)
    s.pattern(14, 6, ["X.X", ".XX"], yellow); s.put(16, 5, yellow)
    s.rect(4, 18, 3, 4, wing); s.rect(25, 18, 3, 4, wing)
    s.pattern(14, 17, ["XXXX", ".XX."], orange)
    s.outline()
    return s, Face(11, 19, 13, 14, 18, mouth=False, cheek_dx=3)


def fox():
    s = Sprite()
    fur, white, dark = c("#FF7A3D"), c("#FFF4EA"), c("#3A2A3A")
    s.rect(10, 27, 4, 3, dark); s.rect(18, 27, 4, 3, dark)
    s.rrect(9, 22, 14, 7, 3, fur); s.rect(13, 23, 6, 5, white)
    s.ellipse(26, 22, 3.5, 5, fur); s.rect(25, 25, 4, 2, white)
    for i in range(8):
        s.rect(6, 1 + i, 1 + i, 1, fur); s.rect(26 - i, 1 + i, 1 + i, 1, fur)
    s.rect(6, 1, 1, 2, dark); s.rect(26, 1, 1, 2, dark)
    s.rrect(4, 8, 24, 15, 5, fur)
    s.rrect(9, 15, 14, 8, 4, white)
    s.rect(4, 16, 5, 3, white); s.rect(23, 16, 5, 3, white)
    s.rect(15, 17, 2, 1, INK)
    s.outline()
    return s, Face(10, 20, 12, 14, 19, cheek_dx=3)


def frog():
    s = Sprite()
    green, belly, dark = c("#6DD36B"), c("#D2F7B4"), c("#4BAF55")
    s.rect(7, 27, 5, 3, green); s.rect(20, 27, 5, 3, green)
    s.ellipse(10, 9, 4.5, 4.5, green); s.ellipse(22, 9, 4.5, 4.5, green)
    s.rrect(4, 9, 24, 19, 7, green)
    s.rrect(9, 19, 14, 8, 4, belly)
    s.rect(6, 25, 2, 2, dark); s.rect(24, 25, 2, 2, dark)
    s.outline()
    s.rrect(8, 6, 5, 6, 2, WHITE); s.rrect(20, 6, 5, 6, 2, WHITE)
    return s, Face(9, 21, 7, 14, 15, cheek_y=13, cheek_dx=2, zx=26, zy=1)


def penguin():
    s = Sprite()
    navy, white, orange = c("#2E3A5C"), c("#FFFFFF"), c("#FF9A3C")
    s.rect(10, 28, 4, 2, orange); s.rect(18, 28, 4, 2, orange)
    s.ellipse(16, 17, 11, 12.5, navy)
    s.rect(3, 16, 3, 7, navy); s.rect(26, 16, 3, 7, navy)
    s.ellipse(16, 18, 8, 9.5, white)
    s.pattern(14, 16, ["XXXX", ".XX."], orange)
    s.outline()
    return s, Face(11, 19, 12, 14, 18, mouth=False, cheek_dx=3)


# ----------------------------------------------------------------- fantasy

def fairy():
    s = Sprite()
    hair, wing, dress, flower = c("#B48CFF"), c("#C9F4FF"), c("#6DD3A0"), c("#FF8FB0")
    s.ellipse(5, 17, 4, 6, wing); s.ellipse(27, 17, 4, 6, wing)
    s.ellipse(6, 24, 3, 3, wing); s.ellipse(26, 24, 3, 3, wing)
    s.rect(13, 28, 2, 2, SKIN); s.rect(17, 28, 2, 2, SKIN)
    s.rrect(10, 21, 12, 6, 2, dress); s.rect(10, 26, 12, 2, dress)
    s.rect(8, 22, 2, 3, SKIN); s.rect(22, 22, 2, 3, SKIN)
    s.rrect(7, 4, 18, 18, 7, hair)
    s.rrect(9, 9, 14, 12, 5, SKIN)
    s.pattern(9, 8, ["XXXXXXXXXXXXXX", "XX.XXXX.XXX.XX", "X....X....X..X"], hair)
    s.pattern(18, 2, [".X.X.", "XXXXX", ".XXX.", "XXXXX", ".X.X."], flower); s.put(20, 4, c("#FFD45C"))
    s.outline()
    s.put(4, 14, WHITE); s.put(26, 14, WHITE)
    return s, Face(11, 19, 13, 14, 18, zx=2, zy=1)


def ghost():
    s = Sprite()
    body, shade = c("#F4F2FF"), c("#D5D3F5")
    s.ellipse(16, 14, 11, 11, body)
    s.rect(5, 14, 22, 12, body)
    for x in range(5, 27):
        for y in range(21, 27):
            if (x, y) in s.p:
                s.p[(x, y)] = shade
    s.pattern(5, 26, ["XXX..XXXX..XXXX..XXX"], shade)
    s.rect(3, 15, 2, 3, body); s.rect(27, 15, 2, 3, body)
    s.outline()
    return s, Face(11, 19, 12, 14, 17)


def slime():
    s = Sprite()
    a, b, hi = c("#FF7EB8"), c("#D77BFF"), c("#FFE1F0")
    s.ellipse(16, 16, 12, 10, a)
    s.rect(4, 16, 24, 10, a)
    for x in range(4, 28):
        for y in range(21, 27):
            if (x, y) in s.p:
                s.p[(x, y)] = b
    for x, d in ((5, 2), (9, 1), (13, 3), (18, 1), (22, 2), (26, 1)):
        s.rect(x, 26, 2, d, b)
    s.outline()
    s.rect(8, 9, 4, 2, hi); s.rect(7, 11, 2, 3, hi); s.put(13, 8, hi)
    return s, Face(11, 19, 14, 14, 19, cheek=c("#FF4F8B"))


def dragon():
    s = Sprite()
    scale, belly, horn, wing = c("#4FCB9A"), c("#FFF1C9"), c("#FFE08A"), c("#8E7CFF")
    s.ellipse(4, 17, 3.5, 5, wing); s.ellipse(28, 17, 3.5, 5, wing)
    s.rect(10, 27, 4, 3, scale); s.rect(18, 27, 4, 3, scale)
    s.rrect(9, 21, 14, 8, 3, scale); s.rect(13, 22, 6, 6, belly)
    s.rect(24, 25, 4, 2, scale); s.rect(27, 23, 2, 3, scale)
    s.tri_up(10, 2, 5, horn); s.tri_up(22, 2, 5, horn)
    s.rrect(5, 6, 22, 17, 7, scale)
    s.pattern(14, 4, ["X.X", "XXX"], c("#3FAE84"))
    s.rrect(10, 16, 12, 7, 3, belly)
    s.put(14, 17, INK); s.put(17, 17, INK)
    s.outline()
    return s, Face(10, 20, 12, 14, 19, cheek_dx=3)


def mushroom():
    s = Sprite()
    cap, spot, stem = c("#E8574A"), c("#FFF6EC"), c("#FFE9D2")
    s.rect(11, 28, 4, 2, stem); s.rect(17, 28, 4, 2, stem)
    s.rrect(9, 13, 14, 16, 5, stem)
    s.rect(7, 20, 2, 3, stem); s.rect(23, 20, 2, 3, stem)
    s.ellipse(16, 10, 14, 8.5, cap)
    s.rect(2, 10, 28, 4, cap)
    s.rrect(8, 3, 6, 4, 1, spot); s.rrect(20, 5, 5, 4, 1, spot); s.rect(3, 9, 3, 3, spot); s.rect(26, 10, 3, 3, spot)
    s.rect(15, 6, 3, 2, spot)
    s.outline()
    return s, Face(11, 19, 17, 14, 22, zx=26, zy=0)


def cubey():
    s = Sprite()
    cream, shade, top = c("#FFF4E2"), c("#EAD3BA"), c("#FFFFFF")
    s.rect(11, 27, 3, 2, shade); s.rect(18, 27, 3, 2, shade)
    s.rrect(6, 8, 20, 19, 3, cream)
    s.rect(7, 23, 18, 3, shade)
    s.rect(8, 9, 16, 3, top)
    for x, y in ((10, 21), (21, 13), (23, 19), (9, 13)):
        s.put(x, y, c("#F0DEC6"))
    s.rect(4, 17, 2, 3, cream); s.rect(26, 17, 2, 3, cream)
    s.outline()
    s.pattern(25, 3, [".X.", "XXX", ".X."], c("#FFD45C"))
    return s, Face(11, 19, 15, 14, 19, zx=2, zy=3)


def chippy():
    s = Sprite()
    body, pin, screen, led = c("#3B4160"), c("#B8BFD6"), c("#7CF2C9"), c("#FF5C6C")
    for y in range(12, 25, 3):
        s.rect(4, y, 3, 1, pin); s.rect(25, y, 3, 1, pin)
    for x in range(10, 23, 3):
        s.rect(x, 27, 1, 3, pin)
    s.rect(15, 4, 1, 4, pin); s.rect(14, 2, 3, 2, led)
    s.rrect(7, 8, 18, 19, 2, body)
    s.rrect(9, 11, 14, 10, 2, screen)
    s.rect(10, 12, 12, 1, c("#B5FFE6"))
    s.put(9, 23, c("#FFD45C")); s.put(11, 23, c("#5CC8FF"))
    s.outline()
    return s, Face(11, 18, 14, 14, 18, cheeks=False, highlight=False, zx=24, zy=1)


CHARACTERS = [
    ("boy", "小宇", "PEOPLE", boy), ("girl", "小桃", "PEOPLE", girl), ("astronaut", "星航员", "PEOPLE", astronaut),
    ("wizard", "小法师", "PEOPLE", wizard), ("ninja", "影忍", "PEOPLE", ninja),
    ("cat", "比特猫", "ANIMAL", cat), ("shiba", "柴柴", "ANIMAL", shiba), ("rabbit", "棉花兔", "ANIMAL", rabbit),
    ("panda", "团团", "ANIMAL", panda), ("chick", "啾啾", "ANIMAL", chick), ("fox", "小狐", "ANIMAL", fox),
    ("frog", "呱呱", "ANIMAL", frog), ("penguin", "企企", "ANIMAL", penguin),
    ("fairy", "花仙子", "FANTASY", fairy), ("ghost", "飘飘", "FANTASY", ghost), ("slime", "软软", "FANTASY", slime),
    ("dragon", "小龙", "FANTASY", dragon), ("mushroom", "菇菇", "FANTASY", mushroom),
    ("cubey", "方糖仔", "FANTASY", cubey), ("chippy", "芯芯", "FANTASY", chippy),
]


def build():
    result = []
    for ident, name, family, fn in CHARACTERS:
        base, face = fn()
        frames = []
        for mood in FRAMES:
            s = base.copy()
            draw_face(s, face, mood)
            frames.append(s)
        colors = []
        for s in frames:
            for col in s.p.values():
                if col not in colors:
                    colors.append(col)
        if len(colors) > 15:
            raise SystemExit(f"{ident}: {len(colors)} colors exceed the 15-color palette")
        result.append((ident, name, family, colors, frames))
    return result


def encode(colors, sprite):
    index = {col: i + 1 for i, col in enumerate(colors)}
    data = bytearray()
    for y in range(N):
        for x in range(0, N, 2):
            a = index.get(sprite.p.get((x, y)), 0)
            b = index.get(sprite.p.get((x + 1, y)), 0)
            data.append((a << 4) | b)
    return data


def write_c(chars):
    COMPONENT.mkdir(parents=True, exist_ok=True)
    (COMPONENT / "include").mkdir(exist_ok=True)
    h = ["/* Generated by tools/make_chars.py. */",
         "/** Identifiers of the built-in pixel characters and moods. */",
         "#ifndef SP_CHARS_H", "#define SP_CHARS_H", "",
         "typedef enum {"] + [f"    SP_CHAR_{i.upper()}," for i, *_ in chars] + ["    SP_CHAR_COUNT,", "} sp_char_id_t;", "",
         "typedef enum {"] + [f"    SP_MOOD_{m}," for m in MOODS] + ["    SP_MOOD_COUNT,", "} sp_mood_t;", "",
         "/** Frame index of the closed-eye frame used for blinking. */", f"#define SP_FRAME_BLINK {len(MOODS)}",
         f"#define SP_FRAME_COUNT {len(FRAMES)}", f"#define SP_SPRITE_SIZE {N}", f"#define SP_FRAME_BYTES {N * N // 2}",
         "", "#endif", ""]
    (COMPONENT / "include/sp_chars.h").write_text("\n".join(h))
    out = ["/* Generated by tools/make_chars.py; 4-bit indexed, index 0 is transparent. */",
           '#include "sugarat_pixel.h"', ""]
    for ident, name, family, colors, frames in chars:
        out.append(f"static const uint32_t {ident}_palette[16] = {{0, " + ", ".join(f"0x{x}" for x in colors) + "};")
        out.append(f"static const uint8_t {ident}_frames[SP_FRAME_COUNT][SP_FRAME_BYTES] = {{")
        for mood, s in zip(FRAMES, frames):
            data = encode(colors, s)
            out.append(f"    /* {mood} */ {{")
            out += ["        " + ",".join(f"0x{b:02x}" for b in data[i:i + 32]) + "," for i in range(0, len(data), 32)]
            out.append("    },")
        out += ["};", ""]
    out.append("const sp_char_t sp_characters[SP_CHAR_COUNT] = {")
    for ident, name, family, colors, frames in chars:
        out.append(f'    [SP_CHAR_{ident.upper()}] = {{"{ident}", "{name}", SP_FAMILY_{family}, {ident}_palette, {ident}_frames}},')
    out += ["};", ""]
    (COMPONENT / "src/sp_chars.c").write_text("\n".join(out))


def proof(chars, path):
    scale = 4
    cols = len(FRAMES)
    img = Image.new("RGB", (cols * (N * scale + 8) + 8, len(chars) * (N * scale + 8) + 8), (40, 38, 56))
    for r, (ident, name, family, colors, frames) in enumerate(chars):
        for k, s in enumerate(frames):
            tile = Image.new("RGBA", (N, N), (0, 0, 0, 0))
            for (x, y), col in s.p.items():
                tile.putpixel((x, y), tuple(int(col[i:i + 2], 16) for i in (0, 2, 4)) + (255,))
            tile = tile.resize((N * scale, N * scale), Image.Resampling.NEAREST)
            img.paste(tile, (8 + k * (N * scale + 8), 8 + r * (N * scale + 8)), tile)
    img.save(path)


def main():
    chars = build()
    write_c(chars)
    if len(sys.argv) > 1:
        proof(chars, sys.argv[1])
    for ident, name, family, colors, _ in chars:
        print(f"{ident:10s} {name}  {len(colors)} colors")


if __name__ == "__main__":
    main()
