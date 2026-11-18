---
description: "Display and exchange a five-field social card on the SDGOODS round badge, edit it from a phone, and build the application without flashing."
---

# Sugarat Social Card

English | [中文](README.zh.md)

## Summary

Display company, nickname, role, one contact and a personal URL on the 360×360 SDGOODS ESP32-S3 badge. Swipe up for vCard QR, device exchange, phone editing, received cards and language selection. [Validation](validation.json) separates native checks, firmware compilation and physical-device acceptance.

## Contents

- [Controls](#controls)
- [Phone editing](#phone-editing)
- [Device exchange](#device-exchange)
- [Saving](#saving)
- [Build and checks](#build-and-checks)
- [Limits and sources](#limits-and-sources)

<a id="controls"></a>
## Controls

The home screen shows the name at 28 px, role and company on one 16 px line, then centered contact and URL lines. Long fields scroll horizontally. Start in the lower screen and swipe up to reveal the drawer; it follows the finger and snaps into place. Its four 96×64 px icon buttons have 24 px horizontal and 8 px vertical gaps; a 104×40 px language control sits below them. All five targets fit the round screen. Swipe down or tap outside to dismiss. Sliding across an icon does not activate it. Subpages return with a rightward swipe beginning at the left edge; there is no Back button. Received cards support bounded horizontal swipes from the interior. The BSP retains top-down system controls, pause/resume, launcher exit and power-latch handling.

The device starts in English when no preference exists. Existing language preferences remain in effect. The drawer language control switches Chinese and English after committing and reading back the independent NVS setting; it does not change the card record format. An unconfirmed setting reports a visible error and keeps the displayed language.

<a id="phone-editing"></a>
## Phone editing

Open Edit from the drawer. As in [Sugarat Cover](../sugarat-cover/README.md#phone-management), first join the displayed Sugarat hotspot in the phone Wi-Fi settings. This step has no QR and needs no password by default. Use password restarts the hotspot with a fresh eight-digit WPA2 password; Remove password restores the open hotspot. Keep the connection when the phone reports no internet. After a phone joins, the badge automatically displays the editor-page QR; disconnecting returns to the connection step.

Captive DNS, HTTP probe redirects and DHCP option 114 offer automatic page entry on supported phones; the operating system controls whether a window opens. Otherwise scan the page QR with the phone camera or enter `http://192.168.4.1` in a browser. The root address redirects into the current editing session; profile APIs still require its token. Fill the fields and save; success appears only after the device commits and reads back the record. No account, cloud or mobile app is required. Left-edge return, platform controls, power-off and the default five-minute timeout close the hotspot. Reopening creates a new authorization. Editing and ESP-NOW run in separate sessions.

Text limits are UTF-8 bytes: nickname 48, company 64, role 64, contact 96 and URL 160. Nickname is required. Contact type is Email, Phone or WeChat; URLs use HTTP or HTTPS. The device validates text and rejects unsupported glyphs. Long fields scroll on the badge and use an ellipsis in the phone preview; QR and exchange retain the complete fields. The phone preview shows content rather than a calibrated device simulation. Every field is public card data. The editor opens in the device language and offers an independent language switch; its URL retains that choice across refreshes. Switching language preserves form contents and translates validation, authorization and save failures.

<a id="device-exchange"></a>
## Device exchange

Both badges open Exchange, discover each other and explicitly select the peer. They display a shared six-digit code; each user compares it on both screens and confirms. Ephemeral P-256 agreement derives separate AES-GCM direction keys. Profiles transfer only after both authenticated confirmations. Bounded fragments retry within a finite window, reject replay and deduplicate receipt. A card is acknowledged only after verified local saving. Success requires both local receipt and the peer's acknowledgement; a timeout after local saving reports that the remote outcome is unconfirmed. Discovery broadcasts nickname and ephemeral pairing data; profile contact fields remain inside encrypted transfer. This protocol interoperates with this application.

<a id="saving"></a>
## Saving

The app uses its own `sugarat_card` identity and data directory. Existing private NVS records stay authoritative. Otherwise, mounted appdata holds two sequence-numbered, SHA-256-checked files; missing appdata partitions permit private NVS fallback. Damaged or unavailable filesystems, invalid records, unknown versions and unconfirmed writes stop mutations and preserve records for recovery. No filesystem or shared NVS partition is erased or formatted. NVS writes retain a free-entry reserve and may refuse when shared storage is insufficient. Stored fields are plaintext. There are twelve received-card slots; another transfer from a known identity updates that entry, while a new identity is refused at capacity. No contact is automatically evicted.

<a id="build-and-checks"></a>
## Build and checks

Activate ESP-IDF 5.5.4 for ESP32-S3, then run from the repository root:

```sh
idf.py -C plays/sugarat-card -B /tmp/sugarat-card-build build
```

The single application image is `/tmp/sugarat-card-build/sugarat_card.bin`, version 0.1.2. Platform partitions remain the initialized template. Device layout and available launcher capacity require inspection before a later installation; building does not authorize or perform a write.

With `IDF_PATH` set to the SDK checkout, CMake, Ninja, OpenSSL 3, Python and Pillow installed, run:

```sh
python3 plays/sugarat-card/tools/check_host.py --output /tmp/sugarat-card-checks
```

Each run owns a new output directory containing the native build, sanitizer results, bilingual text snapshot, actual LVGL images and `drawer-swipe.gif`. The browser editor check additionally needs the repository's Playwright installation and Chromium; pass that run's `build` directory to `tools/check_editor.py --build <build-directory> --output <review-directory>`. It uses a private loopback server and the actual C parser/file store to check both languages, refreshes, narrow layouts and unconfirmed saves. Native checks also exercise the real language persistence implementation with an NVS adapter and injected write failures. Native HTTP checks execute the firmware handlers for root/probe redirects, captive JSON, API authorization and queued/save-failure responses. Neither check emulates the ESP-IDF Wi-Fi driver.

`tools/prepare_assets.py --check` checks the embedded editor. To regenerate it, run the script without that flag; `--font-dir <lvgl>/scripts/built_in_font` additionally regenerates fonts from the retained Source Han Sans inputs. Font sources are supplied, so a normal firmware build needs no font generator.

<a id="limits-and-sources"></a>
## Limits and sources

The application uses the Sugarat SDGOODS kit 0.2.1, its BSP, LVGL 8.3.11 and Watch controls. App-owned generated fonts include the basic CJK block at 16 and 28 px using two-bit glyph bitmaps; emoji are unsupported. The larger fonts increase the application image size, so launcher installation must check actual available capacity. BSP, application, LVGL, fonts and icon designs retain their included licenses and notices. Fri3d Friends informs the interaction direction; no code from that project or GPL badge projects is imported. [Hardware reference](../../../hardware-kits/sugarat-sdgoods/references/hardware.md) identifies the documented board.

The firmware does not implement NFC, avatar uploads, interest matching, vCard file import, contact deletion or cloud synchronization. Battery use, physical frame rate, phone-to-hotspot behavior, QR scanning, two-device radio range and launcher coexistence require hardware acceptance. [Source entry](main/apps/ui_helloapp.c) owns runtime integration; [the UI](main/card/card_view.c), [exchange](main/card/card_exchange.c), [network](main/card/card_network.c) and [store](main/card/card_store.c) own their respective behavior. No serial access or device write is part of the supplied checks.
