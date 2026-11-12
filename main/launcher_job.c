/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "launcher_job.h"
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "app_icon.h"
#include "esp_flash.h"
#include "esp_rom_crc.h"
#include "slot_table.h"

static const char *TAG = "job";
/* Internal-RAM stack: flash writes run on this task, and a PSRAM stack is unreachable while the cache is off. */
#define STACK_BYTES 8192

static struct {
    SemaphoreHandle_t lock, wake;
    launcher_job_status_t status;
    bool busy;
    volatile bool cancel;
    volatile uint32_t received;
    uint32_t lookup_id;
    int slot;
    bool release_first;
    app_problem_t sink_problem;
} s;

static void update(void (*apply)(launcher_job_status_t *, const void *), const void *arg) {
    xSemaphoreTake(s.lock, portMAX_DELAY);
    apply(&s.status, arg);
    xSemaphoreGive(s.lock);
}

static void set_step(launcher_job_status_t *st, const void *arg) { st->step = *(const launcher_job_step_t *)arg; }

static bool write_chunk(const uint8_t *data, size_t len, void *context) {
    (void)context;
    s.sink_problem = app_store_install_write(data, len);
    return s.sink_problem == APP_OK;
}

/* Best effort: a missing or unreadable cover leaves the category icon in place. */
static void fetch_icon(const community_play_t *play) {
    uint8_t *jpeg;
    size_t len;
    if (community_fetch_cover(play, &jpeg, &len) != COMMUNITY_OK) return;
    app_icon_decode(play->play_id, jpeg, len);
    free(jpeg);
}

static void run_lookup(launcher_job_status_t *done) {
    done->network = community_fetch_play(s.lookup_id, &done->play);
    if (done->network == COMMUNITY_OK) fetch_icon(&done->play);
}

static void run_install(launcher_job_status_t *done) {
    const community_play_t *play = &done->play;
    app_install_t request = { .play_id = play->play_id, .image_bytes = play->size, .replaces_slot = s.slot,
                              .release_first = s.release_first };
    memcpy(request.title, play->title, sizeof request.title);
    memcpy(request.category, play->category, sizeof request.category);
    memcpy(request.sha256, play->sha256, 32);
    done->problem = app_store_install_begin(&request);
    if (done->problem != APP_OK) return;
    launcher_job_step_t step = JOB_STEP_DOWNLOAD;
    update(set_step, &step);
    s.sink_problem = APP_OK;
    done->network = community_download(play, write_chunk, NULL, &s.cancel, &s.received);
    if (done->network != COMMUNITY_OK) {
        if (done->network == COMMUNITY_SINK) done->problem = s.sink_problem;
        app_store_install_abort();
        return;
    }
    step = JOB_STEP_VERIFY;
    update(set_step, &step);
    done->problem = app_store_install_end(&done->installed);
    if (done->problem != APP_OK) return;
    if (!app_icon_find(play->play_id)) fetch_icon(play);
    app_icon_persist(play->play_id);
}

static void run_uninstall(launcher_job_status_t *done) {
    app_info_t *apps = heap_caps_calloc(APP_STORE_MAX, sizeof *apps, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!apps) { done->problem = APP_ERR_NO_MEM; return; }
    int count = app_store_snapshot(apps, APP_STORE_MAX, NULL);
    for (int i = 0; i < count && done->problem == APP_OK; i++) {
        if (s.slot >= 0 && apps[i].slot != s.slot) continue;
        done->problem = app_store_uninstall(apps[i].slot, false);
        if (done->problem == APP_OK) app_icon_remove(apps[i].play_id);
    }
    free(apps);
}

static void load_icons(void) {
    app_icon_init();
    app_info_t *apps = heap_caps_calloc(APP_STORE_MAX, sizeof *apps, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!apps) return;
    int count = app_store_snapshot(apps, APP_STORE_MAX, NULL);
    for (int i = 0; i < count; i++) {
        if (!apps[i].play_id) continue;
        uint8_t header[24];
        uint32_t at = apps[i].offset + apps[i].size - LT_SLOT_RESERVE;
        if (apps[i].size > LT_SLOT_RESERVE && esp_flash_read(NULL, header, at, sizeof header) == ESP_OK &&
            !memcmp(header, "SGAPP001", 8)) {
            uint32_t meta, jpeg, crc;
            memcpy(&meta, header + 8, 4); memcpy(&jpeg, header + 12, 4); memcpy(&crc, header + 16, 4);
            if (meta && meta <= 4096 && jpeg && jpeg <= 48 * 1024 && meta + jpeg <= LT_SLOT_RESERVE - sizeof header) {
                uint8_t *data = heap_caps_malloc(meta + jpeg, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (data && esp_flash_read(NULL, data, at + sizeof header, meta + jpeg) == ESP_OK &&
                    esp_rom_crc32_le(0, data, meta + jpeg) == crc && app_icon_decode(apps[i].play_id, data + meta, jpeg)) {
                    free(data); continue;
                }
                free(data);
            }
        }
        app_icon_load(apps[i].play_id);
    }
    free(apps);
}

static void finish(launcher_job_status_t *st, const void *arg) {
    const launcher_job_status_t *done = arg;
    st->network = done->network;
    st->problem = done->problem;
    st->play = done->play;
    st->installed = done->installed;
    st->phase = done->network == COMMUNITY_OK && done->problem == APP_OK ? JOB_SUCCEEDED : JOB_FAILED;
}

static void worker(void *arg) {
    (void)arg;
    app_store_init();
    load_icons();
    for (;;) {
        xSemaphoreTake(s.wake, portMAX_DELAY);
        launcher_job_status_t done;
        xSemaphoreTake(s.lock, portMAX_DELAY);
        done = s.status;
        xSemaphoreGive(s.lock);
        done.network = COMMUNITY_OK;
        done.problem = APP_OK;
        switch (done.kind) {
        case JOB_LOOKUP: run_lookup(&done); break;
        case JOB_INSTALL: run_install(&done); break;
        case JOB_UNINSTALL: run_uninstall(&done); break;
        case JOB_NONE: break;
        }
        ESP_LOGI(TAG, "job %d finished: network %d, storage %d", done.kind, done.network, done.problem);
        update(finish, &done);
        s.busy = false;
    }
}

void launcher_job_start(void) {
    s.lock = xSemaphoreCreateMutex();
    s.wake = xSemaphoreCreateBinary();
    if (!s.lock || !s.wake || xTaskCreate(worker, "launcher_job", STACK_BYTES, NULL, 4, NULL) != pdPASS)
        ESP_LOGE(TAG, "worker could not start; downloads and app changes are unavailable");
}

typedef struct { launcher_job_kind_t kind; const community_play_t *play; } begin_t;

static void begin(launcher_job_status_t *st, const void *arg) {
    const begin_t *b = arg;
    uint32_t sequence = st->sequence + 1;
    memset(st, 0, sizeof *st);
    st->sequence = sequence;
    st->kind = b->kind;
    st->phase = JOB_RUNNING;
    st->step = JOB_STEP_PREPARE;
    if (b->play) { st->play = *b->play; st->total = b->play->size; }
}

static bool start(launcher_job_kind_t kind, const community_play_t *play) {
    if (!s.lock || s.busy) return false;
    s.busy = true;
    s.cancel = false;
    s.received = 0;
    begin_t b = { kind, play };
    update(begin, &b);
    xSemaphoreGive(s.wake);
    return true;
}

bool launcher_job_lookup(uint32_t play_id) {
    if (s.busy) return false;
    s.lookup_id = play_id;
    return start(JOB_LOOKUP, NULL);
}

bool launcher_job_install(const community_play_t *play, int replaces_slot, bool release_first) {
    if (s.busy) return false;
    s.slot = replaces_slot;
    s.release_first = release_first;
    return start(JOB_INSTALL, play);
}

bool launcher_job_uninstall(int slot) {
    if (s.busy) return false;
    s.slot = slot;
    return start(JOB_UNINSTALL, NULL);
}

void launcher_job_cancel(void) { s.cancel = true; }

void launcher_job_status(launcher_job_status_t *out) {
    if (!s.lock) { memset(out, 0, sizeof *out); return; }
    xSemaphoreTake(s.lock, portMAX_DELAY);
    *out = s.status;
    xSemaphoreGive(s.lock);
    out->received = s.received;
}
