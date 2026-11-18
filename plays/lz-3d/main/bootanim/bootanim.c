#include "bootanim.h"

#include <string.h>

#ifdef BOOTANIM_HOST
#include <zlib.h>
#else
#include "miniz.h"   /* ESP32-S3 ROM inflater */
#endif

#define HEADER_BYTES 40
#define ENTRY_BYTES  16
#define MAX_SIDE     1024

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }

typedef struct { uint32_t offset, size; int x0, y0, x1, y1; } entry_t;

static entry_t entry(const bootanim_t *b, int i)
{
    const uint8_t *p = b->data + b->table_offset + (size_t)i * ENTRY_BYTES;
    return (entry_t){rd32(p), rd32(p + 4), (int)rd16(p + 8), (int)rd16(p + 10), (int)rd16(p + 12), (int)rd16(p + 14)};
}

static bool range_ok(const bootanim_t *b, uint32_t offset, uint32_t bytes)
{
    return offset <= b->size && bytes <= b->size - offset;
}

bool bootanim_open(bootanim_t *b, const uint8_t *data, size_t size, bootanim_alloc_fn alloc_small,
                   bootanim_alloc_fn alloc_large, bootanim_free_fn free_fn)
{
    memset(b, 0, sizeof(*b));
    b->free_fn = free_fn;
    if (size < HEADER_BYTES || memcmp(data, "LZBA", 4) != 0 || rd16(data + 4) != 1) return false;
    b->data = data;
    b->size = size;
    b->width = (int)rd16(data + 6);
    b->height = (int)rd16(data + 8);
    b->fps = (int)rd16(data + 10);
    b->frame_count = (int)rd16(data + 12);
    b->audio_rate = (int)rd16(data + 14);
    b->audio_samples = rd32(data + 16);
    b->audio_offset = rd32(data + 20);
    b->audio_bytes = rd32(data + 24);
    b->table_offset = rd32(data + 28);
    if (rd32(data + 32) != size) return false;
    if (b->width <= 0 || b->width > MAX_SIDE || b->height <= 0 || b->height > MAX_SIDE) return false;
    if (b->fps <= 0 || b->frame_count <= 0) return false;
    if (!range_ok(b, b->table_offset, (uint32_t)b->frame_count * ENTRY_BYTES)) return false;
    if (!range_ok(b, b->audio_offset, b->audio_bytes) || b->audio_bytes < (b->audio_samples + 1) / 2) return false;
    if (b->audio_samples && b->audio_rate <= 0) return false;
    for (int i = 0; i < b->frame_count; i++) {
        entry_t e = entry(b, i);
        if (!range_ok(b, e.offset, e.size)) return false;
        if (e.x1 > e.x0 && (e.y1 <= e.y0 || e.x1 > b->width || e.y1 > b->height)) return false;
        if (e.x1 > e.x0 && e.size == 0) return false;
    }

    size_t px = (size_t)b->width * b->height;
#ifdef BOOTANIM_HOST
    b->inflater = alloc_small(sizeof(z_stream));
#else
    b->inflater = alloc_small(sizeof(tinfl_decompressor));
#endif
    b->frame = alloc_large(px * 2);
    b->delta = alloc_large(px * 2);
    if (!b->inflater || !b->frame || !b->delta) {
        bootanim_close(b);
        return false;
    }
    memset(b->frame, 0, px * 2);   /* frame 0 is stored as XOR against black */
    return true;
}

static bool inflate_raw(bootanim_t *b, const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len)
{
#ifdef BOOTANIM_HOST
    z_stream *z = b->inflater;
    memset(z, 0, sizeof(*z));
    if (inflateInit2(z, -15) != Z_OK) return false;
    z->next_in = (Bytef *)src;
    z->avail_in = (uInt)src_len;
    z->next_out = dst;
    z->avail_out = (uInt)dst_len;
    int rc = inflate(z, Z_FINISH);
    bool ok = rc == Z_STREAM_END && z->avail_out == 0;
    inflateEnd(z);
    return ok;
#else
    tinfl_decompressor *d = b->inflater;
    tinfl_init(d);
    size_t in = src_len, out = dst_len;
    tinfl_status st = tinfl_decompress(d, src, &in, dst, dst, &out, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    return st == TINFL_STATUS_DONE && out == dst_len;
#endif
}

bool bootanim_decode_next(bootanim_t *b, int *x0, int *y0, int *x1, int *y1)
{
    if (!b->frame || b->next >= b->frame_count) return false;
    entry_t e = entry(b, b->next);
    if (e.x1 > e.x0) {
        int w = e.x1 - e.x0, h = e.y1 - e.y0;
        if (!inflate_raw(b, b->data + e.offset, e.size, (uint8_t *)b->delta, (size_t)w * h * 2)) return false;
        const uint16_t *d = b->delta;
        for (int y = e.y0; y < e.y1; y++) {
            uint16_t *row = b->frame + (size_t)y * b->width + e.x0;
            for (int x = 0; x < w; x++) row[x] ^= *d++;
        }
        if (*x1 <= *x0) {
            *x0 = e.x0; *y0 = e.y0; *x1 = e.x1; *y1 = e.y1;
        } else {
            if (e.x0 < *x0) *x0 = e.x0;
            if (e.y0 < *y0) *y0 = e.y0;
            if (e.x1 > *x1) *x1 = e.x1;
            if (e.y1 > *y1) *y1 = e.y1;
        }
    }
    b->next++;
    return true;
}

void bootanim_close(bootanim_t *b)
{
    if (b->free_fn) {
        b->free_fn(b->inflater);
        b->free_fn(b->frame);
        b->free_fn(b->delta);
    }
    b->inflater = NULL;
    b->frame = NULL;
    b->delta = NULL;
}

static const int16_t k_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
    66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
    408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
    1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132,
    7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767};
static const int8_t k_index_adj[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

void bootanim_audio_init(const bootanim_t *b, bootanim_audio_t *a)
{
    memset(a, 0, sizeof(*a));
    a->adpcm = b->data + b->audio_offset;
    a->samples = b->audio_samples;
}

size_t bootanim_audio_read(bootanim_audio_t *a, int16_t *pcm, size_t max)
{
    size_t n = 0;
    while (n < max && a->pos < a->samples) {
        int code = (a->adpcm[a->pos >> 1] >> ((a->pos & 1) * 4)) & 15;
        int step = k_steps[a->index];
        int delta = step >> 3;
        if (code & 4) delta += step;
        if (code & 2) delta += step >> 1;
        if (code & 1) delta += step >> 2;
        a->pred += (code & 8) ? -delta : delta;
        if (a->pred > 32767) a->pred = 32767;
        if (a->pred < -32768) a->pred = -32768;
        a->index += k_index_adj[code & 7];
        if (a->index < 0) a->index = 0;
        if (a->index > 88) a->index = 88;
        pcm[n++] = (int16_t)a->pred;
        a->pos++;
    }
    return n;
}
