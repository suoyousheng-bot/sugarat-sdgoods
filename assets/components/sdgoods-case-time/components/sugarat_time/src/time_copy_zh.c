/* SPDX-License-Identifier: Apache-2.0 */
/** Chinese time-tool text selected for Chinese requests and references. */
#include "time_copy.h"
#include <inttypes.h>
const time_copy_t time_copy_zh = {
    .titles = {"番茄专注", "倒计时", "秒表", "桌面时钟"},
    .ready = "准备开始", .running = "计时中", .paused = "已暂停", .done = "时间到 · 点击继续",
    .focus = "专注", .rest = "休息",
    .focus_detail = "%s · 已完成 %" PRIu32 " 轮", .countdown_detail = "切换页面也会继续计时",
    .lap_detail = "第 %" PRIu32 " 次 · %02" PRIu64 ":%02" PRIu64 ".%02u", .lap_hint = "开始后可记录当前时间",
    .seconds = "%02u 秒", .clock_unset = "请先设置当前时间", .clock_detail = "手动校时 · 重启后需重设",
    .clock_columns = "小时             分钟", .duration_columns = "分钟               秒",
    .set_time = "设置时间", .set_duration = "设置时长",
    .cancel = "取消", .save = "保存", .reset = "重置", .lap = "记次", .start = "开始", .pause = "暂停",
    .focus_presets = {"25分", "45分", "休息"}, .countdown_presets = {"10秒", "5分", "自定"},
    .rest_complete = "休息结束", .focus_complete = "专注完成", .countdown_complete = "倒计时结束",
    .invalid_duration = "请设置有效时长",
};
