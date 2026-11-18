# Sugarat SDGOODS firmware foundation

This project adds the application described in [README.md](README.md). Its local changes include bilingual Watch font subsets, Wi-Fi/ESP-NOW services and a serialized touch/flash gate. The foundation inventory below identifies the reviewed seed.

This source template targets the documented ESP32-S3 round badge: 32MB Flash, 8MB Octal PSRAM, ST77916 360x360 display and CST816 touch. It derives from the supplied HelloApp baseline and retains upstream licensing. Sources: https://github.com/SDGOODS/SDGOODS-ESP32S3 and https://github.com/lvgl/lvgl/tree/v8.3.11.

Use ESP-IDF 5.5.4 and the Sugarat-managed Python/toolchain environment. LVGL 8.3.11 core sources are pinned locally under components/lvgl; no component-registry download is needed. LVGL examples, demos, tests and documentation are excluded. The minimal application has ASCII fonts and no Wi-Fi/BLE integration. Add and validate requested networking or Chinese font support explicitly.

Develop the requested application in main/apps/ui_helloapp.c. Keep apps_registry.c and main/CMakeLists.txt in sync when adding sources. Board pins and drivers belong to components/sdgoods_board; use its public headers. Keep UI in the circular screen's 240x240 safe rectangle at (60,60). Preserve power-key sleep/wake and GPIO7 power-latch behavior.

Sugarat modifications: project naming is supplied at installation; ESP-IDF is pinned to 5.5.4; LVGL is a local 8.3.11 component with the retained GIF/PSRAM patches; the greeting wraps within the safe width; startup does not run the persistence example; failed FAT mounts and NVS initialization do not erase user data, and only a fully erased data partition receives a new filesystem; the panel color-done callback requests the context switch that esp_lcd drops, so synchronous panel writes no longer wait about 1.5 ms for the next interrupt; internal-RAM panel writes skip the heap statistics walk; sdgoods_lvgl_fast_loop() lets a continuously animating application keep the 2 ms loop wakeup without touch. Files bearing the Sugarat modification notice carry these changes. No vendor cloud, marketplace scripts, credentials, personal paths, SDK installation, build outputs or historical device observations are distributed.

The partition table is the reference single-application layout, not a detected device layout or a managed update system. Compare the actual device layout and obtain backup/reset/overwrite acceptance before device writes. Never call launcher host cleanup from application firmware. A damaged or foreign data partition requires a separately designed explicit recovery flow.

A successful build establishes compilation only. Inspect each connected device and separately verify written bytes, boot, display, touch, power and other requested behavior. The device preview needs a project-owned native adapter; this firmware template does not emulate peripherals.
