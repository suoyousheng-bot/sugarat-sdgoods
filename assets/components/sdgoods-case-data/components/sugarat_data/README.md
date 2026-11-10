# Data and dashboard cases

English | [中文](README.zh.md)

Reuse a temperature gauge, temperature/humidity/battery overview, bounded-history chart or upper-threshold alert on the SDGOODS 360-pixel round badge. Install `sdgoods-case-data` with the standalone installer described in the kit entry guide, `target=esp-idf` and the project language. Watch controls and their fonts are included. Add `sugarat_data` to main `REQUIRES`. Installation preserves custom files and application entry points and never accesses or flashes a device.

## Scenes and language

Choose `SD_SCENE_GAUGE`, `SD_SCENE_OVERVIEW`, `SD_SCENE_HISTORY` or `SD_SCENE_ALARM`. Select `SD_LOCALE_ZH` or `SD_LOCALE_EN` from the explicit user language, otherwise the reference language, then the conversation language. The language button switches resources without resetting data or acknowledgement. Use `gallery_navigation=false` for a standalone case; enable arrows only for a requested test overview. Only the gauge has a full-screen measurement ring.

History retains the latest 48 sample or gap records in RAM, with evenly spaced sample slots rather than a time axis. Invalid, offline and expired intervals break the line. The upper temperature alarm triggers at the configured threshold and clears at threshold minus positive hysteresis. Acknowledgement keeps the active alarm until a valid recovery reading; a new alarm after recovery requires a new acknowledgement. Restart clears history and acknowledgement.

## Data integration and ownership

Read [sugarat_data.h](include/sugarat_data.h) and [data_model.h](include/data_model.h). Call `sd_open()` with explicit monotonic clock, scene, locale, source label, limits, expiry, positive refresh interval and navigation choice on the LVGL thread. Invalid configuration returns NULL without a screen. Load the returned screen with `lv_scr_load()`. Bind `sd_set_paused()` to shell pause/resume; deletion releases the timer and state. Load another screen before deleting it.

Use `SD_SOURCE_EXTERNAL` for actual telemetry. Providers decode their inputs, marshal complete readings to the LVGL thread and call `sd_publish()` while the screen is alive. Temperature is signed integer tenths of Celsius; humidity and battery are whole percentages from 0 to 100. A rejected reading hides all current values, retains the last accepted sample and alarm and adds a history gap. `sd_disconnect()` reports provider loss. Freshness uses monotonic receive time. Expired, offline and invalid data never become zero readings or clear an active alarm.

The component starts empty and does not acquire sensors, connect to services or synthesize samples. `SD_SOURCE_DEMO` only adds the persistent demo label; callers must supply any demonstration explicitly. Stop provider callbacks before deleting the screen. Hidden screens accept published data but defer rendering; resume re-evaluates freshness. Preserve BSP ownership and existing application entry points.

## Verification

The user approved the actual LVGL static preview and explicitly skipped flashing and physical acceptance. ESP-IDF 5.5.4 firmware compilation passed. The upstream Sugarat regression project builds these same sources and checks both languages, the 32 KiB LVGL heap, round bounds, text overlap, glyphs, navigation, pause/resume, disposal and recorded UI text. Model tests cover input validation, exact expiry, bounded history, gaps, hysteresis, acknowledgement and rearming. Physical display, touch, power and real sensor behavior remain unverified. Every new hardware integration requires its own validation.
