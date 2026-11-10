/* SPDX-License-Identifier: Apache-2.0 */
/** Chinese music resources. */
#include "media_copy.h"
const media_copy_t media_copy_zh = {
    .tracks = {"晨光", "海风", "夜行"}, .views = {"本地播放器", "专辑封面", "播放控制"},
    .states = {"已暂停", "正在开始", "播放中", "正在停止", "播放完成", "播放失败"},
    .language = "EN", .original = "Sugarat 原创音轨", .preview = "静音预览", .volume = "音量 %u%%",
    .mute = "静音", .unmute = "取消静音", .repeat = "单曲循环", .once = "单次播放",
    .device_error = "播放失败，点播放重试", .timeout_error = "音频未确认停止，请重启",
};
