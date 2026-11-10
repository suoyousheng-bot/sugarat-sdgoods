# Character and companion cases

English | [中文](README.zh.md)

Reuse the user-accepted interactive pet, eight-mood badge or voice-state scene on the SDGOODS 360-pixel round badge. Install `sdgoods-case-companion` with the standalone installer described in the kit entry guide, `target=esp-idf` and the project language. Watch controls and 20 Pixel characters are included as dependencies. Add `sugarat_companion` to main `REQUIRES`; installation preserves existing application entry points and customized files and never accesses a device.

## Scene and language

Choose `SC_SCENE_PET`, `SC_SCENE_MOODS` or `SC_SCENE_VOICE`. Select `SC_LOCALE_ZH` or `SC_LOCALE_EN` from the explicit user language, otherwise the reference language, then the conversation language. Both resource files include every label, state and character name. Keep the accepted presentation: no circle around the character, current IP name as the heading, a separate language button and `gallery_navigation=false` for standalone cases. Enable gallery navigation only for an explicitly requested test overview.

Pet supports touch/play reactions, sleep/wake and a local interaction count. Mood badge cycles eight moods and animation. Tapping the IP name selects the next character. Voice demo shows listening while held, then thinking, speaking and idle; its label states that it records nothing. Demo offline cancels the sequence and remains offline until explicitly changed. All state is RAM-only and resets on restart.

## Integration and ownership

Read `include/sugarat_companion.h`, then call `sc_open()` on the LVGL thread with explicit `sc_config_t`: monotonic `now_ms`, scene, locale, character, positive reaction/thinking/speaking timings, voice mode and gallery choice. Load the returned screen, bind `sc_set_paused()` to the product shell's pause/resume callbacks and delete the screen after loading another screen. The screen owns and releases its timer and character animations. `companion_model.h` exposes local interaction timing without LVGL.

Choose `SC_VOICE_DEMO` for a visibly labeled local demonstration. For actual voice, choose `SC_VOICE_EXTERNAL` and supply `on_voice` plus caller-owned `user_data`. External mode starts offline and never runs simulated transitions. The synchronous LVGL-thread callback receives true for an accepted press and false exactly once on release or cancellation, including hidden display and screen deletion. Provider state changes while held do not lose the stop request. Do not delete the screen from inside a callback. Post actual service states through `sc_set_voice_state()` on the LVGL thread while the screen is alive. The provider owns audio, networking, authentication, retries and shutdown; await that shutdown before releasing its resources. No provider is started by this component.

## Verification

The unframed IP-name layout was accepted by the user on an SDGOODS round badge with ESP-IDF 5.5.4 and LVGL 8.3.11 after Flash write, independent digest verification and startup checks. Native tests cover both languages, all 20 characters and eight moods, 32 KiB LVGL heap, round-screen targets, held input, interruption and complete disposal. The repository's `examples/companion` harness builds this component directly. Every new integration still needs its own build and physical verification; the voice demo does not establish working audio or cloud service behavior.
