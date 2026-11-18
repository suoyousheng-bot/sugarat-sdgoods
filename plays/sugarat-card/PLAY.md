# Sugarat Social Card

A pocket social card for your round Sugarat badge. Show your name, role, company, contact and personal link; edit from a phone and exchange cards with another badge. English by default, with Chinese available.

Community play: Sugarat 玩法社区 #33 (`sugarat_card`), published version 0.1.2.
Source version in this directory: 0.1.2.

## About the play

## Your card, on your badge

Display your name, role and company, one contact and a personal link on the round screen. Long fields scroll automatically. Swipe up to open QR code, Exchange, Edit, Contacts and language controls. Swipe right from the left edge to return.

## Edit from your phone

Open Edit, join the displayed Sugarat Wi-Fi hotspot in your phone settings, and stay connected if the phone reports no internet. After connecting, scan the page QR or open http://192.168.4.1. Edit the five fields and save. The page reports success only after the badge confirms saving. No account or mobile app is required. A temporary hotspot password is optional.

## Share and keep contacts

Show a vCard QR for a phone to scan. For badge-to-badge exchange, open Exchange on both badges, select the other badge, compare the six-digit code and confirm on both screens. The application uses encrypted ESP-NOW transfer and acknowledges a card after local saving. Contacts holds up to twelve identities; exchanging again with the same identity updates its entry.

## English edition

Fresh installations start in English. The phone editor also defaults to English. You can choose Chinese from the drawer, and saved device language preferences remain in effect after an update. Changing language preserves the card fields. The cover shows an example profile rendered by the actual application UI.

## Validation and current limits

Version 0.1.2 passed native storage, protocol, HTTP, language and LVGL checks, browser editor checks, and ESP32-S3 compilation. These checks do not run the ESP32 Wi-Fi driver. The preceding 0.1.1 build passed standalone flashing and independent byte verification; the owner confirmed a normal physical screen after reconnecting USB. Version 0.1.2 has not been flashed or physically tested. Physical touch, phone hotspot association, QR scanning, two-badge radio exchange and managed launcher installation remain unverified. Serial diagnostics and a diagnostic software restart could not be verified in the preceding device session.

Card fields are public information used for display and sharing. Local card storage is plaintext. Only enter details you want to share.

## Requirements

SDGOODS ESP32-S3 round badge with a 360x360 touch screen, 32 MB Flash and 8 MB PSRAM. Installation needs sufficient contiguous application space. Phone editing needs a Wi-Fi-capable phone and browser; badge exchange needs a second badge running Sugarat Social Card. No cloud account is required.

## Make your own (做同款)

1. Download the attachment `sugarat-card.tar.gz` of release `v0.3.0`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.0/sugarat-card.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-card --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_card.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
