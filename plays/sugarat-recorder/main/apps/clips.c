/* SPDX-License-Identifier: Apache-2.0 */
/* Persistent WAV files in the platform's application-private FAT directory. */

#include "clips.h"

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "sdgoods_app_sdk.h"
#include "sdgoods_input.h"
#include "sugarat_audio.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define HEADER SUGARAT_AUDIO_WAV_HEADER_BYTES
#define RATE SUGARAT_AUDIO_SAMPLE_RATE
#define MAX_SAMPLES (RATE * CLIP_MAX_MS / 1000u)
#define FLASH_WINDOW_MS 300
#define WRITE_CHUNK (32u * 1024u)

static void clip_path(char path[96], const clip_store_t *store, uint32_t id)
{
    snprintf(path, 96, "%s/REC%08" PRIu32 ".WAV", store->path, id);
}

static uint32_t clip_id(const char *name)
{
    if (strlen(name) != 15 || memcmp(name, "REC", 3) != 0 || strcasecmp(name + 11, ".WAV") != 0) {
        return 0;
    }
    uint32_t id = 0;
    for (int i = 3; i < 11; ++i) {
        if (name[i] < '0' || name[i] > '9') return 0;
        id = id * 10 + (unsigned)(name[i] - '0');
    }
    return id;
}

static int newest_first(const void *first, const void *second)
{
    const voice_clip_t *a = first, *b = second;
    return (a->id < b->id) - (a->id > b->id);
}

esp_err_t clips_open(clip_store_t *store)
{
    sdgoods_touch_flash_window(FLASH_WINDOW_MS);
    esp_err_t error = sdgoods_appdata_begin(esp_app_get_description()->project_name,
                                           store->path, sizeof(store->path));
    sdgoods_touch_flash_window_end();
    if (error != ESP_OK) return error;
    DIR *dir = opendir(store->path);
    if (!dir) return ESP_FAIL;

    store->count = 0;
    store->next_id = 1;
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno) error = ESP_FAIL;
            break;
        }
        const uint32_t id = clip_id(entry->d_name);
        if (!id) continue;
        if (id >= store->next_id) store->next_id = id + 1;
        char path[96];
        clip_path(path, store, id);
        struct stat info;
        if (stat(path, &info) != 0) {
            error = ESP_FAIL;
            break;
        }
        if (!S_ISREG(info.st_mode) || info.st_size <= HEADER ||
            (info.st_size - HEADER) % sizeof(int16_t) != 0) continue;
        const size_t samples = (info.st_size - HEADER) / sizeof(int16_t);
        if (samples > MAX_SAMPLES) continue;
        if (store->count == CLIP_LIMIT) {
            error = ESP_ERR_INVALID_SIZE;
            break;
        }
        store->clips[store->count++] = (voice_clip_t){id, samples};
    }
    closedir(dir);
    qsort(store->clips, store->count, sizeof(store->clips[0]), newest_first);
    return error;
}

static esp_err_t write_error(void)
{
    return errno == ENOSPC ? ESP_ERR_NO_MEM : ESP_FAIL;
}

esp_err_t clips_save(clip_store_t *store, const int16_t *pcm, size_t samples)
{
    if (!pcm || !samples || samples > MAX_SAMPLES) return ESP_ERR_INVALID_ARG;
    if (store->count >= CLIP_LIMIT || store->next_id > 99999999) return ESP_ERR_INVALID_SIZE;
    char partial[96], path[96];
    snprintf(partial, sizeof(partial), "%s/AUDIO.TMP", store->path);
    clip_path(path, store, store->next_id);
    uint8_t header[HEADER];
    sugarat_audio_write_wav_header(header, samples, 1);

    sdgoods_touch_flash_window(FLASH_WINDOW_MS);
    FILE *file = fopen(partial, "wb");
    esp_err_t error = ESP_OK;
    if (!file || fwrite(header, 1, sizeof(header), file) != sizeof(header)) error = write_error();
    sdgoods_touch_flash_window_end();
    if (!file) return error;

    const uint8_t *data = (const uint8_t *)pcm;
    const size_t bytes = samples * sizeof(*pcm);
    for (size_t offset = 0; error == ESP_OK && offset < bytes;) {
        const size_t chunk = bytes - offset < WRITE_CHUNK ? bytes - offset : WRITE_CHUNK;
        sdgoods_touch_flash_window(FLASH_WINDOW_MS);
        if (fwrite(data + offset, 1, chunk, file) != chunk) error = write_error();
        sdgoods_touch_flash_window_end();
        offset += chunk;
    }

    sdgoods_touch_flash_window(FLASH_WINDOW_MS);
    if (error == ESP_OK && (fflush(file) != 0 || fsync(fileno(file)) != 0)) error = write_error();
    if (fclose(file) != 0 && error == ESP_OK) error = write_error();
    if (error == ESP_OK && rename(partial, path) != 0) error = write_error();
    if (error != ESP_OK) unlink(partial);
    sdgoods_touch_flash_window_end();

    if (error == ESP_OK) {
        memmove(store->clips + 1, store->clips, store->count * sizeof(store->clips[0]));
        store->clips[0] = (voice_clip_t){store->next_id++, samples};
        ++store->count;
    }
    return error;
}

esp_err_t clips_read(const clip_store_t *store, voice_clip_t clip, uint8_t **wav, size_t *bytes)
{
    char path[96];
    clip_path(path, store, clip.id);
    FILE *file = fopen(path, "rb");
    if (!file) return ESP_ERR_NOT_FOUND;
    const size_t size = HEADER + clip.samples * sizeof(int16_t);
    uint8_t *buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }
    uint8_t expected[HEADER];
    sugarat_audio_write_wav_header(expected, clip.samples, 1);
    const bool ok = fread(buffer, 1, size, file) == size && fgetc(file) == EOF && !ferror(file) &&
                    memcmp(buffer, expected, HEADER) == 0;
    fclose(file);
    if (!ok) {
        free(buffer);
        return ESP_FAIL;
    }
    *wav = buffer;
    *bytes = size;
    return ESP_OK;
}

esp_err_t clips_delete(clip_store_t *store, uint32_t id)
{
    for (size_t i = 0; i < store->count; ++i) {
        if (store->clips[i].id != id) continue;
        char path[96];
        clip_path(path, store, id);
        sdgoods_touch_flash_window(FLASH_WINDOW_MS);
        int result = unlink(path);
        sdgoods_touch_flash_window_end();
        if (result != 0) return ESP_FAIL;
        --store->count;
        memmove(store->clips + i, store->clips + i + 1, (store->count - i) * sizeof(store->clips[0]));
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}
