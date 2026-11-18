# SDGOODS Game UI components

English | [中文](README.zh.md)

`sugarat_game` is a retro pixel-game LVGL 8.3.11 component library for the 360x360 round SDGOODS badge: night-blue background, beveled pixel boxes with hard shadows, block-drawn rings and charts, 1-bit pixel Chinese text, 71 pixel icons and RPG components. Build every application screen from it instead of styling raw LVGL widgets, drawing icons or adding fonts. Install `sdgoods-ui-game` with `target=esp-idf` under the firmware project root, which also installs `sdgoods-pixel-characters`, and add `sugarat_game` to `main/REQUIRES`. `include/sugarat_game.h` documents every function; `examples/sg_gallery.c` shows each component in a complete screen.

## Choosing a style

`sdgoods-ui-watch` (`sugarat_watch`, prefix `sw_`) has the same functions, enums, icon names and argument order with a watchOS look. Pick one style per application. A screen written for one switches style by renaming `sg_`/`SG_` to `sw_`/`SW_`, `sugarat_game.h` to `sugarat_watch.h` and the `REQUIRES` entry; the Game-only components and `sg_box` have no Watch counterpart.

## Building a screen

Call every function on the LVGL thread after `lv_init()` and `sg_init()`. Start each screen with `sg_screen()`. For scrolling content, create `sg_page(screen, title)` and add sections, cards, list items and controls; `sg_scroll_indicator(page)` adds the position arc. Place fixed content with `lv_obj_align()` or `sg_row()`/`sg_column()`. Keep tap targets and text inside the 240x240 safe rectangle at (60,60) unless a component documents edge placement, such as `sg_volume`, `sg_status_bar` and `sg_page_dots`.

```c
lv_obj_t *screen = sg_screen();
lv_obj_t *dialog = sg_dialog(screen, "小法师", "固件已编译完成，现在烧录吗？");
lv_obj_align(dialog, LV_ALIGN_BOTTOM_MID, 0, -60);
sp_avatar(sg_dialog_portrait(dialog), SP_CHAR_WIZARD, 2, SP_MOOD_TALK);
static const char *const options[] = {"继续游戏", "新的冒险", "设置"};
lv_obj_align(sg_menu(screen, options, 3, 0), LV_ALIGN_TOP_MID, 0, 70);
lv_obj_add_event_cb(screen, on_tap, LV_EVENT_CLICKED, NULL);
lv_scr_load(screen);
```

Use tones (`SG_TONE_*`) and text roles (`SG_FONT_*`) for any extra label instead of colors and fonts. Create text with `sg_label` and icons with `sg_icon`. For a custom panel, call `sg_box(obj, style, tone, shadow)` on a plain object with no background, border or shadow style of its own; `SG_BOX_PANEL`, `BUTTON`, `FRAME` and `INSET` draw the bevels on the `SG_UNIT` (2 px) grid, and shadowed boxes sink into their shadow while pressed. Keep sizes and offsets multiples of `SG_UNIT` so edges stay crisp.

## Taps and values

Interactive parts send `LV_EVENT_CLICKED` with event bubbling, so one handler on a component, page or screen receives every tap below it. Tag a button, list item or app icon with `sg_set_action(obj, id)` using 0..999, then read `sg_event_action(e)` in the handler; it walks from the tapped object to the handler's object and returns the first tag or `SG_ACTION_NONE`. Built-in parts report `SG_ACTION_CONFIRM`/`CANCEL` (alerts), `PREV`/`PLAY`/`NEXT` (`sg_now_playing`) and `VOICE` (`sg_voice_button`); `sg_menu` options and `sg_inventory` slots report their index. `SG_ACTION_BACK` is reserved for an application's own back button. `sg_set_action` uses the object's LVGL user data, so do not store other pointers there. Toggles, sliders, steppers, segmented controls and rollers send `LV_EVENT_VALUE_CHANGED`; read them with the getter in the header or the LVGL getter it names.

## Component catalog

| Need | Functions |
| --- | --- |
| Foundation | `sg_init`, `sg_screen`, `sg_label`, `sg_icon`/`_set`, `sg_color`, `sg_font`, `sg_box`/`_set_tone`, `sg_set_action`, `sg_event_action` |
| Layout | `sg_page`, `sg_row`, `sg_column`, `sg_card`, `sg_section`, `sg_status_bar`/`_set_time`/`_set_battery`/`_set_connected`, `sg_page_dots`/`_set`, `sg_scroll_indicator` |
| Controls | `sg_button`, `sg_icon_button`, `sg_toggle_button`, `sg_toggle`, `sg_checkbox`, `sg_slider`, `sg_stepper`/`_get`/`_set`, `sg_segmented`/`_get`, `sg_progress`/`_set`, `sg_ring`/`_set`, `sg_spinner`, `sg_roller` |
| Lists | `sg_list_item`, `sg_list_item_value`, `sg_list_item_toggle`, `sg_list_item_chevron` |
| Feedback | `sg_badge`, `sg_toast`, `sg_notification`, `sg_alert`, `sg_empty_state`, `sg_loading` |
| Data | `sg_stat`/`_set`, `sg_gauge`/`_set`, `sg_tile`/`_set`, `sg_bar_chart`/`_set`, `sg_sparkline`/`_set` |
| Time | `sg_clock`/`_set`, `sg_analog_clock`/`_set`, `sg_timer`/`_set` |
| Media | `sg_now_playing`/`_set`, `sg_volume`/`_set` |
| AI companion | `sg_voice_button`/`_set`, `sg_waveform`/`_set_active`, `sg_chat_bubble`, `sg_avatar_frame` |
| Launcher | `sg_app_grid`, `sg_app_icon` |
| Game only | `sg_hp_bar`/`_set`, `sg_hearts`/`_set`, `sg_dialog`/`_set_text`/`_skip`/`_portrait`, `sg_menu`/`_set_selected`/`_get_selected`, `sg_counter`/`_set`, `sg_level`/`_set`, `sg_inventory`/`_set`/`_select`, `sg_achievement`, `sg_title_screen` |

Icons are `SG_ICON_*` in `include/sg_icons.h`, 8x8 pixel art at 16, 24, 32 and 40 px (`SG_SIZE_S` to `XL`). For a character, put `sp_avatar()` from `sugarat_pixel.h` inside `sg_avatar_frame()` or `sg_dialog_portrait()` and add `sugarat_pixel` to `REQUIRES`.

## Gallery examples

`examples/sg_gallery.c` builds 28 screens from the public API only: title screen, HUD, dialog, portrait dialog, menu, inventory, achievement, watch face, analog face, app grid, settings, buttons, selection, values, picker, music, timer, notification, alert, loading, empty state, control center, AI companion, chat, charts, tiles, voice states and the icon set. Copy the closest screen as the starting point for an application screen. To run them, add `examples/sg_gallery.c` to the application's sources and `examples/` to its include paths, add `sugarat_pixel` to `REQUIRES`, then load `sg_gallery_create(index)` for `0..sg_gallery_count()-1`.

## Text and fonts

Text fonts are 1-bit pixel fonts: Chinese from Source Han Sans SC rendered at 16 px or at 12 px and scaled 2x or 3x, Latin letters and digits from unscii-8 scaled to match. Glyphs are 16 px (caption), 24 px (body, headline) and 36 px (title), with digits at 32 and 56 px. The 16 and 24 px fonts hold printable ASCII, common punctuation, the 1119 Chinese characters in `tools/common-hanzi.txt` and characters found in the component sources; 36 px holds only source characters. Characters outside a font render as blanks. When an application shows other characters, run `python3 tools/make_fonts.py --font-dir <lvgl>/scripts/built_in_font --sources ../../main` from the component directory; it needs Pillow and the LVGL 8.3 font directory, collects non-ASCII characters from string literals and rewrites `src/fonts/`. Text that only arrives at run time, such as chat replies, is limited to the bundled set. Icon art lives in `tools/make_icons.py`, which regenerates the icon fonts.

## Memory and performance

The library takes about 0.19 MB of flash, mostly fonts and icons, and 0.3 KB of static RAM. Labels share static font and tone styles. Measured on a host build with the device's 32 KB LVGL heap, gallery screens use 0.8-8 KB on top of a 9.1 KB baseline; inventory and settings use 12-13 KB, and the 71-icon test screen 16 KB, for a peak of 25.2 KB. Keep one application screen alive: load the new screen, then delete the old one with `lv_scr_load_anim(..., true)` or `lv_obj_del()`. For more simultaneous screens, raise `CONFIG_LV_MEM_SIZE_KILOBYTES` or move the LVGL heap to PSRAM. The typewriter, waveform, spinner and blinking prompts run LVGL timers only while their objects exist.

## Licenses and verification

The code and the pixel icon art are Apache-2.0. Chinese glyph subsets follow `OFL.txt`; unscii-8 is in the public domain. The component and gallery compile without warnings against the SDGOODS template with ESP-IDF 5.5.4. Every gallery screen was rendered on a host LVGL 8.3.11 build with a 32 KB heap and reviewed visually; the component has not yet run on the device. Verify touch targets, scrolling smoothness, text coverage and memory of each new application on the device.
