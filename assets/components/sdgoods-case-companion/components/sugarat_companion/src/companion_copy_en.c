/* SPDX-License-Identifier: Apache-2.0 */
/** English companion resources. */
#include "companion_copy.h"
#include <inttypes.h>
const sc_copy_t sc_copy_en = {
    .language = "中文",
    .characters = {"Yuri", "Peach", "Astro", "Wizard", "Ninja", "Bitcat", "Shiba", "Bunny", "Panda", "Chick",
                   "Fox", "Frog", "Penguin", "Fairy", "Ghost", "Slime", "Dragon", "Mushroom", "Cubey", "Chippy"},
    .moods = {"Idle", "Happy", "Surprised", "Sad", "Sleepy", "Love", "Talking", "Wink"},
    .voice = {"Ready to listen", "Listening", "Thinking", "Speaking", "Offline"},
    .pet = "Pet", .play = "Play", .sleep = "Sleep", .wake = "Wake", .mood = "Mood", .animate = "Animate", .still = "Still",
    .pet_detail = "%" PRIu32 " touches · tap name to choose", .mood_detail = "Tap name to choose a character",
    .demo_detail = "UI demo · no recording", .external_detail = "Voice service status",
    .hold = "Hold to talk", .listening = "Listening", .thinking = "Thinking", .speaking = "Speaking", .offline = "Offline",
    .go_offline = "Demo offline", .go_online = "Demo online",
};
