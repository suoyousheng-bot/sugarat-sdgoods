/* Direct-to-panel animation for the SDGOODS round badge: a two-core work queue, double-buffered
 * internal-RAM strips sent with sdgoods_lcd_draw_bitmap_safe(), and a parallel sugarat_rotate
 * renderer. Pacing and statistics live in sugarat_frame_pacing.h. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "sugarat_frame_pacing.h"
#include "sugarat_rotate.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Helper task for sugarat_frame_parallel(). */
typedef struct { int core;UBaseType_t priority;uint32_t stack_bytes; } sugarat_frame_workers_config_t;

/** Core 1, priority 5 (below the strip sender), 4096-byte stack. */
sugarat_frame_workers_config_t sugarat_frame_workers_default_config(void);

/** Starts the helper task once. ESP_ERR_INVALID_STATE when already started. */
esp_err_t sugarat_frame_workers_start(const sugarat_frame_workers_config_t *config);

typedef void (*sugarat_frame_job_fn)(int index,void *arg);

/** Calls fn(i,arg) for every i in 0..count-1 on the calling task and the helper, each claiming the
 *  next index until none remain, and returns after all calls finished. Without a started helper it
 *  runs every index on the caller. Call from one task at a time, on the other core than the helper.
 *  Calls for different indexes may run concurrently and must write disjoint memory. */
void sugarat_frame_parallel(int count,sugarat_frame_job_fn fn,void *arg);

/** Renders rows y0..y1-1 and columns lo..hi of a rotation job on both cores. A range of at most
 *  SUGARAT_ROTATE_TILE rows is split by columns with the row geometry computed once. */
void sugarat_frame_rotate(const sugarat_rotate_job_t *job,int y0,int y1,int lo,int hi);

/** Strip buffers and their sender. */
typedef struct {
    int max_width;          /**< Widest region in pixels. */
    int rows;               /**< Rows per strip. */
    int sender_core;
    UBaseType_t sender_priority;
    uint32_t timeout_ms;    /**< Wait for a strip to free before the stream gives up. */
} sugarat_frame_stream_config_t;

/** 360 pixels, 12 rows (8640 bytes per strip), sender on core 1 at priority 7, 3000 ms timeout. */
sugarat_frame_stream_config_t sugarat_frame_stream_default_config(void);

typedef struct sugarat_frame_stream sugarat_frame_stream_t;

/** Allocates two internal DMA strips and starts the sender task. With memory for only one strip the
 *  stream works without render/transfer overlap. ESP_ERR_NO_MEM when no strip fits. */
esp_err_t sugarat_frame_stream_create(const sugarat_frame_stream_config_t *config,sugarat_frame_stream_t **out);

/** Number of strips: 2 overlaps rendering with transfer, 1 does not. */
int sugarat_frame_stream_buffers(const sugarat_frame_stream_t *stream);

/** Fills rows x (x2-x1) pixels of screen rows y..y+rows-1, columns x1..x2-1, at
 *  pixels[r*(x2-x1)+c] in byte-swapped RGB565. */
typedef void (*sugarat_frame_strip_fn)(uint16_t *pixels,int x1,int y,int x2,int rows,void *arg);

/** Sends screen region [x1,x2) x [y1,y2) strip by strip: fill renders the next strip while the
 *  sender transfers the previous one. Returns after every strip reached the panel.
 *  ESP_ERR_INVALID_ARG for an empty region or one wider than max_width; the panel error of a failed
 *  strip (later strips are not sent); ESP_ERR_TIMEOUT when a strip did not free in time, after which
 *  the stream is unusable and returns ESP_ERR_INVALID_STATE. Call from one task at a time. */
esp_err_t sugarat_frame_stream_send(sugarat_frame_stream_t *stream,int x1,int y1,int x2,int y2,sugarat_frame_strip_fn fill,void *arg);

/** Returns and clears the time the sender spent in panel transfers since the last call. Call
 *  between sugarat_frame_stream_send() calls, from the task that sends. */
int64_t sugarat_frame_stream_take_send_us(sugarat_frame_stream_t *stream);

/** True when no LVGL object sits on the top or system layer and the app-shell control center is
 *  closed. Call on the LVGL task. Pressed touch and the shell's pause callbacks remain the caller's
 *  conditions; see README.md. */
bool sugarat_frame_lvgl_layers_clear(void);

#ifdef __cplusplus
}
#endif
