#include "lz3d.h"

#include <math.h>
#include <string.h>

#ifndef LZ3D_HOT
#define LZ3D_HOT
#endif

/* Depth range: 15 bits, so 16.16 steps stay inside int32. Smaller is nearer. */
#define LZ3D_DEPTH 32000.0f

/* 16.16 conversion clamped to +-2^30. Sliver triangles have huge gradients; clamping keeps the
 * arithmetic defined, and such triangles cover at most a few pixels. */
/* ceil for values already clamped to the screen range; avoids a libm call in the span loop. */
static inline int iceil(float f)
{
    int i = (int)f;
    return i + (f > (float)i);
}

static inline int32_t fix16(float v)
{
    v *= 65536.0f;
    if (v > 1073741824.0f) return 1073741824;
    if (v < -1073741824.0f) return -1073741824;
    return (int32_t)v;
}

bool lz3d_init(lz3d_t *r, const lz_model_t *m, const lz3d_config_t *cfg, int strip_rows,
               lz3d_alloc_fn alloc, lz3d_alloc_fn alloc_large)
{
    memset(r, 0, sizeof(*r));
    r->cfg = *cfg;
    r->model = m;
    r->strip_rows = strip_rows;
    while ((1 << r->tex_shift) < m->tex_size) r->tex_shift++;
    r->strip_cap = (cfg->screen_h + strip_rows - 1) / strip_rows;
    /* A visible triangle is listed once per strip it touches; four strips covers triangles up to
     * 3 * strip_rows + 1 rows tall on average, far above this model's size on screen. */
    r->bin_cap = m->tri_count * 4;
    r->pose = alloc(sizeof(float) * 3 * m->pos_count);
    r->sx = alloc(sizeof(float) * m->pos_count);
    r->sy = alloc(sizeof(float) * m->pos_count);
    r->sz = alloc(sizeof(float) * m->pos_count);
    r->tris = alloc_large(sizeof(lz3d_tri_t) * m->tri_count);
    r->bin = alloc(sizeof(uint16_t) * r->bin_cap);
    r->bin_start = alloc(sizeof(uint16_t) * (r->strip_cap + 1));
    return r->pose && r->sx && r->sy && r->sz && r->tris && r->bin && r->bin_start;
}

static void pose_frame(const lz_model_t *m, const lz_anim_t *a, int f, int g, float w, float *out)
{
    const lz_pos_t *p = a->pos + (unsigned)f * m->pos_count, *q = a->pos + (unsigned)g * m->pos_count;
    const float s = m->pos_scale;
    for (int i = 0; i < m->pos_count; i++) {
        float x = p[i].x + (q[i].x - p[i].x) * w;
        float y = p[i].y + (q[i].y - p[i].y) * w;
        float z = p[i].z + (q[i].z - p[i].z) * w;
        out[i * 3 + 0] = x * s + m->pos_offset[0];
        out[i * 3 + 1] = y * s + m->pos_offset[1];
        out[i * 3 + 2] = z * s + m->pos_offset[2];
    }
}

void lz3d_pose(lz3d_t *r, int anim, float t_s)
{
    const lz_anim_t *a = &r->model->anims[anim];
    float ft = t_s * a->fps;
    if (ft < 0) ft = 0;
    int f, g;
    float w;
    if (a->loop) {
        ft = fmodf(ft, (float)a->frames);
        f = (int)ft;
        w = ft - f;
        g = (f + 1) % a->frames;
    } else if (ft >= a->frames - 1) {
        f = g = a->frames - 1;
        w = 0;
    } else {
        f = (int)ft;
        w = ft - f;
        g = f + 1;
    }
    pose_frame(r->model, a, f, g, w, r->pose);
}

void lz3d_pose_blend(lz3d_t *r, const float *from, float w)
{
    int n = r->model->pos_count * 3;
    for (int i = 0; i < n; i++) r->pose[i] += (from[i] - r->pose[i]) * w;
}

static void setup_tri(lz3d_tri_t *t, const float *x, const float *y, const float *z, const float *u, const float *v)
{
    /* Attribute planes from the unsorted vertices. */
    float ex1 = x[1] - x[0], ey1 = y[1] - y[0], ex2 = x[2] - x[0], ey2 = y[2] - y[0];
    float inv = 1.0f / (ex1 * ey2 - ex2 * ey1);
#define PLANE(a, ddx, ddy, org) do { \
        float a1 = a[1] - a[0], a2 = a[2] - a[0]; \
        ddx = (a1 * ey2 - a2 * ey1) * inv; ddy = (a2 * ex1 - a1 * ex2) * inv; \
        org = a[0] - ddx * x[0] - ddy * y[0]; } while (0)
    PLANE(u, t->dudx, t->dudy, t->u_o);
    PLANE(v, t->dvdx, t->dvdy, t->v_o);
    PLANE(z, t->dzdx, t->dzdy, t->z_o);
#undef PLANE
    t->du = fix16(t->dudx);
    t->dv = fix16(t->dvdx);
    t->dz = fix16(t->dzdx);
    /* Sort vertices by y for span edges. */
    int a = 0, b = 1, c = 2, s;
    if (y[a] > y[b]) { s = a; a = b; b = s; }
    if (y[b] > y[c]) { s = b; b = c; c = s; }
    if (y[a] > y[b]) { s = a; a = b; b = s; }
    t->x0 = x[a]; t->y0 = y[a]; t->x1 = x[b]; t->y1 = y[b];
    float h02 = y[c] - y[a], h01 = y[b] - y[a], h12 = y[c] - y[b];
    t->s02 = h02 > 0 ? (x[c] - x[a]) / h02 : 0;
    t->s01 = h01 > 0 ? (x[b] - x[a]) / h01 : 0;
    t->s12 = h12 > 0 ? (x[c] - x[b]) / h12 : 0;
    t->ys = (int16_t)ceilf(y[a] - 0.5f);
    t->ye = (int16_t)ceilf(y[c] - 0.5f);
}

void lz3d_prepare(lz3d_t *r, float yaw, float pitch)
{
    const lz_model_t *m = r->model;
    const lz3d_config_t *c = &r->cfg;
    const float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    const float k = c->scale, zk = LZ3D_DEPTH * 0.5f / m->radius;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i < m->pos_count; i++) {
        float dx = r->pose[i * 3] - m->center[0], dy = r->pose[i * 3 + 1] - m->center[1], dz = r->pose[i * 3 + 2] - m->center[2];
        float x1 = dx * cy - dy * sy, y1 = dx * sy + dy * cy;
        float y2 = y1 * cp - dz * sp, z2 = y1 * sp + dz * cp;
        float px = c->pivot_x + x1 * k, py = c->pivot_y - z2 * k;
        r->sx[i] = px; r->sy[i] = py;
        r->sz[i] = LZ3D_DEPTH * 0.5f + y2 * zk;
        if (px < minx) minx = px;
        if (px > maxx) maxx = px;
        if (py < miny) miny = py;
        if (py > maxy) maxy = py;
    }
    const float ts = (float)m->tex_size / 65536.0f;
    int n = 0;
    for (int i = 0; i < m->tri_count; i++) {
        const uint16_t *tv = m->tris + i * 3;
        float x[3], y[3], z[3], u[3], v[3];
        for (int j = 0; j < 3; j++) {
            int p = m->vert_pos[tv[j]];
            x[j] = r->sx[p]; y[j] = r->sy[p]; z[j] = r->sz[p];
            u[j] = m->vert_uv[tv[j] * 2] * ts;
            v[j] = m->vert_uv[tv[j] * 2 + 1] * ts;
        }
        float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        if (fabsf(area) < 1e-3f) continue;
        /* Counter-clockwise front faces become negative area once screen y points down. */
        if (c->cull && area > 0 && !(m->tri_flags[i] & LZ_TRI_TWO_SIDED)) continue;
        lz3d_tri_t *t = &r->tris[n];
        setup_tri(t, x, y, z, u, v);
        if (t->ye <= t->ys) continue;
        n++;
    }
    r->tri_visible = n;
    int x0 = (int)floorf(minx) - 1, x1 = (int)ceilf(maxx) + 2, y0 = (int)floorf(miny) - 1, y1 = (int)ceilf(maxy) + 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->screen_w) x1 = c->screen_w;
    if (y1 > c->clip_y1) y1 = c->clip_y1;
    r->x0 = x0; r->y0 = y0; r->x1 = x1; r->y1 = y1;
}

bool lz3d_bin(lz3d_t *r, int y0, int y1)
{
    const int rows = r->strip_rows;
    int strips = (y1 - y0 + rows - 1) / rows;
    if (strips > r->strip_cap) strips = r->strip_cap;
    if (strips < 0) strips = 0;
    r->strip_y0 = y0;
    r->strip_count = strips;
    uint16_t *start = r->bin_start;
    memset(start, 0, sizeof(uint16_t) * (strips + 1));
    int total = 0;
    for (int i = 0; i < r->tri_visible; i++) {
        const lz3d_tri_t *t = &r->tris[i];
        int a = (t->ys - y0) / rows, b = (t->ye - 1 - y0) / rows;
        if (t->ys < y0) a = 0;
        if (t->ye - 1 < y0) continue;
        if (b >= strips) b = strips - 1;
        for (int s = a; s <= b; s++) start[s + 1]++;
        total += b >= a ? b - a + 1 : 0;
    }
    if (total > r->bin_cap) return false;
    for (int s = 0; s < strips; s++) start[s + 1] += start[s];
    /* Second pass fills each strip's slice; bin_start[s] advances and is restored afterwards. */
    for (int i = 0; i < r->tri_visible; i++) {
        const lz3d_tri_t *t = &r->tris[i];
        int a = (t->ys - y0) / rows, b = (t->ye - 1 - y0) / rows;
        if (t->ys < y0) a = 0;
        if (t->ye - 1 < y0) continue;
        if (b >= strips) b = strips - 1;
        for (int s = a; s <= b; s++) r->bin[start[s]++] = (uint16_t)i;
    }
    for (int s = strips; s > 0; s--) start[s] = start[s - 1];
    start[0] = 0;
    return true;
}

/* Unsigned steps wrap like the texture does; depth stays in range inside the triangle. */
static LZ3D_HOT void span(uint16_t *o, uint16_t *zp, int n, uint32_t u, uint32_t v, uint32_t z,
                          uint32_t du, uint32_t dv, uint32_t dz, const uint16_t *tex, int shift)
{
    const uint32_t mask = (1u << shift) - 1;
    const int vs = 16 - shift;
    const uint32_t vmask = mask << shift;
    for (; n > 0; n--, o++, zp++, u += du, v += dv, z += dz) {
        uint16_t d = (uint16_t)(z >> 16);
        if (d < *zp) {
            *zp = d;
            *o = tex[((v >> vs) & vmask) | ((u >> 16) & mask)];
        }
    }
}

LZ3D_HOT void lz3d_render(const lz3d_t *r, uint16_t *out, uint16_t *zbuf, int out_x, int stride, int y, int rows, int cx0, int cx1)
{
    const uint16_t bg = r->cfg.bg;
    for (int i = 0; i < rows; i++) {
        uint16_t *o = out + i * stride + cx0 - out_x, *zp = zbuf + i * stride + cx0 - out_x;
        for (int x = cx0; x < cx1; x++) { *o++ = bg; *zp++ = 0xffff; }
    }
    int s = (y - r->strip_y0) / r->strip_rows;
    if (s < 0 || s >= r->strip_count) return;
    const uint16_t *tex = r->cfg.texture;
    const int shift = r->tex_shift;
    for (int k = r->bin_start[s]; k < r->bin_start[s + 1]; k++) {
        const lz3d_tri_t *t = &r->tris[r->bin[k]];
        int ra = t->ys > y ? t->ys : y, rb = t->ye < y + rows ? t->ye : y + rows;
        for (int row = ra; row < rb; row++) {
            float yc = row + 0.5f;
            float xa = t->x0 + (yc - t->y0) * t->s02;
            float xb = yc < t->y1 ? t->x0 + (yc - t->y0) * t->s01 : t->x1 + (yc - t->y1) * t->s12;
            if (xa > xb) { float q = xa; xa = xb; xb = q; }
            /* Near-horizontal edges of slivers extrapolate far; keep the conversion defined. */
            if (xa < cx0 - 1) xa = cx0 - 1;
            if (xb > cx1 + 1) xb = cx1 + 1;
            if (xb <= xa) continue;
            int xs = iceil(xa - 0.5f), xe = iceil(xb - 0.5f);
            if (xs < cx0) xs = cx0;
            if (xe > cx1) xe = cx1;
            if (xe <= xs) continue;
            float xc = xs + 0.5f;
            int32_t u = fix16(t->u_o + t->dudx * xc + t->dudy * yc);
            int32_t v = fix16(t->v_o + t->dvdx * xc + t->dvdy * yc);
            int32_t z = fix16(t->z_o + t->dzdx * xc + t->dzdy * yc);
            int off = (row - y) * stride + xs - out_x;
            span(out + off, zbuf + off, xe - xs, (uint32_t)u, (uint32_t)v, (uint32_t)z, (uint32_t)t->du, (uint32_t)t->dv, (uint32_t)t->dz, tex, shift);
        }
    }
}
