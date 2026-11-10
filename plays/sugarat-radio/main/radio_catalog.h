/* SPDX-License-Identifier: Apache-2.0
 * Modified for Sugarat Radio by Deotaland, 2026.
 * US station directory, loaded on a worker; callers must await completion before reading.
 * Empty region selects United States; retired ids resolve to nationwide.
 * Network failure falls back to bundled URLs, which still require Internet access.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RADIO_CATALOG_MAX  64
#define RADIO_NAME_MAX     128
#define RADIO_URL_MAX      200
#define RADIO_CODEC_MAX    8
#define RADIO_COUNTRY_MAX  4
#define RADIO_TAG_MAX      40

typedef struct {
    char     name[RADIO_NAME_MAX];
    char     url[RADIO_URL_MAX];      /* 拉流地址（优先 url_resolved） */
    char     codec[RADIO_CODEC_MAX];  /* "MP3" / "AAC" ...，供播放器判编码 */
    char     country[RADIO_COUNTRY_MAX];
    char     tag[RADIO_TAG_MAX];
    uint32_t bitrate;
    int      votes;
    float    freq_mhz;                /* 解析出的 FM 频率；0 = 无 */
} radio_station_t;

/** Load US stations for a market id; return count or zero on allocation failure. */
int radio_catalog_load(const char *region_id);

int                    radio_catalog_count(void);
const radio_station_t *radio_catalog_at(int idx);
const radio_station_t *radio_catalog_next(int idx);   /* 循环取下一台（失败跳台用） */

/* Country is US after load. */
const char *radio_catalog_country(void);

/* Display label of the selected market or US presets. */
const char *radio_catalog_city(void);

/* 从台名解析 FM 频率（87.0~108.0 MHz），失败返回 0。也可独立用于测试。 */
float radio_parse_freq(const char *name);
