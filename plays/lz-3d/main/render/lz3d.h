/* Strip-based software renderer for the baked lz model on a byte-swapped RGB565 panel.
 * Orthographic projection makes texture coordinates and depth linear in screen space, so every
 * pixel steps three fixed-point values with no division. Pure C: no ESP-IDF dependency. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lz_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Placement and appearance that stay fixed across frames. */
typedef struct {
    int screen_w, screen_h;
    float pivot_x, pivot_y;     /**< Screen position of the model centre. */
    float scale;                /**< Pixels per model unit. */
    int clip_y1;                /**< Rows at or below this stay untouched (reserved for LVGL). */
    uint16_t bg;                /**< Byte-swapped RGB565 background. */
    const uint16_t *texture;    /**< Byte-swapped RGB565, model tex_size squared; may be a RAM copy. */
    bool cull;                  /**< Skip back faces of triangles not flagged two-sided. */
} lz3d_config_t;

/** Per-triangle screen setup; vertices sorted top to bottom. */
typedef struct {
    float x0, y0, x1, y1;       /**< Top and middle vertex. */
    float s02, s01, s12;        /**< dx/dy of the long edge and both short edges. */
    float u_o, v_o, z_o;        /**< Attribute values extrapolated to screen (0,0), texels / depth. */
    float dudx, dudy, dvdx, dvdy, dzdx, dzdy;
    int32_t du, dv, dz;         /**< 16.16 per-pixel steps along x. */
    int16_t ys, ye;             /**< Covered rows [ys, ye). */
} lz3d_tri_t;

/** Renderer state; allocate with lz3d_init(). */
typedef struct {
    lz3d_config_t cfg;
    const lz_model_t *model;
    int tex_shift;
    float *pose;                /**< pos_count * 3 model positions for this frame. */
    float *sx, *sy, *sz;        /**< Projected positions. */
    lz3d_tri_t *tris;           /**< Visible triangles of this frame. */
    int tri_visible;
    uint16_t *bin;              /**< Triangle indexes grouped by strip. */
    int bin_cap;
    uint16_t *bin_start;        /**< strip_count + 1 offsets into bin. */
    int strip_cap;
    int strip_rows, strip_y0, strip_count;
    int x0, y0, x1, y1;         /**< Screen bounds of this frame, [x0,x1) x [y0,y1). */
} lz3d_t;

/** Memory allocator for frame buffers. Returns NULL on failure. */
typedef void *(*lz3d_alloc_fn)(unsigned bytes);

/** Allocates per-frame buffers for model and strips of strip_rows rows. alloc_small serves the
 *  small, hot arrays (about 23 KB for 800 triangles); alloc_large serves the triangle setups
 *  (80 bytes per model triangle). Returns false when an allocation failed; the state is then
 *  unusable. */
bool lz3d_init(lz3d_t *r, const lz_model_t *model, const lz3d_config_t *cfg, int strip_rows,
               lz3d_alloc_fn alloc_small, lz3d_alloc_fn alloc_large);

/** Writes the pose at time t_s of animation anim into r->pose; looping animations wrap, others hold
 *  their last frame. Frames are interpolated linearly. */
void lz3d_pose(lz3d_t *r, int anim, float t_s);

/** Blends r->pose toward from: pose = pose + (from - pose) * w, with w in [0,1]. */
void lz3d_pose_blend(lz3d_t *r, const float *from, float w);

/** Projects the current pose with yaw (about the vertical axis) and pitch, sets up visible
 *  triangles and computes the frame bounds r->x0..y1. */
void lz3d_prepare(lz3d_t *r, float yaw, float pitch);

/** Groups visible triangles into strips of strip_rows rows starting at row y0 and ending before
 *  y1. Call after lz3d_prepare(). Returns false when a triangle list would exceed the bin
 *  capacity; the frame must then not be rendered. */
bool lz3d_bin(lz3d_t *r, int y0, int y1);

/** Renders columns [cx0,cx1) of rows [y, y+rows) into out, where screen pixel (x,y') is
 *  out[(y'-y)*stride + x - out_x]. zbuf holds rows * stride entries laid out like out. Disjoint
 *  column ranges of one strip may be rendered concurrently. */
void lz3d_render(const lz3d_t *r, uint16_t *out, uint16_t *zbuf, int out_x, int stride, int y, int rows, int cx0, int cx1);

#ifdef __cplusplus
}
#endif
