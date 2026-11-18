# Magic Bottle

A motion-controlled magic bottle for solo play or two-device pouring, with adjustable liquid quantities and mixed colors. No router or internet needed.

Community play: Sugarat 玩法社区 #26 (`sugarat_fluid`), published version 0.2.1.
Source version in this directory: 0.2.1.

## About the play

# Magic Bottle

Turn the Sugarat round screen into a motion-controlled magic bottle. Play alone or share liquid with a friend's SDGoods device without a router or internet.

- Tilt and shake to move the liquid; tap for four color themes and hold to reset in Solo.
- Swipe right for a full-screen menu with large buttons; set Fill, or leave a bottle empty to catch a friend's liquid. The arc on the right rim shows how full the bottle is.
- Host a room, join from the other device and accept the invitation. The top edge is the bottle mouth: keep the receiver upright, tip the sender mouth downward, then exchange postures to pour back. Arrows at the mouth show who is catching.
- The stream drains from one bottle and splashes into the other in the sender's color, then blends gradually; shaking mixes faster. Full bottles and empty sources stop transferring.
- Only the theme is saved. New solo bottle starts a fresh practice bottle; exiting or restarting ends session liquid.
- English interface; silent.

See [play instructions and limits](README.md#play). The simulation derives from MIT-licensed FluidBox; Sugarat maintains the circular container, rendering, liquid accounting and device integration.

## Requirements

SDGoods ESP32-S3 round badge with QMI8658 and 8 MB PSRAM. Two badges with this firmware and the same ESP-NOW channel are required for sharing.

## Make your own (做同款)

1. Download the attachment `sugarat-fluid.tar.gz` of release `v0.3.0`: https://github.com/suoyousheng-bot/sugarat-sdgoods/releases/download/v0.3.0/sugarat-fluid.tar.gz. It is this folder plus every shared component it uses, including LVGL; nothing else needs to be downloaded. From a repository checkout, `python3 scripts/kit.py export-play --id sugarat-fluid --directory <new-folder>` writes the same project.
2. Make the requested changes in that standalone project. Keep the components, partition table and license files; do not start from the basic template instead.
3. Raise the version in `version.txt` (and `PROJECT_VER` when CMakeLists.txt sets it).
4. Build with ESP-IDF 5.5.4 for `esp32s3`. The application image is `build/sugarat_fluid.bin`; it is the only file to install.
5. Install the application image through the Sugarat desktop app or a launcher slot. Never change the device partition table or reuse a historical flash address.

Report compilation, desktop preview, written-byte verification, startup and physical behavior separately.
