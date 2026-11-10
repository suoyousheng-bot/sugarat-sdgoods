/* SPDX-License-Identifier: Apache-2.0 */
/** Chinese companion resources. */
#include "companion_copy.h"
#include <inttypes.h>
const sc_copy_t sc_copy_zh = {
    .language = "EN",
    .characters = {"小宇", "小桃", "星航员", "小法师", "影忍", "比特猫", "柴柴", "棉花兔", "团团", "啾啾",
                   "小狐", "呱呱", "企企", "花仙子", "飘飘", "软软", "小龙", "菇菇", "方糖仔", "芯芯"},
    .moods = {"待机", "开心", "惊讶", "难过", "睡觉", "喜欢", "说话", "眨眼"},
    .voice = {"准备倾听", "正在聆听", "思考中", "正在回答", "未连接"},
    .pet = "摸摸", .play = "玩耍", .sleep = "睡觉", .wake = "叫醒", .mood = "表情", .animate = "动效", .still = "静止",
    .pet_detail = "互动 %" PRIu32 " 次 · 点名字选角色", .mood_detail = "点名字选角色",
    .demo_detail = "交互演示 · 不录音", .external_detail = "语音服务状态",
    .hold = "按住说话", .listening = "正在聆听", .thinking = "思考中", .speaking = "正在回答", .offline = "未连接",
    .go_offline = "演示离线", .go_online = "演示上线",
};
