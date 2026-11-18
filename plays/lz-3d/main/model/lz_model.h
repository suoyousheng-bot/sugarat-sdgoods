/* Exported lz character: decimated mesh, baked per-frame vertex animation and an RGB565 texture.
 * Generated data lives in lz_model_data.c (tools/export_model.py). Pure C: no ESP-IDF dependency. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Quantized position; world = q * pos_scale + pos_offset (Blender axes, z up, front faces -y). */
typedef struct { int16_t x, y, z; } lz_pos_t;

/** One baked animation: frames x pos_count positions, sampled at fps. */
typedef struct {
    const char *name;
    uint16_t frames;
    float fps;
    bool loop;
    const lz_pos_t *pos;
} lz_anim_t;

typedef struct {
    uint16_t pos_count, vert_count, tri_count;
    const uint16_t *vert_pos;   /**< Render vertex -> position index. */
    const uint16_t *vert_uv;    /**< Two per render vertex; 0..65535 maps to [0,1), v=0 is the top texture row. */
    const uint16_t *tris;       /**< Three render-vertex indices per triangle, counter-clockwise from the front. */
    const uint8_t *tri_flags;   /**< Bit 0: two-sided (never back-face culled). */
    float pos_scale;
    float pos_offset[3];
    float center[3];            /**< Rotation pivot. */
    float radius;               /**< Largest distance of any animated position from center. */
    const uint16_t *texture;    /**< tex_size x tex_size byte-swapped RGB565, rows top first. */
    uint16_t tex_size;          /**< Power of two. */
    const lz_anim_t *anims;
    uint8_t anim_count;
} lz_model_t;

#define LZ_TRI_TWO_SIDED 1u

extern const lz_model_t lz_model;

#ifdef __cplusplus
}
#endif
