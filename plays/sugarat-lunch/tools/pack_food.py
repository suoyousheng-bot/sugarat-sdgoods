"""Pack pixel food PNGs as SDGOODS LVGL RGB565 big-endian plus alpha."""

from pathlib import Path

from PIL import Image

food_dir = Path(__file__).resolve().parents[1] / "main" / "food"
for source in sorted(food_dir.glob("*.png")):
    with Image.open(source) as png:
        pixels = png.convert("RGBA").resize((160, 160), Image.Resampling.NEAREST)
    packed = bytearray()
    for red, green, blue, alpha in pixels.getdata():
        color = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
        packed.extend((color >> 8, color & 255, alpha))
    source.with_suffix(".bin").write_bytes(packed)
