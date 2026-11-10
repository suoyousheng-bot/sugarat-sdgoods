# Time and productivity cases

English | [中文](README.zh.md)

Reuse a complete focus/rest timer, countdown, stopwatch or manual clock on the SDGOODS 360-pixel round badge. Install `sdgoods-case-time` with the standalone installer described in the kit entry guide, `target=esp-idf` and the project language; `sdgoods-ui-watch` and pixel characters are included as dependencies. Add `sugarat_time` to the application's CMake `REQUIRES`. Installation preserves existing application entry points and customized files; it does not build or flash firmware.

## Choose the case and language

Use the language explicitly requested by the user; otherwise follow the reference language, then the conversation language. Select `TIME_LOCALE_ZH` for Chinese or `TIME_LOCALE_EN` for English. The paired resources in `src/time_copy_zh.c` and `src/time_copy_en.c` cover controls, picker labels, states and completion messages. Tapping the title changes language without resetting timers; it cancels unsaved picker edits and dismisses transient notices.

| Case | Behavior |
|---|---|
| `TIME_TOOL_FOCUS` | 25/45-minute focus and 5-minute rest; start/pause/resume/reset; completed focus count and explicit focus/rest transitions. |
| `TIME_TOOL_COUNTDOWN` | 10-second/5-minute presets and a 00:01–99:59 duration picker; rejects zero; completion stops at zero until restarted. |
| `TIME_TOOL_STOPWATCH` | Start/pause/resume, latest elapsed lap timestamp and lap count; reset while paused. |
| `TIME_TOOL_CLOCK` | Manual hours/minutes with midnight rollover; no calendar, network synchronization or persistence. |

## Integrate

Call `time_app_open()` on the LVGL thread after the product BSP starts. Supply monotonic milliseconds that include hidden-display time, the language, the selected tool and `gallery_navigation=false`. This produces an independent case with no left/right gallery arrows or page dots. Enable `gallery_navigation=true` only for an explicitly requested four-tool test overview.

```c
#include "time_app.h"
#include "esp_timer.h"

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }

lv_obj_t *screen = time_app_open((time_app_config_t){
    .now_ms = now_ms,
    .locale = TIME_LOCALE_EN,
    .tool = TIME_TOOL_FOCUS,
    .gallery_navigation = false,
});
lv_scr_load(screen);
```

Keep the screen alive while using its tools; `lv_obj_del(screen)` releases its state and update timer. Bind it to the existing product app shell and delete it only after loading another screen. Focus, countdown and stopwatch retain state across gallery page changes. All state is RAM-only and resets on restart. Completion is visual, with no audio or wake alarm. Hidden displays do not pause elapsed-time accounting. `time_model.h` provides the same accounting without LVGL for applications composing another layout.

## Verification

The source comes from the user-accepted time application on an SDGOODS round badge with ESP-IDF 5.5.4 and LVGL 8.3.11. The test application passed device writing, independent digest verification and startup observation before user acceptance. Native checks cover both languages, the device's 32 KiB LVGL heap, round-screen button containment, paused/background timers, custom-duration rejection, focus/rest transitions, midnight and complete screen disposal. The repository's `examples/time-efficiency` harness builds this installed component directly. Every new integration still requires its own build and physical verification; host rendering does not establish touch, power or alarm behavior on a device.
