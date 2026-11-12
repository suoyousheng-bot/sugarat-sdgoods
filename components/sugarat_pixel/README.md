# SDGOODS pixel characters

English | [中文](README.zh.md)

`sugarat_pixel` is a library of 20 animated 32x32 pixel characters with 8 moods for LVGL 8.3.11 on the SDGOODS round badge. Use it for mascots, companions, game heroes and dialog portraits instead of drawing characters or embedding sprite images. `sdgoods-ui-watch` and `sdgoods-ui-game` install it as a dependency; it can also be installed alone with `target=esp-idf` under the firmware project root. Add `sugarat_pixel` to `main/REQUIRES` and include `sugarat_pixel.h`.

## Showing a character

Call every function on the LVGL thread. `sp_avatar(parent, id, scale, mood)` creates an object of 32 x `scale` pixels square (scale 1..10) that draws the character with crisp pixel edges and animates it. `sp_avatar_set_mood()` changes the expression, `sp_avatar_set_char()` swaps the character while keeping scale and mood, and `sp_avatar_set_animated(avatar, false)` holds the mood frame still. Inside the UI libraries, place it in `sw_avatar_frame()`, `sg_avatar_frame()` or `sg_dialog_portrait()`; scale 2 fits a 72 px portrait and scale 4 a 150 px frame.

```c
lv_obj_t *buddy = sp_avatar(frame, SP_CHAR_CUBEY, 4, SP_MOOD_HAPPY);
lv_obj_center(buddy);
sp_avatar_set_mood(buddy, SP_MOOD_TALK);
```

The avatar keeps its state in its LVGL user data, so do not call `sw_set_action()`/`sg_set_action()` on it; tag its frame or another parent instead. Each avatar runs a 100 ms LVGL timer that the object deletes with itself.

## Characters and moods

| Family | Characters (`SP_CHAR_*`, display name) |
| --- | --- |
| People | `BOY` 小宇, `GIRL` 小桃, `ASTRONAUT` 星航员, `WIZARD` 小法师, `NINJA` 影忍 |
| Animals | `CAT` 比特猫, `SHIBA` 柴柴, `RABBIT` 棉花兔, `PANDA` 团团, `CHICK` 啾啾, `FOX` 小狐, `FROG` 呱呱, `PENGUIN` 企企 |
| Fantasy | `FAIRY` 花仙子, `GHOST` 飘飘, `SLIME` 软软, `DRAGON` 小龙, `MUSHROOM` 菇菇, `CUBEY` 方糖仔, `CHIPPY` 芯芯 |

Moods are `SP_MOOD_IDLE` (bobs and blinks), `HAPPY` (hops), `SURPRISED`, `SAD`, `SLEEP`, `LOVE`, `TALK` (mouth moves; use it while speech plays) and `WINK`. `sp_char_get(id)` returns the static record with the id, display name, family, palette and frames for custom drawing.

## Memory and changes

All characters take about 95 KB of flash as 4-bit indexed frames; each avatar adds one LVGL object, a small state block and a timer to the LVGL heap. Drawing merges runs of equal pixels into rectangles, so a scale-4 avatar redraws quickly, but avoid more than a few animated avatars on one screen. Characters are defined as code in `tools/make_chars.py`; to add or change one, edit it and run `python3 tools/make_chars.py proof.png` from the component directory (requires Pillow), which rewrites `src/sp_chars.c` and `include/sp_chars.h` and writes a proof sheet. The art is original and Apache-2.0. The component compiles without warnings against the SDGOODS template with ESP-IDF 5.5.4, and frames were rendered on a host LVGL 8.3.11 build and reviewed visually; the component has not yet run on the device.
