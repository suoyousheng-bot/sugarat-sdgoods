---
description: "Sugarat Radio controls, US service selection and customization parameters."
---

# Sugarat Radio play reference

English | [中文](PLAY.zh.md)

## Summary

An Internet radio for a single-touch ESP32-S3 round badge. The first screen is Sugarat Wi-Fi setup. After DHCP, saved credentials and setup hotspot shutdown, the radio opens automatically. Saved Sugarat credentials reconnect after restart; upstream provisioning credentials are not imported. English is the default; language switching remains available. Screen, touch, power and audio wiring retain the reviewed upstream BSP.

## Contents

- [Controls](#controls)
- [US stations and time](#us-stations-and-time)
- [Customization](#customization)
- [Release acceptance](#release-acceptance)

<a id="controls"></a>
## Controls

Swipe left/right on the main screen to cycle 13 persisted skins. Previous/next buttons change stations; the center button pauses/resumes or retries a failed stream. Swipe up or tap the list icon to browse stations. Open Settings from that list to select Wi-Fi, US region, appearance, sleep timer or refresh. Pull down from the top for the BSP control center. Switching skins preserves playback. A region change refreshes the directory while retaining the current stream. The Wi-Fi settings screen returns to the existing radio after a newly committed connection or an explicit return action.

<a id="us-stations-and-time"></a>
## US stations and time

The directory tries a configured HTTPS curated JSON array first, then [Radio Browser](https://docs.radio-browser.info/) queries restricted to `countrycode=US`, then [bundled development stations](main/stations-us.json). A city/state selection first searches the city name within its state, then expands to that state and the nationwide list when results are sparse; it is not a guarantee that every result broadcasts in that city. No IP location request is made. A working hosted curated list supplies its own ordering and takes priority over region queries. Both remote sources accept only MP3/AAC/AAC+ HTTP(S) entries, reject known playlists/HLS and other codecs, deduplicate URLs, and cap the list at 64. Malformed, incomplete or oversized responses fall back. Bundled stations still require Internet access.

The development presets are [KEXP's published 160K AAC-LC stream](https://www.kexp.org/mobile/kexp-livestreams/) and two [Radio Paradise streams](https://radioparadise.com/listen/stream-links). Station names remain their own brands. Known US callsigns receive configured FM labels; an Internet-only station receives a channel number without an invented frequency. KEXP's Seattle identity is [90.3 FM](https://www.kexp.org/about/), KCRW's LA identity is [89.9 FM](https://www.kcrw.com/pages/about-kcrw), and [WNYC FM](https://wnyc.org/about) is distinguished from WNYC AM. A display frequency identifies the broadcaster; this device receives Internet streams rather than FM RF.

The region selector offers nationwide plus 16 city/state markets. Regional POSIX timezones include daylight-saving rules, with fixed offsets for Phoenix and Honolulu. Nationwide defaults to Eastern Time. SNTP uses `time.cloudflare.com`; until time is available the clock shows `--:--`. Location and timezone are manually chosen; the device does not infer them from the user's network.

<a id="customization"></a>
## Customization

| Parameter or file | Default | Effect |
| --- | --- | --- |
| `CONFIG_SUGARAT_RADIO_CURATED_URL` | Empty | HTTPS URL for the JSON array; configure after hosting the reviewed list. |
| `CONFIG_SUGARAT_RADIO_BROWSER_URL` | `https://de1.api.radio-browser.info` | Directory server; configure a working Radio Browser mirror as needed. |
| `CONFIG_SUGARAT_RADIO_DEFAULT_TIMEZONE` | Eastern POSIX timezone | Clock timezone for nationwide mode. |
| `CONFIG_SUGARAT_WIFI_US_CHANNELS` | Enabled | Set US channels 1–11 before scanning or reconnecting. |
| `CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM` | 8 | Development radio power cap; validate reception and power on hardware. |
| `main/stations-us.json` | KEXP + two Radio Paradise streams | Embedded development fallback and example hosted-feed format. |
| `main/radio_region.c` | 17 choices | Market labels, query terms and timezones; picker supports at most 20 choices. |
| `main/apps/ui_radio_skin.c` | 13 skins | Skin names, colors and registry; keep IDs stable for saved preferences. |

Preferences use NVS; no credentials belong in source, manifests or distribution artifacts. The firmware does not erase NVS to recover initialization failures and does not mount/format appdata at startup. The Sugarat network owner replaces the upstream Wi-Fi driver initialization; do not initialize the BSP Wi-Fi manager or a second I2S owner. Keep `NOTICE`, source copyright and modified-file notices when making another derivative.

<a id="release-acceptance"></a>
## Release acceptance

Desktop rendering and compilation are recorded separately in [VALIDATION.md](VALIDATION.md). Device acceptance requires both setup methods, wrong-password recovery, reboot reconnect, setup hotspot closure, 13 skins, station changes, MP3/AAC sound, disconnection recovery, volume/mute, sleep and physical power behavior. Long-running playback and actual US network access remain hardware/network checks. Commercial publication also needs the real hosted-feed URL and a reviewed station list; no streaming rights are granted by the source-code license.
