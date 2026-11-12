/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "app_store.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/sha256.h"
#include "flash_layout.h"
#include "cJSON.h"
#include "esp_rom_crc.h"
#include "sdgoods_app_sdk.h"
#include "sdgoods_board.h"

static const char *TAG = "app_store";
#define HEADER_BYTES (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
#define RECORDS_FILE "apps.tsv"
#define RECORDS_TEMP "apps.tmp"

static struct {
    SemaphoreHandle_t lock;
    app_store_state_t state;
    lt_table_t *table;                 /* Worker-owned working copy. */
    app_info_t *apps;                  /* Published under `lock` (PSRAM). */
    app_info_t *work;                  /* Worker scratch list (PSRAM). */
    int count;
    app_space_t space;
    bool data_ok;
    char dir[64];
    struct {
        bool active, header_checked;
        app_install_t request;
        int slot;
        uint32_t offset, size, written, erased_end;
        mbedtls_sha256_context sha;
    } job;
} s = { .state = APP_STORE_LOADING };

static void copy_text(char *dst, size_t cap, const char *src) {
    size_t n = strnlen(src, cap - 1);
    while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;   /* never split a UTF-8 sequence */
    memcpy(dst, src, n);
    dst[n] = 0;
    for (char *p = dst; *p; p++) if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
}

static bool valid_project(const char *name) {
    if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) return false;
    static const char *const reserved[] = { "wifi", "device", "push", "launcher" };
    for (size_t i = 0; i < sizeof reserved / sizeof *reserved; i++) if (!strcasecmp(name, reserved[i])) return false;
    for (const char *p = name; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.'))
            return false;
    return true;
}

/* Read the image header and application description at `offset`. */
static bool read_identity(uint32_t offset, char project[33], char version[33]) {
    uint8_t *buf = heap_caps_malloc(HEADER_BYTES, MALLOC_CAP_8BIT);
    if (!buf) return false;
    bool ok = esp_flash_read(NULL, buf, offset, HEADER_BYTES) == ESP_OK;
    const esp_image_header_t *header = (const esp_image_header_t *)buf;
    const esp_app_desc_t *desc = (const esp_app_desc_t *)(buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
    ok = ok && header->magic == ESP_IMAGE_HEADER_MAGIC && header->chip_id == ESP_CHIP_ID_ESP32S3 &&
         desc->magic_word == ESP_APP_DESC_MAGIC_WORD;
    if (ok) {
        memcpy(project, desc->project_name, 32); project[32] = 0;
        memcpy(version, desc->version, 32); version[32] = 0;
        ok = valid_project(project);
    }
    free(buf);
    return ok;
}

static bool verify_image(uint32_t offset, uint32_t size, uint32_t *image_len) {
    esp_image_metadata_t data = {0};
    const esp_partition_pos_t pos = { .offset = offset, .size = size };
    if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &pos, &data) != ESP_OK) return false;
    *image_len = data.image_len;
    return true;
}

/* ---- launcher records (/appdata/launcher/apps.tsv: slot, project, play, category, title) ---- */

typedef struct { int slot; char project[33]; uint32_t play_id; char category[24]; char title[64]; } record_t;
static record_t *records;   /* PSRAM, APP_STORE_MAX entries */

static int load_records(record_t *out, int cap) {
    if (!s.data_ok) return 0;
    char path[96];
    snprintf(path, sizeof path, "%s/%s", s.dir, RECORDS_FILE);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[192];
    int n = 0;
    while (n < cap && fgets(line, sizeof line, f)) {
        char *field[5];
        int k = 0;
        field[k++] = line;
        for (char *p = line; *p && k < 5; p++) if (*p == '\t') { *p = 0; field[k++] = p + 1; }
        if (k != 5) continue;
        field[4][strcspn(field[4], "\r\n")] = 0;
        record_t *r = &out[n++];
        r->slot = atoi(field[0]);
        copy_text(r->project, sizeof r->project, field[1]);
        r->play_id = strtoul(field[2], NULL, 10);
        copy_text(r->category, sizeof r->category, field[3]);
        copy_text(r->title, sizeof r->title, field[4]);
    }
    fclose(f);
    return n;
}

static void save_records(const app_info_t *apps, int count) {
    if (!s.data_ok) return;
    char path[96], temp[96];
    snprintf(path, sizeof path, "%s/%s", s.dir, RECORDS_FILE);
    snprintf(temp, sizeof temp, "%s/%s", s.dir, RECORDS_TEMP);
    sdgoods_touch_flash_window(SDGOODS_TOUCH_FLASH_WIN_MAX_MS);
    FILE *f = fopen(temp, "w");
    bool ok = f != NULL;
    for (int i = 0; ok && i < count; i++) {
        if (apps[i].damaged) continue;
        ok = fprintf(f, "%d\t%s\t%lu\t%s\t%s\n", apps[i].slot, apps[i].project, (unsigned long)apps[i].play_id,
                     apps[i].category, apps[i].title) > 0;
    }
    if (f && fclose(f) != 0) ok = false;
    if (ok) {
        unlink(path);
        ok = rename(temp, path) == 0;
    }
    sdgoods_touch_flash_window_end();
    if (!ok) ESP_LOGW(TAG, "could not save %s; titles fall back to project names", path);
}

static void remove_tree(const char *path, int depth) {
    enum { PATH_BYTES = 320 };
    DIR *dir = depth < 8 ? opendir(path) : NULL;
    char *child = dir ? heap_caps_malloc(PATH_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
    if (dir && child) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            int len = snprintf(child, PATH_BYTES, "%s/%s", path, entry->d_name);
            if (len < 0 || len >= PATH_BYTES) { ESP_LOGW(TAG, "skipped a path longer than %d bytes", PATH_BYTES); continue; }
            struct stat st;
            if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) remove_tree(child, depth + 1);
            else {
                sdgoods_touch_flash_window(1000);
                unlink(child);
                sdgoods_touch_flash_window_end();
            }
        }
    }
    if (dir) closedir(dir);
    free(child);
    sdgoods_touch_flash_window(1000);
    rmdir(path);
    sdgoods_touch_flash_window_end();
}

/* Desktop installs keep version-bound metadata in the slot's reserved tail. */
static void read_desktop_record(app_info_t *app) {
    uint8_t header[24];
    uint32_t at = app->offset + app->size - LT_SLOT_RESERVE;
    if (app->size <= LT_SLOT_RESERVE || esp_flash_read(NULL, header, at, sizeof header) != ESP_OK ||
        memcmp(header, "SGAPP001", 8)) return;
    uint32_t json_len, jpeg_len, crc;
    memcpy(&json_len, header + 8, 4); memcpy(&jpeg_len, header + 12, 4); memcpy(&crc, header + 16, 4);
    if (!json_len || json_len > 4096 || jpeg_len > 48 * 1024 || json_len + jpeg_len > LT_SLOT_RESERVE - sizeof header) return;
    size_t len = json_len + jpeg_len;
    uint8_t *payload = heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!payload) return;
    if (esp_flash_read(NULL, payload, at + sizeof header, len) == ESP_OK && esp_rom_crc32_le(0, payload, len) == crc) {
        cJSON *json = cJSON_ParseWithLength((const char *)payload, json_len);
        const cJSON *project = cJSON_GetObjectItemCaseSensitive(json, "project");
        const cJSON *play = cJSON_GetObjectItemCaseSensitive(json, "playId");
        const cJSON *title = cJSON_GetObjectItemCaseSensitive(json, "title");
        const cJSON *category = cJSON_GetObjectItemCaseSensitive(json, "category");
        if (cJSON_IsString(project) && !strcmp(project->valuestring, app->project) && cJSON_IsNumber(play) &&
            play->valuedouble > 0 && play->valuedouble <= UINT32_MAX && cJSON_IsString(title) && cJSON_IsString(category)) {
            app->play_id = (uint32_t)play->valuedouble;
            copy_text(app->title, sizeof app->title, title->valuestring);
            copy_text(app->category, sizeof app->category, category->valuestring);
        }
        cJSON_Delete(json);
    }
    free(payload);
}

/* ---- scanning ---- */

/* Rebuild the published list from the working table, reusing launcher records. */
static void scan(bool verify_unrecorded) {
    int known = load_records(records, APP_STORE_MAX);
    app_info_t *apps = s.work;
    int count = 0;
    bool learned = false;
    for (int slot = 0; slot < LT_OTA_SLOTS; slot++) {
        lt_slot_t geometry = lt_slot(s.table, slot);
        if (!geometry.size) continue;
        app_info_t *app = &apps[count++];
        memset(app, 0, sizeof *app);
        app->slot = slot;
        app->offset = geometry.offset;
        app->size = geometry.size;
        if (!read_identity(geometry.offset, app->project, app->version)) {
            app->damaged = true;
            continue;
        }
        copy_text(app->title, sizeof app->title, app->project);
        const record_t *record = NULL;
        for (int i = 0; i < known && !record; i++)
            if (records[i].slot == slot && !strcmp(records[i].project, app->project)) record = &records[i];
        read_desktop_record(app);
        if (record && app->play_id == 0) {
            app->play_id = record->play_id;
            copy_text(app->title, sizeof app->title, record->title);
            copy_text(app->category, sizeof app->category, record->category);
        }
        if (!record && verify_unrecorded) {
            uint32_t len;
            app->damaged = !verify_image(geometry.offset, geometry.size, &len);
            learned |= !app->damaged;
            ESP_LOGI(TAG, "ota_%d %s: %s", slot, app->project, app->damaged ? "image invalid" : "verified");
        }
    }
    xSemaphoreTake(s.lock, portMAX_DELAY);
    memcpy(s.apps, apps, sizeof(app_info_t) * count);
    s.count = count;
    s.space = (app_space_t){ s.table->pool_end - s.table->pool_start, lt_free_bytes(s.table), lt_largest_gap(s.table) };
    xSemaphoreGive(s.lock);
    if (learned || known != count) save_records(apps, count);
}

static const app_info_t *find_slot(int slot) {
    for (int i = 0; i < s.count; i++) if (s.apps[i].slot == slot) return &s.apps[i];
    return NULL;
}

app_problem_t app_store_init(void) {
    if (!s.lock) s.lock = xSemaphoreCreateMutex();
    const uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    if (!s.table) s.table = heap_caps_malloc(sizeof(lt_table_t), caps);
    if (!s.apps) s.apps = heap_caps_calloc(APP_STORE_MAX, sizeof(app_info_t), caps);
    if (!s.work) s.work = heap_caps_calloc(APP_STORE_MAX, sizeof(app_info_t), caps);
    if (!records) records = heap_caps_calloc(APP_STORE_MAX, sizeof(record_t), caps);
    if (!s.lock || !s.table || !s.apps || !s.work || !records) { s.state = APP_STORE_UNAVAILABLE; return APP_ERR_NO_MEM; }
    s.data_ok = sdgoods_appdata_begin("launcher", s.dir, sizeof s.dir) == ESP_OK;
    if (!s.data_ok) ESP_LOGW(TAG, "appdata unavailable; app titles are not kept");
    if (flash_layout_load(s.table) != ESP_OK) { s.state = APP_STORE_UNAVAILABLE; return APP_ERR_LAYOUT; }
    scan(true);
    s.state = APP_STORE_READY;
    ESP_LOGI(TAG, "%d app(s), %lu bytes free", s.count, (unsigned long)s.space.free);
    return APP_OK;
}

app_store_state_t app_store_state(void) { return s.state; }

int app_store_snapshot(app_info_t *out, int cap, app_space_t *space) {
    if (s.state != APP_STORE_READY) { if (space) *space = (app_space_t){0}; return 0; }
    xSemaphoreTake(s.lock, portMAX_DELAY);
    int n = s.count < cap ? s.count : cap;
    memcpy(out, s.apps, sizeof(app_info_t) * n);
    if (space) *space = s.space;
    xSemaphoreGive(s.lock);
    return n;
}

uint32_t app_store_slot_bytes(uint32_t image_bytes) { return lt_slot_bytes_for(image_bytes); }

/* Invalidate the image header, then empty the table row. */
static app_problem_t release_slot(int slot) {
    lt_slot_t geometry = lt_slot(s.table, slot);
    if (!geometry.size) return APP_OK;
    if (flash_layout_erase(geometry.offset, 0x1000) != ESP_OK) return APP_ERR_FLASH;
    lt_table_t *next = heap_caps_malloc(sizeof(lt_table_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!next) return APP_ERR_NO_MEM;
    memcpy(next, s.table, sizeof *next);
    app_problem_t problem = APP_OK;
    if (lt_set_slot(next, slot, 0, 0) != LT_OK || flash_layout_commit(next) != ESP_OK) problem = APP_ERR_FLASH;
    else memcpy(s.table, next, sizeof *next);
    free(next);
    return problem;
}

app_problem_t app_store_install_begin(const app_install_t *request) {
    if (s.state != APP_STORE_READY) return APP_ERR_LAYOUT;
    if (s.job.active || request->image_bytes < HEADER_BYTES) return APP_ERR_SIZE;
    if (request->release_first && request->replaces_slot >= 0) {
        app_problem_t problem = release_slot(request->replaces_slot);
        if (problem != APP_OK) return problem;
        scan(false);
    }
    uint32_t size = lt_slot_bytes_for(request->image_bytes);
    int slot;
    uint32_t offset;
    lt_status_t status = lt_allocate(s.table, size, &slot, &offset);
    if (status == LT_ERR_NO_SLOT) return APP_ERR_SLOTS;
    if (status != LT_OK) return APP_ERR_SPACE;
    lt_table_t *next = heap_caps_malloc(sizeof(lt_table_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!next) return APP_ERR_NO_MEM;
    memcpy(next, s.table, sizeof *next);
    bool committed = lt_set_slot(next, slot, offset, size) == LT_OK && flash_layout_commit(next) == ESP_OK;
    if (committed) memcpy(s.table, next, sizeof *next);
    free(next);
    if (!committed) return APP_ERR_FLASH;
    s.job.active = true;
    s.job.header_checked = false;
    s.job.request = *request;
    if (request->release_first) s.job.request.replaces_slot = -1;
    s.job.slot = slot;
    s.job.offset = offset;
    s.job.size = size;
    s.job.written = 0;
    s.job.erased_end = offset;
    mbedtls_sha256_init(&s.job.sha);
    mbedtls_sha256_starts(&s.job.sha, 0);
    ESP_LOGI(TAG, "installing play %lu into ota_%d at 0x%lx (+0x%lx)", (unsigned long)request->play_id, slot,
             (unsigned long)offset, (unsigned long)size);
    return APP_OK;
}

app_problem_t app_store_install_write(const uint8_t *data, size_t len) {
    if (!s.job.active) return APP_ERR_SIZE;
    if (len > s.job.request.image_bytes - s.job.written) return APP_ERR_SIZE;
    uint32_t end = s.job.offset + s.job.written + len;
    while (s.job.erased_end < end) {
        if (flash_layout_erase(s.job.erased_end, LT_SLOT_ALIGN) != ESP_OK) return APP_ERR_FLASH;
        s.job.erased_end += LT_SLOT_ALIGN;
    }
    if (flash_layout_write(s.job.offset + s.job.written, data, len) != ESP_OK) return APP_ERR_FLASH;
    mbedtls_sha256_update(&s.job.sha, data, len);
    s.job.written += len;
    if (!s.job.header_checked && s.job.written >= HEADER_BYTES) {
        char project[33], version[33];
        if (!read_identity(s.job.offset, project, version)) return APP_ERR_IMAGE;
        s.job.header_checked = true;
    }
    return APP_OK;
}

void app_store_install_abort(void) {
    if (!s.job.active) return;
    s.job.active = false;
    mbedtls_sha256_free(&s.job.sha);
    if (release_slot(s.job.slot) != APP_OK) ESP_LOGE(TAG, "could not release ota_%d; it is listed as damaged", s.job.slot);
    scan(false);
}

app_problem_t app_store_install_end(app_info_t *installed) {
    if (!s.job.active) return APP_ERR_SIZE;
    uint8_t digest[32];
    mbedtls_sha256_finish(&s.job.sha, digest);
    app_problem_t problem = APP_OK;
    uint32_t image_len = 0;
    char project[33] = "", version[33] = "";
    if (s.job.written != s.job.request.image_bytes) problem = APP_ERR_SIZE;
    else if (memcmp(digest, s.job.request.sha256, 32) != 0) problem = APP_ERR_HASH;
    else if (!verify_image(s.job.offset, s.job.size, &image_len) || image_len != s.job.written ||
             !read_identity(s.job.offset, project, version)) problem = APP_ERR_IMAGE;
    int replaced = s.job.request.replaces_slot;
    for (int i = 0; problem == APP_OK && i < s.count; i++) {
        const app_info_t *other = &s.apps[i];
        if (other->damaged || other->slot == replaced || strcmp(other->project, project)) continue;
        if (other->play_id && other->play_id != s.job.request.play_id) problem = APP_ERR_CONFLICT;
        else replaced = other->slot;   /* same identity, installed manually or earlier: this is its update */
    }
    if (problem != APP_OK) {
        ESP_LOGE(TAG, "install into ota_%d rejected (problem %d)", s.job.slot, problem);
        app_store_install_abort();
        return problem;
    }
    s.job.active = false;
    mbedtls_sha256_free(&s.job.sha);
    if (replaced >= 0 && release_slot(replaced) != APP_OK) ESP_LOGE(TAG, "old ota_%d could not be released", replaced);

    app_info_t *apps = s.work;
    int count = 0;
    xSemaphoreTake(s.lock, portMAX_DELAY);
    for (int i = 0; i < s.count; i++) if (s.apps[i].slot != replaced) apps[count++] = s.apps[i];
    xSemaphoreGive(s.lock);
    app_info_t *app = &apps[count++];
    memset(app, 0, sizeof *app);
    app->slot = s.job.slot;
    app->offset = s.job.offset;
    app->size = s.job.size;
    app->play_id = s.job.request.play_id;
    copy_text(app->project, sizeof app->project, project);
    copy_text(app->version, sizeof app->version, version);
    copy_text(app->title, sizeof app->title, s.job.request.title[0] ? s.job.request.title : project);
    copy_text(app->category, sizeof app->category, s.job.request.category);
    save_records(apps, count);
    if (installed) *installed = *app;
    scan(false);
    ESP_LOGI(TAG, "installed %s %s in ota_%d", project, version, s.job.slot);
    return APP_OK;
}

app_problem_t app_store_uninstall(int slot, bool keep_data) {
    if (s.state != APP_STORE_READY || slot < 0 || slot >= LT_OTA_SLOTS) return APP_ERR_LAYOUT;
    char project[33] = "";
    xSemaphoreTake(s.lock, portMAX_DELAY);
    const app_info_t *app = find_slot(slot);
    if (app && !app->damaged) memcpy(project, app->project, sizeof project);
    xSemaphoreGive(s.lock);
    app_problem_t problem = release_slot(slot);
    if (problem != APP_OK) return problem;
    if (!keep_data && project[0] && valid_project(project) && sdgoods_appdata_mount() == ESP_OK) {
        char path[96];
        snprintf(path, sizeof path, "/appdata/%s", project);
        remove_tree(path, 0);
    }
    scan(false);
    int count = app_store_snapshot(s.work, APP_STORE_MAX, NULL);
    save_records(s.work, count);
    ESP_LOGI(TAG, "removed ota_%d %s%s", slot, project, keep_data ? " (data kept)" : "");
    return APP_OK;
}

app_problem_t app_store_select_boot(int slot) {
    xSemaphoreTake(s.lock, portMAX_DELAY);
    const app_info_t *app = find_slot(slot);
    bool bootable = app && !app->damaged;
    xSemaphoreGive(s.lock);
    if (!bootable) return APP_ERR_IMAGE;
    return flash_layout_boot_slot(slot) == ESP_OK ? APP_OK : APP_ERR_FLASH;
}
