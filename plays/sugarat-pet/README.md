---
description: "Build Sugarat's animated SDGoods companions with nearby friendship and inherited eggs."
---

# Sugarat Pocket Companion

English | [中文](README.zh.md)

## Summary

Six original animated character families share the existing gentle epet care simulation on the SDGoods 360×360 round badge. First companions are Ember, Leaf or Cloud; paired parents can produce Sprout, Glow and Willow, with inherited bloodlines and visible individual differences. The application builds one `sugarat_pet.bin`. [Validation](validation.json) distinguishes native tests, firmware compilation and physical-device acceptance.

## Contents

- [Controls](#controls)
- [Build](#build)
- [Host checks](#host-checks)
- [Saving and time](#saving-and-time)
- [Nearby pairing](#nearby-pairing)
- [Sources and limits](#sources-and-limits)

<a id="controls"></a>
## Controls

The home screen emphasizes the animated companion, with a name, growth label and a bottom shortcut. Tap that shortcut or drag upward from the lower screen, including the companion's feet, to reveal six labeled icons: Feed, Play, Sleep or Wake, Stats, Family and Meet. Each button is 72×64 pixels with 10-pixel horizontal and vertical gaps; tapping a gap does nothing. The tray follows vertical movement after 10 pixels, then snaps open or closed; swipe down or tap above it to dismiss it. Background rendering pauses during movement and while the tray is open. BSP press ownership and guarded taps prevent sliding over a button from activating it; top-edge system controls remain owned by the BSP. Stats contains Clean. Swipe left or right in Family to browse companions and any egg, with no wrap at either end; the first-companion page uses the same gestures. The upper-left Back control has a 72×72 touch area and exits without changing the active companion. The bottom Select control has a 152×56 touch area and resumes the browsed member or tends the egg. A ready egg hatches when tapped. The firmware UI uses English.

<a id="build"></a>
## Build

Activate ESP-IDF 5.5.4 with the ESP32-S3 toolchain, CMake and Ninja, then run from the repository root:

```sh
idf.py -C plays/sugarat-pet -B /tmp/sugarat-pet-build build
```

The image is `/tmp/sugarat-pet-build/sugarat_pet.bin`, project identity `sugarat_pet`, firmware version 0.2.2. Artwork is compiled into the application. The reference partition table is unchanged. Install only the application through the device's supported route after inspecting its actual layout and available capacity; do not use the build's combined flash command to replace platform partitions. `CONFIG_PET_SOCIAL_CHANNEL` selects a shared ESP-NOW channel, default 1. The LVGL pool is 64 KiB; current scene and decoded-frame buffers belong to the application.

<a id="host-checks"></a>
## Host checks

With a C compiler, CMake, Ninja, OpenSSL 3, Python and Pillow available, run:

```sh
python3 plays/sugarat-pet/tools/pack_characters.py --check
python3 plays/sugarat-pet/tools/check_host.py --output /tmp/sugarat-pet-host
```

The checks execute the actual care, legacy reader, family records, file store, social state machine, cryptography, BSP guarded taps, application gestures and LVGL view with AddressSanitizer and UndefinedBehaviorSanitizer. They cover 18 lower-screen swipe origins, finger tracking, background freezing, independent button gaps, bounded member browsing, Back versus Select and animation disposal. Radio datagrams and device services are native adapters. Two endpoints exercise loss, reordering, duplication, tampering, failures before and after durable writes, and reboot recovery. Output includes a text snapshot, round-screen images, `animated-family.gif` and `controls-swipe.gif`. These checks never open serial or Wi-Fi hardware.

<a id="saving-and-time"></a>
## Saving and time

The first family-capable launch imports the newest validated 56-byte Ember record from private NVS without replacing it. New families use alternating 8 KiB `family-a.dat` and `family-b.dat` files in the SDK-provided `sugarat_pet` appdata directory. When that filesystem is unavailable, compact private NVS keys `family_a` and `family_b` retain the same validated record without its zero padding, at most 3 KiB per key. Existing compact records remain authoritative even if appdata later becomes available. NVS writes require space for a maximum next record and leave 128 available entries for other users, in addition to the library reserve. File writes flush and sync; NVS writes commit; both read back before success. Unknown versions, wholly damaged records or unconfirmed writes disable further writes and breeding; a damaged partner can recover from a valid file. Unavailable storage or insufficient shared NVS capacity permits temporary care with a visible notice. An unformatted appdata partition is never formatted automatically. No shared NVS or partition is erased. Older firmware cannot read family progress; see the [migration guide](../../../docs/upgrade-guide/v0.2.1-rc.2/pet-family-save/guide.md).

Care autosaves after actions and every 30 effective foreground seconds. System pause, launcher return and normal power-off request a save; an unconfirmed save stops software launcher return. Physical power removal can still lose changes since the last confirmed write. Flash operations use the BSP touch-flash exclusion window. Only the selected companion advances; tending an egg pauses parent care. System controls, application absence and power-off freeze progress. Each growth level takes 30 active minutes, with adulthood at Lv31. Egg progress takes 30 active minutes; parental breeding rest takes 120 active minutes. Neglect has a recoverable health floor of 20.

<a id="nearby-pairing"></a>
## Nearby pairing

Meet starts an owned RAM-configured ESP-NOW station only when Wi-Fi is available. Both devices explicitly choose each other. A first meeting uses ephemeral P-256 agreement and a six-digit comparison code confirmed by both users; subsequent meetings authenticate with the saved friend key. Application packets use direction-specific AES-GCM keys, fresh sequence nonces and replay rejection. Radio callbacks only enqueue bounded datagrams; the LVGL owner processes them. Closing Meet, opening system controls or powering down releases the radio.

Greet, Gift and Play each contribute one friendship bit after the peer accepts the activity. Breeding requires both users' consent, adults, energy of at least 30, a free egg and family slot, completed activities and no pending transaction. Known self, parents and siblings are excluded. Each parent contributes one of its two base alleles. Both devices calculate and verify the same offspring digest; they receive different child identities and at least one visible difference. Prepared plans, commit decisions and receipts are durable and idempotent. A disconnected prepared transaction stays pending; reconnect the same active parents to finish it. Parents remain in the family. Limits are six local members, one egg, sixteen friends and sixteen retained transaction receipts; full records refuse additional breeding without deleting companions.

<a id="sources-and-limits"></a>
## Sources and limits

[upstream.json](upstream.json) pins unchanged MIT-licensed epet sources. [Character metadata](assets/characters/manifest.json) pins the six project-generated ImageGen source atlases; `tools/pack_characters.py` compiles 96 anchored 96×96 indexed RLE frames. Each family has four-frame idle, blink, feeding and happy clips. Sleep uses a closed-eye pose with breathing; birth uses egg motion and a happy reveal. Growth currently scales the same artwork. Ten traits are stored and inherited; body width, color, forehead marking and animation tempo are expressed. Independent growth drawings, layered eye/ear/mouth parts, dedicated sad/birth clips, sound effects, member archival and a separate friend album are outside this build.

Pixel-Pets and pet-esp informed the social and inheritance design; their code and character assets are not imported. BSP, LVGL and Watch retain their licenses. Physical ESP-NOW interoperability, radio range, actual touch, battery life, launcher coexistence and power-loss behavior require two-device acceptance of this firmware version. Native rendering and a successful ESP32-S3 build do not establish those results.
