# SDGOODS Watch UI components

English | [中文](README.zh.md)

`sugarat_watch` is a watchOS-style LVGL 8.3.11 component library for the 360x360 round SDGOODS badge: black background, rounded cards, capsule buttons, anti-aliased Chinese text and 71 icons. Build every application screen from it instead of styling raw LVGL widgets, drawing icons or adding fonts. Install `sdgoods-ui-watch` with `target=esp-idf` under the firmware project root, which also installs `sdgoods-pixel-characters`, and add `sugarat_watch` to `main/REQUIRES`. `include/sugarat_watch.h` documents every function; `examples/sw_gallery.c` shows each component in a complete screen.

## Choosing a style

`sdgoods-ui-game` (`sugarat_game`, prefix `sg_`) has the same functions, enums, icon names and argument order with a retro pixel-game look and extra RPG components. Pick one style per application. A screen written for one switches style by renaming `sw_`/`SW_` to `sg_`/`SG_`, `sugarat_watch.h` to `sugarat_game.h` and the `REQUIRES` entry; only `sw_activity_rings` has no Game counterpart.

## Building a screen

Call every function on the LVGL thread after `lv_init()` and `sw_init()`. Start each screen with `sw_screen()`. For scrolling content, create `sw_page(screen, title)` and add sections, cards, list items and controls; `sw_scroll_indicator(page)` adds the crown-style position arc. Place fixed content with `lv_obj_align()` or `sw_row()`/`sw_column()`. Keep tap targets and text inside the 240x240 safe rectangle at (60,60) unless a component documents edge placement, such as `sw_volume`, `sw_status_bar` and `sw_page_dots`.

```c
lv_obj_t *screen = sw_screen();
lv_obj_t *page = sw_page(screen, "设置");
lv_obj_t *wifi = sw_list_item(page, SW_ICON_WIFI, SW_TONE_BLUE, "无线网络", "Sugarat-Lab");
sw_list_item_toggle(wifi, true);
sw_set_action(sw_list_item(page, SW_ICON_BULB, SW_TONE_ORANGE, "亮度与显示", NULL), 1);
lv_obj_add_event_cb(page, on_settings, LV_EVENT_CLICKED, NULL);
lv_obj_add_event_cb(page, on_settings, LV_EVENT_VALUE_CHANGED, NULL);
sw_scroll_indicator(page);
lv_scr_load(screen);
```

Use tones (`SW_TONE_*`) and text roles (`SW_FONT_*`) for any extra label instead of colors and fonts. Create text with `sw_label` and icons with `sw_icon`; when raw LVGL is unavoidable, style it with `sw_color()` and `sw_font()` so it matches.

## Taps and values

Interactive parts send `LV_EVENT_CLICKED` with event bubbling, so one handler on a component, page or screen receives every tap below it. Tag a button, list item or app icon with `sw_set_action(obj, id)` using 0..999, then read `sw_event_action(e)` in the handler; it walks from the tapped object to the handler's object and returns the first tag or `SW_ACTION_NONE`. Built-in parts report `SW_ACTION_CONFIRM`/`CANCEL` (alerts), `PREV`/`PLAY`/`NEXT` (`sw_now_playing`) and `VOICE` (`sw_voice_button`); `SW_ACTION_BACK` is reserved for an application's own back button. `sw_set_action` uses the object's LVGL user data, so do not store other pointers there. Toggles, sliders, steppers, segmented controls and rollers send `LV_EVENT_VALUE_CHANGED`; read them with the getter in the header or the LVGL getter it names.

## Component catalog

| Need | Functions |
| --- | --- |
| Foundation | `sw_init`, `sw_screen`, `sw_label`, `sw_icon`/`_set`, `sw_color`, `sw_font`, `sw_set_action`, `sw_event_action` |
| Layout | `sw_page`, `sw_row`, `sw_column`, `sw_card`, `sw_section`, `sw_status_bar`/`_set_time`/`_set_battery`/`_set_connected`, `sw_page_dots`/`_set`, `sw_scroll_indicator` |
| Controls | `sw_button`, `sw_icon_button`, `sw_toggle_button`, `sw_toggle`, `sw_checkbox`, `sw_slider`, `sw_stepper`/`_get`/`_set`, `sw_segmented`/`_get`, `sw_progress`/`_set`, `sw_ring`/`_set`, `sw_spinner`, `sw_roller` |
| Lists | `sw_list_item`, `sw_list_item_value`, `sw_list_item_toggle`, `sw_list_item_chevron` |
| Feedback | `sw_badge`, `sw_toast`, `sw_notification`, `sw_alert`, `sw_empty_state`, `sw_loading` |
| Data | `sw_stat`/`_set`, `sw_gauge`/`_set`, `sw_tile`/`_set`, `sw_bar_chart`/`_set`, `sw_sparkline`/`_set` |
| Time | `sw_clock`/`_set`, `sw_analog_clock`/`_set`, `sw_timer`/`_set` |
| Media | `sw_now_playing`/`_set`, `sw_volume`/`_set` |
| AI companion | `sw_voice_button`/`_set`, `sw_waveform`/`_set_active`, `sw_chat_bubble`, `sw_avatar_frame` |
| Launcher | `sw_app_grid`, `sw_app_icon` |
| Watch only | `sw_activity_rings`/`_set` |

Icons are `SW_ICON_*` in `include/sw_icons.h` at 16, 20, 28 and 40 px (`SW_SIZE_S` to `XL`). For a character, put `sp_avatar()` from `sugarat_pixel.h` inside `sw_avatar_frame()` and add `sugarat_pixel` to `REQUIRES`.

## Gallery examples

`examples/sw_gallery.c` builds 22 screens from the public API only: watch face, analog face, activity, app grid, settings, buttons, selection, values, picker, music, timer, workout, notification, alert, loading, empty state, control center, AI companion, chat, charts, tiles and voice states. Copy the closest screen as the starting point for an application screen. To run them, add `examples/sw_gallery.c` to the application's sources and `examples/` to its include paths, add `sugarat_pixel` to `REQUIRES`, then load `sw_gallery_create(index)` for `0..sw_gallery_count()-1`.

## Text and fonts

Text fonts are 4-bit anti-aliased Source Han Sans subsets at 14, 16, 20 and 28 px, with Montserrat digits at 24, 40 and 80 px. This application expands 20 px to the basic CJK block and gives 28 px that fallback; 14 px and 16 px retain common and source-text glyphs. Emoji are unsupported. Use the application's `tools/prepare_assets.py --font-dir <lvgl>/scripts/built_in_font` to preserve those choices. `tools/make_icons.py` regenerates the fixed icon fonts.

## Memory and performance

The shared Chinese subsets include the time-case labels and companion character names. New application text still needs its own glyph-coverage check or font regeneration.

The application-generated 20 px basic CJK font dominates flash usage. Labels share static font and tone styles. Application memory and image evidence live in [validation.json](../../validation.json). Keep object and animation ownership explicit when replacing screens; the social card retains one screen and its drawer.

## Licenses and verification

The code is Apache-2.0. Font subsets follow `OFL.txt`; Font Awesome icon designs also follow `FONT-AWESOME-LICENSE.txt` (CC BY 4.0). The component and gallery compile without warnings against the SDGOODS template with ESP-IDF 5.5.4. Every gallery screen was rendered on a host LVGL 8.3.11 build with a 32 KB heap and reviewed visually; the component has not yet run on the device. Verify touch targets, scrolling smoothness, text coverage and memory of each new application on the device.
