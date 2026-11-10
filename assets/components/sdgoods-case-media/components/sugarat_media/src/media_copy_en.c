/* SPDX-License-Identifier: Apache-2.0 */
/** English music resources. */
#include "media_copy.h"
const media_copy_t media_copy_en = {
    .tracks = {"Morning Light", "Sea Breeze", "Night Walk"}, .views = {"Local player", "Album cover", "Controls"},
    .states = {"Paused", "Starting", "Playing", "Stopping", "Finished", "Playback failed"},
    .language = "中文", .original = "Sugarat original", .preview = "Silent preview", .volume = "Volume %u%%",
    .mute = "Mute", .unmute = "Unmute", .repeat = "Repeat track", .once = "Play once",
    .device_error = "Playback failed; tap play", .timeout_error = "Audio stop unconfirmed; restart",
};
