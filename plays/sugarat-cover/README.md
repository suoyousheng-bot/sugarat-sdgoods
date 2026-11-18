---
description: "Manage Sugarat round-badge covers from a phone and send photos to nearby badges."
---

# Sugarat Cover

English | [中文](README.zh.md)

## Summary

Sugarat Cover displays a full-screen photo on the SDGOODS ESP32-S3 360×360 round badge. A phone manages photos through the badge's local Wi-Fi page; nearby badges exchange photo copies through ESP-NOW. Device and phone interfaces default to English and share a saved English/Chinese preference. Compilation and host simulations do not establish physical touch or wireless behavior.

## Table of Contents

- [Controls](#controls)
- [Phone management](#phone-management)
- [Nearby sharing](#nearby-sharing)
- [Storage and failures](#storage-and-failures)
- [Build and verification](#build-and-verification)
- [Sources and limitations](#sources-and-limitations)

<a id="controls"></a>
## Controls

Tap the cover to reveal Phone upload, Share a photo and the language switch. Swipe horizontally to change the selected cover; the previous and next photos are decoded ahead of time, so a swipe follows the finger without a decoding pause. The menu closes after seven seconds. Main buttons measure 228×60 pixels, paired buttons 108×60 with a 12-pixel gap, and the language control 60×60. Functional pages use a left-to-right swipe of at least 60 pixels to return, including swipes begun on a button. Phone upload returns to the action menu; peer discovery, pairing and transfer return to the sharing menu. Exiting a wireless page stops its connection, and controls remain unavailable until shutdown completes. There are no on-screen back buttons. Controls stay inside the 240×240 safe rectangle at (60,60). A release activates a button only inside its original target after movement of at most 24 pixels on either axis; dragging away and returning does not activate it. A changed photo library, rebuilt target or overlay cancels a held press and requires release before another target accepts input. Language updates retain the existing controls so a completed save does not cancel an unrelated press. Cover swipes require at least 40 horizontal pixels and reject predominantly vertical motion.

<a id="phone-management"></a>
## Phone management

Choose Phone upload. The badge first shows its Sugarat hotspot name and asks you to join it in the phone Wi-Fi settings; this step has no QR code and needs no password by default. Use password enables a fresh eight-digit numeric WPA2 password displayed on the badge; Remove password restores the open hotspot. Changing this option restarts the hotspot. Keep the connection when the phone reports no internet. Only after a phone joins does the badge show the management-page QR, which contains an HTTP address rather than Wi-Fi credentials. Use the phone camera to scan it, or type `http://192.168.4.1` in the browser.

Captive DNS, HTTP probe redirects and DHCP option 114 provide an automatic page entry on supported phones; the operating system controls whether a window opens. Android's Wi-Fi sign-in window cannot open the photo picker; when a tap on Upload opens no picker there, the page tells the user to open `http://192.168.4.1` in a browser or scan the page QR code. The root address redirects into the current local management session, so it works without copying a token. Photo APIs and mutations still require the session token. This build provides no cloud service.

Choose a photo, drag its crop, adjust zoom or rotate it, then upload. The browser exports a 360×360 JPEG with a circular preview. Uploading saves a copy without replacing the current cover; select the saved photo and Set as cover to display it. Manage enables deletion of other photos. The current cover cannot be deleted until another cover is selected. Switching the page language saves the shared preference, which survives a page reload.

<a id="nearby-sharing"></a>
## Nearby sharing

On one badge choose Share a photo → Receive photo. On the other choose Send this photo, then select the nearby badge by its suffix. Compare the six-digit code on both screens and confirm on both. Images use ephemeral P-256 key agreement and AES-GCM application encryption over ESP-NOW. A completed receiver save is required before the sender reports delivery; missing final confirmation reports an unknown outcome and asks the user to check the other gallery. Originals remain on both badges, and a received image becomes the cover only after Show is selected.

Phone management and nearby sharing run separately. Both badges must use the same configured 2.4 GHz channel; `CONFIG_COVER_CHANNEL` accepts 1–11 and defaults to 6. Discovery, confirmation, packet acknowledgements, duplicate handling and bounded retries are implemented in [the shared protocol](main/cover/cv_link.c). Host transport simulations exercise that protocol, but do not measure radio throughput or range.

<a id="storage-and-failures"></a>
## Storage and failures

Language changes save in a background worker; the language control shows an ellipsis until completion, while other controls remain available. A save failure displays an actionable error instead of claiming that the language was saved.

The gallery accepts at most 32 JPEGs of 160 KiB each; available filesystem space can impose a lower limit. Images live in the application's `sugarat_cover` appdata directory, while NVS stores the selected image, locale and initial-image marker. Content identifiers deduplicate identical images. A staged write is synchronized and renamed before the gallery advertises it. Saving or language failures are reported without claiming success.

The [foundation](SUGARAT_TEMPLATE.md) retains the platform partition table and never erases NVS. When the appdata partition is entirely erased flash, the first mount creates its filesystem; a damaged or foreign filesystem is never formatted. Storage failures disable gallery writes and display an error; the application has no recovery flow for damaged storage. Flash writers serialize with touch reads, then retain only a 40 ms settling interval after completion. This removes the reserved multi-second post-save input delay; physical touch response still requires device acceptance.

<a id="build-and-verification"></a>
## Build and verification

Use ESP-IDF 5.5.4 with its Python/toolchain environment. [tools/build.py](tools/build.py) requires `--sdk` pointing to that SDK, accepts `--tools` for the toolchain root and `--output` for build artifacts, and performs compilation without opening a serial port. Its default output is the repository's `/tmp/sugarat-cover-build/sugarat_cover.bin`. The local LVGL component requires no registry download.

[tools/check_host.py](tools/check_host.py) requires CMake, Ninja, OpenSSL 3 and Pillow. Pass the matching `--sdk` to build the real HTTP-handler adapter as well as the sanitizer-enabled store, protocol and LVGL tests. It verifies the owner-local [bilingual UI snapshot](tests/expected/ui-text.txt) and renders device screenshots. [tests/web_check.mjs](tests/web_check.mjs) uses the repository Playwright installation and that adapter to check browser crop/upload/display/delete, language persistence, access failures and narrow layouts. These checks use host storage and networking adapters; they do not execute ESP32 peripherals.

An explicit `COVER_UI_DIAGNOSTICS` CMake option adds the device test component for repeatable pointer-driven measurements. Its M/L/U/B commands open the menu, change language, open upload and swipe back; completion waits for the expected interface and a completed repaint. The normal build does not contain these commands.

Build evidence and screenshots live under `/tmp/sugarat-cover-`. No device write is part of these checks. Before a separately authorized deployment, compare the actual target and partition layout, preserve existing data, then verify written bytes, boot, display, touch and two-device transfer independently.

<a id="sources-and-limitations"></a>
## Sources and limitations

[NOTICE](NOTICE) retains SDGOODS, LVGL, Watch font/icon and application attributions. The initial sunset image comes from the repository's UI theme lab. The application uses the reviewed [hardware identity](hardware-product.json); it does not add generic product defaults. Phone upload accepts browser-decodable photos and exports static JPEGs; animation, cloud management, router mode, automatic reciprocal swaps and slideshow playback are outside this build. Real power-loss recovery, physical buttons/touch, QR scanning, radio interoperability, throughput and battery life remain device acceptance work.

## Dev Note

None.
