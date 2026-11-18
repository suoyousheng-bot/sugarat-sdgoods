/* Decoder for bootanim.bin (tools/encode_bootanim.py): XOR-delta frames, raw deflate, and IMA ADPCM
 * audio. Pure C; the device inflates with the ESP32-S3 ROM, a host build (BOOTANIM_HOST) with zlib. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*bootanim_alloc_fn)(size_t bytes);
typedef void (*bootanim_free_fn)(void *p);

/** Decoder state. The file data must stay readable until bootanim_close(). */
typedef struct {
    const uint8_t *data;
    size_t size;
    int width, height, fps, frame_count, audio_rate;
    uint32_t audio_samples, audio_offset, audio_bytes, table_offset;
    int next;                   /**< Index of the next frame bootanim_decode_next() applies. */
    uint16_t *frame;            /**< Current frame, byte-swapped RGB565, width * height. */
    uint16_t *delta;            /**< Inflate output, up to width * height. */
    void *inflater;
    bootanim_free_fn free_fn;
} bootanim_t;

/** Validates the header and frame table and allocates the frame, the delta buffer (large) and the
 *  inflater state (small, about 11 KB). Returns false on a malformed file or a failed allocation,
 *  after freeing whatever was allocated. */
bool bootanim_open(bootanim_t *b, const uint8_t *data, size_t size, bootanim_alloc_fn alloc_small,
                   bootanim_alloc_fn alloc_large, bootanim_free_fn free_fn);

/** Applies the next frame to b->frame and grows [*x0,*x1) x [*y0,*y1) by its changed region; pass
 *  an empty rectangle (x1 <= x0) to start a union. Returns false when every frame was applied or the
 *  frame data is corrupt; b->frame is then unchanged by this call or partly updated, respectively. */
bool bootanim_decode_next(bootanim_t *b, int *x0, int *y0, int *x1, int *y1);

/** Frees the buffers; b->frame becomes NULL. Safe to call twice. */
void bootanim_close(bootanim_t *b);

/** Streaming IMA ADPCM state for the file's audio. Holds only offsets into the file data. */
typedef struct {
    const uint8_t *adpcm;
    uint32_t samples, pos;
    int32_t pred;
    int index;
} bootanim_audio_t;

/** Prepares audio decoding of an opened file; the state stays valid while the file data does,
 *  also after bootanim_close(). */
void bootanim_audio_init(const bootanim_t *b, bootanim_audio_t *a);

/** Decodes up to max mono samples into pcm; returns the count, 0 at the end. */
size_t bootanim_audio_read(bootanim_audio_t *a, int16_t *pcm, size_t max);

#ifdef __cplusplus
}
#endif
