# Music and media cases

English | [中文](README.zh.md)

Reuse the user-accepted local player, album-cover view and playback controls on the SDGOODS 360-pixel round badge. Install `sdgoods-case-media` with the standalone installer described in the kit entry guide, `target=esp-idf` and the project language. Watch controls, Pixel dependencies and the audio driver are included. Add `sugarat_media` to main `REQUIRES`; installation preserves customized sources and existing application entry points and never accesses a device.

## Scene and language

Choose `MEDIA_SCENE_PLAYER`, `MEDIA_SCENE_COVER` or `MEDIA_SCENE_CONTROLS`. Select `MEDIA_LOCALE_ZH` or `MEDIA_LOCALE_EN` from the explicit user language, otherwise the reference language, then the conversation language. All scenes share playback state and track-name headings. Keep `gallery_navigation=false` for standalone cases; enable arrows only for an explicit test overview. Playback starts paused and never resumes automatically after hiding the screen. Controls include seek, previous/next, continuous volume, mute with last-volume restoration and single-track repeat.

## Integration and audio

Read `include/sugarat_media.h` and `include/media_transport.h`. Call `media_open()` on the LVGL thread with explicit `media_config_t`: backend callbacks, three track durations, track count, unmute volume, locale, scene, gallery choice and silent-preview flag. Load the returned screen, bind `media_set_paused()` to shell pause/resume and delete the screen after loading another screen. Providers own their state and PCM buffers beyond screen deletion. The transport serializes request identifiers, waits for matching termination before replacing audio and ignores old completions. Device errors allow an explicit retry; unconfirmed start/stop blocks playback until restart.

`examples/media_clock.c` supplies an explicitly silent native backend. `examples/media_device.c` supplies a firmware-lifetime worker using `sugarat_audio`, static PCM data and bounded command/terminal queues. Copy the selected example into the application, include its header and compile it with main. For device playback, add `sugarat_audio` to main `REQUIRES`, call `media_device_init()` instead of the foundation's `sdgoods_audio_init()` and use `media_device_backend()`. Never initialize both drivers. Apply the volume-setter guard documented in `components/sugarat_audio/README.md` to the existing BSP. Audio callbacks never hold screen pointers or call LVGL. Keep the example owner alive for the firmware lifetime; it has no dynamic shutdown API.

Generate `media_tracks.h` and `media_tracks.c` into an application-owned directory with Python 3 and compile the generated C with main. The generator supplies Morning Light, Sea Breeze and Night Walk: three original eight-second 16 kHz, 16-bit mono compositions with peaks below 7000. It needs no external media or Python dependencies and rejects all output conflicts before writing. Add the output directory to include paths. For a native build, compile the clock example instead of the device example and set `silent_preview=true`. Native preview never claims working audio. Optional WAV output uses the same compositions.

```sh
python3 components/sugarat_media/tools/generate-audio.py --output main/media-assets
python3 components/sugarat_media/tools/generate-audio.py --format wav --output media-preview
```

## Verification

The user accepted this case on an SDGOODS round badge with ESP-IDF 5.5.4 and LVGL 8.3.11 after app-only Flash write, independent digest verification and audio-owner startup observation. Host tests cover both languages, all three views, 32 KiB LVGL heap, round-screen targets, seek, replacement, mute, external volume changes, repeat, stale completion, timeout and screen disposal. The repository's `examples/music-media` harness builds these canonical sources. Each new integration still needs its own build, physical input and audible-quality verification. No network, account, recording, compressed-audio decoder or file importer is supplied.
