# Lunch Shuffle

Lunch Shuffle is an offline meal picker for the SDGOODS ESP32-S3 round badge. Shake the badge to draw one of ten meals. After the reveal, shake again or tap the screen to draw another meal. Each new draw skips the previous result. If motion detection is unavailable, the screen shows a tap hint on startup.

A shake requires four acceleration peaks within 700 ms. Each peak must deviate by more than 0.55 g from the recent acceleration baseline. Counted peaks must be 80–250 ms apart so brief contact bounces and movements separated by a pause do not build up to a draw. A detected pickup suppresses draws from motion during the next 250 ms. The 1.2-second sensor cooldown limits repeat draws from the same gesture.

The 8-bit ticks slow down with the animation, then a rising chime plays as the meal appears. Sound follows the control center volume.

## Build

Run `idf.py build` from this directory with ESP-IDF 5.5.4. The application image is `build/sugarat_lunch.bin` and embeds the food graphics and sound. After editing the food PNGs, regenerate the RGB565 buffers with `python3 tools/pack_food.py` and Pillow. To change the draw timing or sound, run `python3 tools/compose_draw.py` to regenerate the WAV and accompanying timing table together.

## Artwork

The food graphics come from [FiveM Inventory Icons — Pixel Art](https://github.com/SwisserDev/fivem-icons-pixel) and are distributed under CC0 1.0.
