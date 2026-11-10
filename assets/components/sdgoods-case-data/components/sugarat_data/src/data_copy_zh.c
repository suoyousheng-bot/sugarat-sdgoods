/* SPDX-License-Identifier: Apache-2.0 */
/** Chinese text owned by the data application. */
#include "data_copy.h"
const sd_copy_t sd_copy_zh = {
    .scenes = {"温度仪表", "设备看板", "历史曲线", "阈值告警"},
    .statuses = {"等待数据", "数据正常", "数据已过期", "数据源离线", "数据无效"},
    .language = "中文", .demo = "演示数据", .external = "外部数据",
    .temperature = "温度", .humidity = "湿度", .battery = "电量",
    .normal = "温度正常", .alarm = "温度过高", .acknowledged = "已确认 · 待恢复", .acknowledge = "确认告警",
    .range = "量程", .history = "最近采样 · 缺失处断线", .empty = "暂无历史数据",
    .threshold = "告警", .recovery = "恢复", .monitoring = "监测中", .waiting = "等待有效读数",
};
