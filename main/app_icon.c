/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "app_icon.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp32s3/rom/tjpgd.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdgoods_app_sdk.h"
#include "sdgoods_board.h"

static const char *TAG = "app_icon";
#define CACHE_SLOTS   24
#define LARGE_PIXELS  (APP_ICON_LARGE * APP_ICON_LARGE)
#define SMALL_PIXELS  (APP_ICON_SMALL * APP_ICON_SMALL)
#define DECODE_POOL   4096          /* TJpgDec work area; ChaN's minimum for baseline images is 3100 bytes. */
#define FILE_MAGIC    0x314E4349u   /* "ICN1" */
#define BACKGROUND    0x2B2925      /* Card color behind covers smaller than the icon. */

typedef struct {
    uint32_t play_id;   /* 0 = free */
    uint32_t used;      /* Insertion order, for replacing the oldest entry. */
    app_icon_t icon;
    lv_color_t *large, *small;
} entry_t;

typedef struct { uint32_t magic; uint16_t width, height; } file_header_t;

static struct {
    SemaphoreHandle_t lock;
    entry_t entries[CACHE_SLOTS];
    uint32_t clock;
    char dir[96];
    bool dir_ok;
} s;

static void describe(lv_img_dsc_t *dsc, lv_color_t *pixels, uint16_t side) {
    memset(dsc, 0, sizeof *dsc);
    dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    dsc->header.w = side;
    dsc->header.h = side;
    dsc->data_size = (uint32_t)side * side * sizeof(lv_color_t);
    dsc->data = (const uint8_t *)pixels;
}

void app_icon_init(void) {
    if (s.lock) return;
    s.lock = xSemaphoreCreateMutex();
    for (int i = 0; i < CACHE_SLOTS; i++) {
        entry_t *e = &s.entries[i];
        e->large = heap_caps_malloc(LARGE_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        e->small = heap_caps_malloc(SMALL_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!e->large || !e->small) { ESP_LOGE(TAG, "no PSRAM for icon cache entry %d", i); break; }
        describe(&e->icon.large, e->large, APP_ICON_LARGE);
        describe(&e->icon.small, e->small, APP_ICON_SMALL);
    }
    char base[80];
    if (sdgoods_appdata_begin("launcher", base, sizeof base) == ESP_OK) {
        snprintf(s.dir, sizeof s.dir, "%s/icons", base);
        sdgoods_touch_flash_window(SDGOODS_TOUCH_FLASH_WIN_MAX_MS);
        s.dir_ok = mkdir(s.dir, 0775) == 0 || access(s.dir, F_OK) == 0;
        sdgoods_touch_flash_window_end();
    }
    if (!s.dir_ok) ESP_LOGW(TAG, "icon directory unavailable; icons last until restart");
}

static entry_t *find_locked(uint32_t play_id) {
    for (int i = 0; i < CACHE_SLOTS; i++)
        if (s.entries[i].play_id == play_id && s.entries[i].large) return &s.entries[i];
    return NULL;
}

/* Entry for `play_id`: its own, else a free one, else the oldest. Caller holds the lock. */
static entry_t *claim_locked(uint32_t play_id) {
    entry_t *e = find_locked(play_id), *oldest = NULL;
    for (int i = 0; !e && i < CACHE_SLOTS; i++) {
        entry_t *c = &s.entries[i];
        if (!c->large) continue;
        if (!c->play_id) { e = c; break; }
        if (!oldest || c->used < oldest->used) oldest = c;
    }
    if (!e) e = oldest;
    if (e) { e->play_id = play_id; e->used = ++s.clock; }
    return e;
}

/* Average 2 x 2 blocks of the large icon into the small one. */
static void shrink(const lv_color_t *large, lv_color_t *small) {
    for (int y = 0; y < APP_ICON_SMALL; y++)
        for (int x = 0; x < APP_ICON_SMALL; x++) {
            uint32_t r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    lv_color32_t c = { .full = lv_color_to32(large[(y * 2 + dy) * APP_ICON_LARGE + x * 2 + dx]) };
                    r += c.ch.red; g += c.ch.green; b += c.ch.blue;
                }
            small[y * APP_ICON_SMALL + x] = lv_color_make(r / 4, g / 4, b / 4);
        }
}

static bool store_locked(uint32_t play_id, const lv_color_t *large) {
    entry_t *e = claim_locked(play_id);
    if (!e) return false;
    memcpy(e->large, large, LARGE_PIXELS * sizeof(lv_color_t));
    shrink(e->large, e->small);
    return true;
}

typedef struct {
    const uint8_t *data;
    size_t len, pos;
    lv_color_t *out;
    int ox, oy;
} decode_t;

static UINT jpeg_input(JDEC *jd, BYTE *buf, UINT n) {
    decode_t *d = jd->device;
    size_t left = d->len - d->pos;
    if (n > left) n = (UINT)left;
    if (buf) memcpy(buf, d->data + d->pos, n);
    d->pos += n;
    return n;
}

static UINT jpeg_output(JDEC *jd, void *bitmap, JRECT *rect) {
    decode_t *d = jd->device;
    const uint8_t *rgb = bitmap;
    for (int y = rect->top; y <= rect->bottom; y++)
        for (int x = rect->left; x <= rect->right; x++, rgb += 3) {
            int px = d->ox + x, py = d->oy + y;
            if (px >= 0 && px < APP_ICON_LARGE && py >= 0 && py < APP_ICON_LARGE)
                d->out[py * APP_ICON_LARGE + px] = lv_color_make(rgb[0], rgb[1], rgb[2]);
        }
    return 1;
}

bool app_icon_decode(uint32_t play_id, const uint8_t *jpeg, size_t len) {
    if (!s.lock || !play_id || len < 4 || jpeg[0] != 0xFF || jpeg[1] != 0xD8) return false;
    lv_color_t *pixels = heap_caps_malloc(LARGE_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *pool = heap_caps_malloc(DECODE_POOL, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bool ok = false;
    if (pixels && pool) {
        lv_color_t fill = lv_color_hex(BACKGROUND);
        for (int i = 0; i < LARGE_PIXELS; i++) pixels[i] = fill;
        decode_t d = { .data = jpeg, .len = len, .out = pixels };
        JDEC jd;
        JRESULT result = jd_prepare(&jd, jpeg_input, pool, DECODE_POOL, &d);
        if (result == JDR_OK) {
            uint8_t scale = 0;
            while (scale < 3 && ((jd.width >> scale) > APP_ICON_LARGE || (jd.height >> scale) > APP_ICON_LARGE)) scale++;
            if ((jd.width >> scale) > APP_ICON_LARGE || (jd.height >> scale) > APP_ICON_LARGE) {
                result = JDR_FMT2;
            } else {
                d.ox = (APP_ICON_LARGE - (int)(jd.width >> scale)) / 2;
                d.oy = (APP_ICON_LARGE - (int)(jd.height >> scale)) / 2;
                result = jd_decomp(&jd, jpeg_output, scale);
            }
        }
        if (result == JDR_OK) {
            xSemaphoreTake(s.lock, portMAX_DELAY);
            ok = store_locked(play_id, pixels);
            xSemaphoreGive(s.lock);
        } else {
            ESP_LOGW(TAG, "play %lu: cover is not a readable baseline JPEG (TJpgDec %d)", (unsigned long)play_id, result);
        }
    }
    free(pool);
    free(pixels);
    return ok;
}

static void icon_path(char *out, size_t cap, uint32_t play_id) {
    snprintf(out, cap, "%s/%lu.icon", s.dir, (unsigned long)play_id);
}

bool app_icon_persist(uint32_t play_id) {
    if (!s.lock || !s.dir_ok) return false;
    char path[128], temp[128];
    icon_path(path, sizeof path, play_id);
    snprintf(temp, sizeof temp, "%s/icon.tmp", s.dir);
    file_header_t header = { FILE_MAGIC, APP_ICON_LARGE, APP_ICON_LARGE };
    xSemaphoreTake(s.lock, portMAX_DELAY);
    entry_t *e = find_locked(play_id);
    bool ok = e != NULL;
    if (ok) {
        sdgoods_touch_flash_window(SDGOODS_TOUCH_FLASH_WIN_MAX_MS);
        FILE *f = fopen(temp, "wb");
        ok = f && fwrite(&header, sizeof header, 1, f) == 1 && fwrite(e->large, sizeof(lv_color_t), LARGE_PIXELS, f) == LARGE_PIXELS;
        if (f && fclose(f) != 0) ok = false;
        if (ok) { unlink(path); ok = rename(temp, path) == 0; }
        else unlink(temp);
        sdgoods_touch_flash_window_end();
    }
    xSemaphoreGive(s.lock);
    if (e && !ok) ESP_LOGW(TAG, "could not write %s", path);
    return ok;
}

bool app_icon_load(uint32_t play_id) {
    if (!s.lock || !s.dir_ok || !play_id) return false;
    char path[128];
    icon_path(path, sizeof path, play_id);
    lv_color_t *pixels = heap_caps_malloc(LARGE_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pixels) return false;
    file_header_t header;
    FILE *f = fopen(path, "rb");
    bool ok = f && fread(&header, sizeof header, 1, f) == 1 && header.magic == FILE_MAGIC &&
              header.width == APP_ICON_LARGE && header.height == APP_ICON_LARGE &&
              fread(pixels, sizeof(lv_color_t), LARGE_PIXELS, f) == LARGE_PIXELS;
    if (f) fclose(f);
    if (ok) {
        xSemaphoreTake(s.lock, portMAX_DELAY);
        ok = store_locked(play_id, pixels);
        xSemaphoreGive(s.lock);
    }
    free(pixels);
    return ok;
}

void app_icon_remove(uint32_t play_id) {
    if (!s.lock || !s.dir_ok || !play_id) return;
    char path[128];
    icon_path(path, sizeof path, play_id);
    sdgoods_touch_flash_window(SDGOODS_TOUCH_FLASH_WIN_MAX_MS);
    unlink(path);
    sdgoods_touch_flash_window_end();
}

const app_icon_t *app_icon_find(uint32_t play_id) {
    if (!s.lock || !play_id) return NULL;
    xSemaphoreTake(s.lock, portMAX_DELAY);
    entry_t *e = find_locked(play_id);
    xSemaphoreGive(s.lock);
    return e ? &e->icon : NULL;
}
