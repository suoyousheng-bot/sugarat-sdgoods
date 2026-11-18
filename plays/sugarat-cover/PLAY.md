# Sugarat Cover

Your favorite photos on a round badge. Crop and upload from your phone, swipe through your gallery, and send photo copies to a nearby friend without a router or internet.

Community play: Sugarat 玩法社区 #31 (`sugarat_cover`), published version 0.1.4.
Source version in this directory: 0.1.4.

## About the play

# Sugarat Cover

Keep a favorite photo on your Sugarat round badge, manage your gallery from a phone, or send a photo copy to a nearby friend.

## On the badge

Tap the cover to open Phone upload, Share a photo and the language switch. Swipe left or right to change the selected photo; neighboring photos are preloaded, so the cover follows your finger. Swipe right on a functional page to return. The action menu closes after seven seconds. English is the default; English and Chinese share a saved preference across the badge and phone page.

## Upload from a phone

1. Tap Phone upload and join the Sugarat hotspot shown on the badge in your phone Wi-Fi settings. It is open by default; Use password enables a displayed eight-digit password.
2. Keep the Wi-Fi connection if the phone reports no internet. After the phone joins, scan the management-page QR code or open http://192.168.4.1. Some phones open the local page automatically. On Android that automatic Wi-Fi sign-in window cannot open the photo picker; the page then asks you to open http://192.168.4.1 in your browser or scan the badge QR code.
3. Choose a photo, drag the crop, adjust zoom or rotate, and upload. The browser exports a 360 x 360 JPEG with a circular preview.
4. Choose a saved photo and tap Set as cover. Uploading alone does not replace the current cover. Manage deletes other photos; select another cover before deleting the current one.

The gallery holds up to 32 JPEGs of up to 160 KiB each, subject to available application storage. This application provides no cloud gallery and does not require a router or internet for its local photo features.

## Share with a nearby badge

Install the same version on two compatible badges. On the receiving badge, choose Share a photo > Receive photo. On the sender, choose Send this photo and select the nearby badge. Compare the six-digit pairing code on both screens and confirm on both. Keep both devices nearby until delivery completes. Select Show on the receiver to use the new photo as its cover.

Sharing sends a copy; it does not delete or automatically exchange the original. ESP-NOW carries the photo with P-256 key agreement and AES-GCM application encryption. Both devices must use the same configured radio channel. Phone management and sharing operate separately. If final delivery confirmation is lost, check the receiving gallery before retrying.

## Installation and validation

Version 0.1.4 is an ESP32-S3 application image, not a full-flash backup. Use the Sugarat installation flow for the SDGOODS round badge and preserve its platform partitions and application data. A fully erased application-data partition receives a new filesystem on first start; a damaged or foreign one is reported and never formatted.

The release image passed compilation and application-image inspection. Host sanitizer tests cover storage, gestures, paired transfer and failure handling; browser tests exercise the real C HTTP handlers for crop, upload, selection, deletion and language persistence. The listing cover is a host-rendered application screenshot.

On a badge, the storage fix was written and verified, created the filesystem on an erased data partition and saved the starter photo, and phone upload through the scanned management page worked. With v0.1.4 on a badge, cover swipes slide without a pause. The Android sign-in-window notice is verified by browser tests only. Two-device radio transfer, power-loss recovery and multi-app launcher installation/Exit still need acceptance. Radio range, throughput and battery life have not been measured.

## Credits

Application code is Apache-2.0. The firmware retains the SDGOODS platform, LVGL, Watch font/icon and other third-party notices. The initial sunset artwork comes from the repository UI theme lab. Static photos are supported; animation, slideshow playback and cloud management are outside this release.

## Requirements

Sugarat SDGOODS ESP32-S3 round badge with a 360 x 360 touch display, 32 MiB Flash, 8 MiB PSRAM and working application-data storage. Use a phone with Wi-Fi and a modern browser for uploads. Nearby sharing requires two badges with the same firmware and radio channel (default channel 6). Install the application image through a compatible Sugarat installation flow; preserve platform partitions and existing data.

## Make your own (做同款)

1. Download the attachment `sugarat-cover.tar.gz` of release `v0.3.0`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.0/sugarat-cover.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-cover --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_cover.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
