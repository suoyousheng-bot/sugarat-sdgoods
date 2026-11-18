/* Modified by Sugarat: see lz_3d.md for source changes. */
/*
 * 谷仓共创计划 · 谷仓 SDGOODS 开放平台基础工程
 * 平台层（板级支持包 BSP）
 * https://github.com/SDGOODS/SDGOODS-ESP32S3
 *
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * 「谷仓共创计划」与「谷仓 SDGOODS 开放平台」项目、谷仓次元屏（谷仓电子徽章）设备，
 *   以及本基础代码的著作权与相关权利，均归深圳希德创新网络有限公司所有。
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件属于平台层，以 Apache-2.0 发布：可自由商用、可闭源分发，
 * 只需保留本声明并携带 NOTICE 文件。详见 LICENSING.md。
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define AUDIO_RECPLAY_SEC  5u

esp_err_t sdgoods_audio_init(void);

esp_err_t sdgoods_audio_record(void);

esp_err_t sdgoods_audio_play(void);

void sdgoods_audio_abort(void);

/* ---------------------------------------------------------------------------
 * 游戏音频：背景音乐（BGM）循环 + 拍翅音效（SFX）
 * 复用本模块已初始化的 I2S 发送通道（s_tx），避免重复占用 I2S0。
 * ------------------------------------------------------------------------- */
typedef enum {
    AUDIO_BGM_THEME_FLAPPY = 0,   /* 欢快 C 大调 */
    AUDIO_BGM_THEME_PLANE,        /* 激昂上行 */
    AUDIO_BGM_THEME_TETRIS,       /* 经典俄方块旋律 */
    AUDIO_BGM_THEME_POKEMON,      /* 宝可梦风格原创芯片循环（明亮上行 I–V–vi–IV 进行） */
    AUDIO_BGM_THEME_DEFAULT = AUDIO_BGM_THEME_FLAPPY,
} audio_bgm_theme_t;

esp_err_t sdgoods_audio_bgm_start(audio_bgm_theme_t theme);   /* 启动指定主题 BGM（同时打开功放） */
void      sdgoods_audio_bgm_stop(void);    /* 停止 BGM 任务并关闭功放 */
void      sdgoods_audio_sfx_flap(void);    /* 触发一次“拍翅”音效（仅在 BGM 运行时混合输出） */
void      sdgoods_audio_set_volume(int pct); /* 设置全局音量（0~100），背景音与音效同步缩放 */
int       sdgoods_audio_get_volume(void);    /* 读取当前音量（0~100） */

/* ---------------------------------------------------------------------------
 * 一次性声音片段（Sugarat 增加，用于开机动画音效）
 * 16 kHz 单声道 PCM，由后台任务按片段自身电平播放一次（不随全局音量缩放；全局音量为 0 时
 * 功放保持关闭）。数据通过 read 回调分块拉取，调用方不必一次放进内存。
 * ------------------------------------------------------------------------- */

/** 向 pcm 写入最多 max 个 16 kHz 单声道采样，返回写入个数；返回 0 表示片段结束。
 *  在音频任务上调用，arg 指向的数据必须在 sdgoods_audio_clip_busy() 变为 false 之前保持有效。 */
typedef size_t (*sdgoods_audio_clip_read_fn)(int16_t *pcm, size_t max, void *arg);

/** 启动片段播放后立即返回。ESP_ERR_INVALID_STATE：BGM 或另一个片段正在播放，或扬声器未初始化；
 *  ESP_ERR_NO_MEM：任务创建失败。 */
esp_err_t sdgoods_audio_clip_start(sdgoods_audio_clip_read_fn read, void *arg);

/** 片段任务仍在运行（包括尾部静音和关功放）时为 true。 */
bool sdgoods_audio_clip_busy(void);

/** 片段第一个采样到达喇叭的 esp_timer 时间（微秒），用于音画同步；尚未开始写入时为 0。 */
int64_t sdgoods_audio_clip_start_us(void);
