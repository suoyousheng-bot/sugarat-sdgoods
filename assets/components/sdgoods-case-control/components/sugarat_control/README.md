# Device-control cases

English | [中文](README.zh.md)

Reuse the preview-approved switches, level sliders, scene selector or five-button remote on the SDGOODS 360-pixel round badge. Install `sdgoods-case-control` with the standalone installer described in the kit entry guide, `target=esp-idf` and the project language; Sugarat Watch and pixel characters are included as dependencies. Add `sugarat_control` to the application's CMake `REQUIRES`. Installation preserves application entry points and customized files; it neither builds nor flashes firmware.

## Choose the case and language

Select the explicit user language first, otherwise the reference language, then the conversation language. Use `CONTROL_LOCALE_ZH` or `CONTROL_LOCALE_EN`; the paired `control_copy_zh.c` and `control_copy_en.c` resources cover every visible string. Tapping the title changes language without changing simulated values.

| Case | Behavior |
|---|---|
| `CONTROL_SCENE_SWITCHES` | Independent lamp/fan switches, initially off. |
| `CONTROL_SCENE_LEVELS` | Brightness and volume sliders from 0–100%, initially 60%/30%. |
| `CONTROL_SCENE_SCENES` | Reading: both on, 80%/35%; Night: lamp on/fan off, 15%/0%; Away: both off, 0%/0%. Manual switch or level changes clear the selected scene. |
| `CONTROL_SCENE_REMOTE` | Up/Left/OK/Right/Down, last action and a counter capped at 9999. Held touches count once on release; dragging outside a button cancels its activation. |

Every scene retains a visible local-simulation label. Sliders do not control the badge's display or speaker, and remote buttons send no IR, BLE, Wi-Fi or GPIO commands. All values live in RAM and reset when the screen is recreated. Switching a simulated device off preserves its level; changing its level does not switch it on. External control requires separately defined command transport, failures and device acknowledgements; a changed simulated value is never device confirmation.

## Integrate

Call `control_app_open()` on the LVGL thread after the product BSP starts. Supply locale, initial scene and `gallery_navigation=false` for a standalone application. Enable navigation only for an explicitly requested four-page test overview. Invalid locale/scene or state allocation failure returns `NULL`.

```c
#include "control_app.h"

lv_obj_t *screen = control_app_open((control_app_config_t){
    .locale = CONTROL_LOCALE_EN,
    .scene = CONTROL_SCENE_SWITCHES,
    .gallery_navigation = false,
});
if (screen) lv_scr_load(screen);
```

The screen owns its state and refresh timer; keep its LVGL user data reserved. Bind `control_app_set_paused(screen, true/false)` to the existing shell's pause/resume callbacks. Pausing suspends refresh and ignores input; resuming preserves values. Load another screen before `lv_obj_del(screen)`, which releases both state and timer, including pending refreshes. Preserve existing BSP and application entry points. Font glyphs are bundled in Sugarat Watch; regenerate its fonts with this component's `src` directory when extending copy.

## Verification

The user approved the rendered preview and explicitly skipped flashing and physical testing. ESP-IDF 5.5.4 firmware compilation and LVGL 8.3.11 native pointer checks passed. The repository's `examples/device-control` project builds the same component sources. Checks cover both languages, standalone and gallery modes, 32 KiB heap, round-screen targets, limits, scene values, manual overrides, navigation, held/cancelled touches, pause/resume, independent instances and complete disposal. No physical display, touch, power or external-device actuation is verified; every hardware integration needs independent validation.
