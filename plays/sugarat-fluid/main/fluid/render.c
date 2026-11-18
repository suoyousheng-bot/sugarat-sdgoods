/* SPDX-License-Identifier: MIT */
/** Perspective particle rasterizer, bottle glass, fill gauge and pour effects; no display or LVGL calls. */
#include "render.h"
#include "config.h"
#include "sim.h"
#include "esp_attr.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DISC_MAX 10
#define SPEEDS 32
#define DEPTHS 16
#define FX_MAX 160
#define GEOMETRY_MAX 7200
#define OCC 36
#define OCC_CELL 10
#define GAUGE_DEG 55.0f
#define MOUTH_HALF 20
/* On-screen fall acceleration matching the solver's slowed time, in px/s^2 per g. */
#define SCREEN_G (9.81f * PX_PER_METER * GRAVITY_GAIN * TIME_SCALE * TIME_SCALE)
#define STREAM_SPEED 380.0f

const uint8_t fluid_theme_rgb[FLUID_THEMES][3] = {{30, 160, 245}, {155, 65, 240}, {245, 130, 25}, {30, 205, 130}};

typedef enum { GEO_GLASS, GEO_GLEAM, GEO_TRACK, GEO_TICK } geometry_kind_t;
/* Packed: pixel index in bits 0-16, gauge position 0..1000 in bits 17-26, kind in bits 27-28. */
typedef uint32_t geometry_t;
/* Frames rotate between a few buffers; each remembers the row spans it last drew. */
#define DIRTY_SLOTS 4
typedef struct { const uint16_t *buffer; uint32_t used; int16_t x0[360], x1[360]; } dirty_t;
typedef enum { FX_NONE, FX_STREAM, FX_SPLASH, FX_BUBBLE } fx_kind_t;
typedef struct {
    float x, y, vx, vy, r0;
    uint16_t age, life, serial, ahead_serial;
    int16_t ahead; /* Stream drop emitted just before this one, or -1. */
    uint8_t kind, rgb[3];
} fx_t;

static EXT_RAM_BSS_ATTR struct {
    uint8_t span[DISC_MAX + 1][DISC_MAX * 2 + 1];
    float speed_lit[SPEEDS], speed_white[SPEEDS], dim[DEPTHS];
    sim_particle_view_t particles[PARTICLE_MAX];
    geometry_t geometry[GEOMETRY_MAX];
    unsigned geometry_count;
    uint8_t occupied[OCC * OCC];
    fx_t fx[FX_MAX];
    float emit_credit, bubble_credit;
    int last_drop;
    uint16_t serial;
    uint32_t last_ms, value_until_ms, rng;
    unsigned last_total;
    bool started;
    dirty_t dirty[DIRTY_SLOTS];
    int16_t x0[360], x1[360];
    uint32_t frame;
} renderer;

/* Record a drawn run so the next use of this buffer clears only what changed. */
static inline void mark(int y, int x0, int x1)
{
    if (x0 < renderer.x0[y]) renderer.x0[y] = (int16_t)x0;
    if (x1 > renderer.x1[y]) renderer.x1[y] = (int16_t)x1;
}

/* 5x7 glyphs for 0-9 and '%', one byte per row, low five bits used. */
static const uint8_t glyphs[11][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14}, {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30}, {2, 6, 10, 18, 31, 2, 2}, {31, 16, 30, 1, 1, 17, 14},
    {6, 8, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8}, {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 2, 12}, {25, 26, 2, 4, 8, 11, 19},
};

static uint16_t rgb(int r, int g, int b)
{
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    uint16_t c = (uint16_t)(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

static void blend(uint16_t *pixel, int r, int g, int b, int alpha)
{
    uint16_t c = (uint16_t)((*pixel >> 8) | (*pixel << 8));
    int pr = (c >> 11) << 3, pg = ((c >> 5) & 63) << 2, pb = (c & 31) << 3;
    *pixel = rgb(pr + (r - pr) * alpha / 255, pg + (g - pg) * alpha / 255, pb + (b - pb) * alpha / 255);
}

static float unit_random(void)
{
    renderer.rng ^= renderer.rng << 13;
    renderer.rng ^= renderer.rng >> 17;
    renderer.rng ^= renderer.rng << 5;
    return (float)(renderer.rng & 0xFFFF) / 65535.0f;
}

static void add_geometry(int x, int y, float t, geometry_kind_t kind)
{
    if (renderer.geometry_count >= GEOMETRY_MAX) return;
    renderer.geometry[renderer.geometry_count++] = (uint32_t)(y * 360 + x) | (uint32_t)(t * 1000) << 17 | (uint32_t)kind << 27;
}

void fluid_render_init(void)
{
    memset(&renderer, 0, sizeof(renderer));
    renderer.rng = 0x9E3779B9u;
    renderer.last_drop = -1;
    for (int r = 0; r <= DISC_MAX; r++)
        for (int y = -r; y <= r; y++)
            renderer.span[r][y + r] = (uint8_t)sqrtf((float)(r * r - y * y));
    for (int s = 0; s < SPEEDS; s++) {
        float speed = powf((float)s / (SPEEDS - 1), 0.55f);
        renderer.speed_lit[s] = 0.55f + 0.45f * speed;
        renderer.speed_white[s] = speed * 0.7f;
    }
    for (int d = 0; d < DEPTHS; d++) renderer.dim[d] = 1.0f - 0.45f * d / (DEPTHS - 1);
    /* Glass first so the gauge draws over it; the mouth gap stays open. */
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < 360; y++)
            for (int x = 0; x < 360; x++) {
                float dx = x - 179.5f, dy = y - 179.5f, r = sqrtf(dx * dx + dy * dy);
                if (r < 159.5f || r >= 176.0f) continue;
                float angle = atan2f(dy, dx) * 57.29578f;
                if (pass == 0 && r >= 170.5f && r < 173.0f && !(fabsf(dx) < MOUTH_HALF && dy < 0) && fabsf(angle) > GAUGE_DEG + 0.6f)
                    add_geometry(x, y, 0, angle > -170 && angle < -110 ? GEO_GLEAM : GEO_GLASS);
                if (pass == 1 && fabsf(angle) <= GAUGE_DEG + 0.6f) {
                    float t = (GAUGE_DEG - angle) / (2 * GAUGE_DEG);
                    t = t < 0 ? 0 : t > 1 ? 1 : t;
                    if (r >= 168.5f && r < 175.5f) add_geometry(x, y, t, GEO_TRACK);
                    else if (r >= 160.0f && r < 168.5f) {
                        float tick = roundf(t * 4) / 4;
                        if (fabsf(t - tick) * 2 * GAUGE_DEG * 0.01745f * r < 1.1f) add_geometry(x, y, tick, GEO_TICK);
                    }
                }
            }
}

void fluid_liquid_rgb(const bottle_liquid_t *liquid, uint8_t out[3])
{
    unsigned total = bottle_liquid_total(liquid);
    for (unsigned a = 0; a < 3; a++) {
        unsigned sum = 0;
        for (unsigned t = 0; t < FLUID_THEMES; t++) sum += fluid_theme_rgb[t][a] * liquid->part[t];
        out[a] = total ? (uint8_t)(sum / total) : fluid_theme_rgb[0][a];
    }
}

static void disc(uint16_t *pixels, int cx, int cy, int r, uint16_t color)
{
    if (r > DISC_MAX) r = DISC_MAX;
    if (r < 0) return;
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= 360) continue;
        int half = renderer.span[r][dy + r];
        int x0 = cx - half, x1 = cx + half;
        if (x0 < 0) x0 = 0;
        if (x1 > 359) x1 = 359;
        if (x0 > x1) continue;
        mark(y, x0, x1);
        for (int x = x0; x <= x1; x++) pixels[y * 360 + x] = color;
    }
}

static void ring(uint16_t *pixels, int cx, int cy, int r, const uint8_t c[3], int alpha)
{
    if (r > DISC_MAX) r = DISC_MAX;
    if (r < 2) return;
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= 360) continue;
        int outer = renderer.span[r][dy + r];
        int inner = dy > -r + 1 && dy < r - 1 ? renderer.span[r - 1][dy + r - 1] - 1 : -1;
        mark(y, cx - outer < 0 ? 0 : cx - outer, cx + outer > 359 ? 359 : cx + outer);
        for (int x = cx - outer; x <= cx + outer; x++)
            if (x >= 0 && x < 360 && abs(x - cx) > inner) blend(&pixels[y * 360 + x], c[0], c[1], c[2], alpha);
    }
}

static void shade(const uint8_t base[3], int depth, int speed, uint16_t *body, uint16_t *shine)
{
    int c[3], h[3];
    float lit = renderer.speed_lit[speed], white = renderer.speed_white[speed], dim = renderer.dim[depth];
    for (int a = 0; a < 3; a++) {
        c[a] = (int)((base[a] * lit + (255 - base[a]) * white) * dim);
        h[a] = (int)((c[a] + (255 - c[a]) * 0.65f) * dim);
    }
    *body = rgb(c[0], c[1], c[2]);
    *shine = rgb(h[0], h[1], h[2]);
}

static bool occupied(float x, float y)
{
    int cx = (int)x / OCC_CELL, cy = (int)y / OCC_CELL;
    return x >= 0 && y >= 0 && cx < OCC && cy < OCC && renderer.occupied[cy * OCC + cx];
}

static void draw_particles(uint16_t *pixels)
{
    memset(renderer.occupied, 0, sizeof(renderer.occupied));
    int count = sim_snapshot(renderer.particles, PARTICLE_MAX);
    for (int i = count - 1; i >= 0; i--) {
        const sim_particle_view_t *p = &renderer.particles[i];
        float scale = PROJ_FOCAL / (PROJ_FOCAL + p->z);
        int x = (int)(180 + (p->x - 180) * scale), y = (int)(180 + (p->y - 180) * scale);
        int r = (int)(7.5f * scale + 0.5f);
        int speed = (int)(p->speed * (SPEEDS - 1) / 5000);
        int depth = (int)(p->z * (DEPTHS - 1) / BOX_D);
        speed = speed < 0 ? 0 : speed >= SPEEDS ? SPEEDS - 1 : speed;
        depth = depth < 0 ? 0 : depth >= DEPTHS ? DEPTHS - 1 : depth;
        uint16_t body, shine;
        shade(p->rgb, depth, speed, &body, &shine);
        disc(pixels, x, y, r, body);
        disc(pixels, x - r / 3, y - r / 3, r / 2, shine);
        if (x >= 0 && y >= 0 && x < 360 && y < 360) renderer.occupied[(y / OCC_CELL) * OCC + x / OCC_CELL] = 1;
    }
}

static fx_t *spawn(fx_kind_t kind)
{
    for (int i = 0; i < FX_MAX; i++)
        if (renderer.fx[i].kind == FX_NONE) {
            memset(&renderer.fx[i], 0, sizeof(renderer.fx[i]));
            renderer.fx[i].kind = (uint8_t)kind;
            renderer.fx[i].ahead = -1;
            renderer.fx[i].serial = ++renderer.serial;
            return &renderer.fx[i];
        }
    return NULL;
}

static void emit_stream(const fluid_scene_t *scene, const uint8_t color[3], float dt)
{
    float r0 = 4.0f + (float)scene->rate * 0.8f;
    float wobble = sinf(scene->time_ms * 0.006f) * 2.0f + sinf(scene->time_ms * 0.017f) * 1.0f;
    float spacing = r0 * 1.1f;
    renderer.emit_credit += STREAM_SPEED * dt / spacing;
    int n = (int)renderer.emit_credit;
    renderer.emit_credit -= (float)n;
    /* Oldest first, so each drop links to the one ahead of it. */
    for (int i = n - 1; i >= 0; i--) {
        fx_t *drop = spawn(FX_STREAM);
        if (!drop) return;
        if (renderer.last_drop >= 0) {
            drop->ahead = (int16_t)renderer.last_drop;
            drop->ahead_serial = renderer.fx[renderer.last_drop].serial;
        }
        renderer.last_drop = (int)(drop - renderer.fx);
        float lead = (float)i / (float)n * STREAM_SPEED * dt;
        drop->r0 = r0;
        drop->life = 2000;
        memcpy(drop->rgb, color, 3);
        if (scene->flow > 0) {
            /* Enters through the mouth from the other bottle above. */
            drop->x = MOUTH_X + wobble;
            drop->y = -r0 + lead;
            drop->vy = STREAM_SPEED;
        } else {
            /* Leaves through the mouth; gravity points out of the neck. */
            drop->x = MOUTH_X + wobble;
            drop->y = MOUTH_Y + 6 - lead;
            drop->vy = -STREAM_SPEED * 0.7f;
        }
    }
    if (scene->flow < 0) {
        /* Air glugs in as liquid leaves. */
        renderer.bubble_credit += dt * (4.0f + scene->rate);
        while (renderer.bubble_credit >= 1.0f) {
            renderer.bubble_credit -= 1.0f;
            fx_t *bubble = spawn(FX_BUBBLE);
            if (!bubble) break;
            bubble->x = MOUTH_X + (unit_random() - 0.5f) * 16;
            bubble->y = MOUTH_Y + 14;
            bubble->r0 = 3 + unit_random() * 3;
            bubble->vx = (unit_random() - 0.5f) * 40;
            bubble->vy = 120 + unit_random() * 60;
            bubble->life = 900;
        }
    }
}

static void splash(fluid_scene_t *scene, const fx_t *drop, float ux, float uy)
{
    float speed = sqrtf(drop->vx * drop->vx + drop->vy * drop->vy);
    scene->impact = true;
    scene->impact_x = drop->x; scene->impact_y = drop->y;
    scene->impact_vx = drop->vx; scene->impact_vy = drop->vy;
    for (int k = 0; k < 2; k++) {
        fx_t *s = spawn(FX_SPLASH);
        if (!s) return;
        float side = (unit_random() - 0.5f) * 2;
        float up = speed * (0.18f + unit_random() * 0.17f);
        s->x = drop->x; s->y = drop->y;
        s->vx = -ux * up - uy * side * 110;
        s->vy = -uy * up + ux * side * 110;
        s->r0 = 2 + unit_random() * 1.5f;
        s->life = (uint16_t)(260 + unit_random() * 160);
        memcpy(s->rgb, drop->rgb, 3);
    }
    if (unit_random() < 0.3f) {
        fx_t *b = spawn(FX_BUBBLE);
        if (!b) return;
        b->x = drop->x + ux * 10; b->y = drop->y + uy * 10;
        b->vx = -ux * 70; b->vy = -uy * 70;
        b->r0 = 2 + unit_random() * 2;
        b->life = 450;
    }
}

static void update_fx(fluid_scene_t *scene, float dt)
{
    float g = sqrtf(scene->down_x * scene->down_x + scene->down_y * scene->down_y);
    float ux = g > 0.05f ? scene->down_x / g : 0, uy = g > 0.05f ? scene->down_y / g : 1;
    float ax = scene->down_x * SCREEN_G, ay = scene->down_y * SCREEN_G;
    uint16_t step = (uint16_t)(dt * 1000);
    for (int i = 0; i < FX_MAX; i++) {
        fx_t *f = &renderer.fx[i];
        if (f->kind == FX_NONE) continue;
        f->age = (uint16_t)(f->age + step);
        if (f->kind == FX_BUBBLE) {
            /* Buoyancy: rise against gravity with a little wobble. */
            f->vx += (-ux * 260 + (unit_random() - 0.5f) * 300) * dt;
            f->vy += (-uy * 260) * dt;
        } else {
            f->vx += ax * dt;
            f->vy += ay * dt;
        }
        f->x += f->vx * dt;
        f->y += f->vy * dt;
        float dx = f->x - 179.5f, dy = f->y - 179.5f, d = sqrtf(dx * dx + dy * dy);
        bool dead = f->age > f->life || f->x < -20 || f->x > 380 || f->y < -30 || f->y > 380;
        if (f->kind == FX_STREAM && f->vy > 0 && f->y > MOUTH_Y + 12 &&
            (occupied(f->x, f->y) || d > FLUID_RADIUS - f->r0)) {
            splash(scene, f, ux, uy);
            dead = true;
        }
        if (f->kind == FX_SPLASH && d > FLUID_RADIUS) dead = true;
        if (f->kind == FX_BUBBLE && (d > FLUID_RADIUS - 4 || (f->age > 120 && !occupied(f->x, f->y)))) dead = true;
        if (dead) f->kind = FX_NONE;
    }
}

/* Continuity: a faster stream is a thinner stream. */
static float stream_radius(const fx_t *f)
{
    if (f->kind != FX_STREAM) return f->r0;
    float speed = sqrtf(f->vx * f->vx + f->vy * f->vy);
    float r = f->r0 * sqrtf(STREAM_SPEED / fmaxf(speed, STREAM_SPEED));
    return r < 2.5f ? 2.5f : r;
}

static void capsule(uint16_t *pixels, float x0, float y0, float r0, float x1, float y1, float r1, uint16_t color)
{
    float length = fmaxf(fabsf(x1 - x0), fabsf(y1 - y0));
    int steps = (int)(length / 1.5f) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        disc(pixels, (int)(x0 + (x1 - x0) * t), (int)(y0 + (y1 - y0) * t), (int)(r0 + (r1 - r0) * t + 0.5f), color);
    }
}

static void draw_fx(uint16_t *pixels, const fluid_scene_t *scene, const uint8_t source[3])
{
    static const uint8_t foam[3] = {215, 240, 255};
    for (int i = 0; i < FX_MAX; i++) {
        const fx_t *f = &renderer.fx[i];
        if (f->kind == FX_BUBBLE) ring(pixels, (int)f->x, (int)f->y, (int)f->r0, foam, 150);
    }
    if (scene->flow) {
        /* The neck stays full while liquid runs through it. */
        uint16_t body, shine;
        shade(source, 0, SPEEDS / 2, &body, &shine);
        int half = 2 + (int)scene->rate / 2;
        for (int y = 0; y < (int)MOUTH_Y + 4; y++) {
            mark(y, (int)MOUTH_X - half, (int)MOUTH_X + half);
            for (int x = (int)MOUTH_X - half; x <= (int)MOUTH_X + half; x++)
                pixels[y * 360 + x] = x == (int)MOUTH_X - half + 1 ? shine : body;
        }
    }
    for (int i = 0; i < FX_MAX; i++) {
        const fx_t *f = &renderer.fx[i];
        if (f->kind != FX_STREAM && f->kind != FX_SPLASH) continue;
        uint16_t body, shine;
        shade(f->rgb, 0, f->kind == FX_STREAM ? SPEEDS / 4 : SPEEDS / 2, &body, &shine);
        float r = stream_radius(f);
        const fx_t *ahead = f->kind == FX_STREAM && f->ahead >= 0 ? &renderer.fx[f->ahead] : NULL;
        if (ahead && (ahead->kind != FX_STREAM || ahead->serial != f->ahead_serial)) ahead = NULL;
        /* Close drops form one column; the accelerating tail breaks into droplets. */
        float speed = sqrtf(f->vx * f->vx + f->vy * f->vy);
        if (ahead && fabsf(ahead->x - f->x) + fabsf(ahead->y - f->y) < 12 + speed * 0.035f) {
            capsule(pixels, f->x, f->y, r, ahead->x, ahead->y, stream_radius(ahead), body);
            capsule(pixels, f->x - r / 3, f->y, r / 3, ahead->x - r / 3, ahead->y, stream_radius(ahead) / 3, shine);
        } else {
            disc(pixels, (int)f->x, (int)f->y, (int)(r + 0.5f), body);
            if (r >= 3) disc(pixels, (int)(f->x - r / 3), (int)f->y, (int)(r / 3), shine);
        }
    }
}

static void line(uint16_t *pixels, float x0, float y0, float x1, float y1, uint16_t color)
{
    float length = fmaxf(fabsf(x1 - x0), fabsf(y1 - y0));
    for (int i = 0; i <= (int)length; i++) {
        float t = length > 0 ? i / length : 0;
        disc(pixels, (int)(x0 + (x1 - x0) * t), (int)(y0 + (y1 - y0) * t), 2, color);
    }
}

static void draw_cue(uint16_t *pixels, fluid_cue_t cue, uint32_t time_ms)
{
    if (cue == FLUID_CUE_NONE) return;
    /* Three chevrons; a brightness wave runs into (catch) or out of (wait) the mouth. */
    float phase = (float)(time_ms % 900) / 900.0f;
    int direction = cue == FLUID_CUE_CATCH ? 1 : -1;
    for (int k = 0; k < 3; k++) {
        float wave = 0.5f + 0.5f * cosf(6.2831853f * (phase - (float)k / 3.0f));
        float brightness = 0.25f + 0.75f * wave;
        float y = direction > 0 ? 38 + k * 17 : 72 - k * 17;
        uint16_t color = cue == FLUID_CUE_CATCH
            ? rgb((int)(200 * brightness), (int)(236 * brightness), (int)(255 * brightness))
            : rgb((int)(255 * brightness), (int)(176 * brightness), (int)(56 * brightness));
        line(pixels, MOUTH_X - 13, y - direction * 6, MOUTH_X, y + direction * 6, color);
        line(pixels, MOUTH_X, y + direction * 6, MOUTH_X + 13, y - direction * 6, color);
    }
}

static void draw_glyph(uint16_t *pixels, int x, int y, int glyph, int scale, uint16_t color)
{
    for (int row = 0; row < 7; row++)
        for (int column = 0; column < 5; column++)
            if (glyphs[glyph][row] & (16 >> column))
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x + column * scale + sx, py = y + row * scale + sy;
                        if (px >= 0 && px < 360 && py >= 0 && py < 360) {
                            mark(py, px, px);
                            pixels[py * 360 + px] = color;
                        }
                    }
}

static void draw_percent(uint16_t *pixels, unsigned percent, int right, int top)
{
    int digits[4], n = 0;
    digits[n++] = -1; /* '%' is drawn last. */
    do { digits[n++] = (int)(percent % 10); percent /= 10; } while (percent && n < 4);
    const int scale = 2, advance = 6 * scale;
    int x = right - n * advance + scale;
    uint16_t halo = rgb(0, 0, 0), ink = rgb(236, 244, 252);
    for (int i = n - 1; i >= 0; i--, x += advance) {
        int glyph = digits[i] < 0 ? 10 : digits[i];
        for (int oy = -2; oy <= 2; oy += 2)
            for (int ox = -2; ox <= 2; ox += 2)
                if (ox || oy) draw_glyph(pixels, x + ox, top + oy, glyph, scale, halo);
        draw_glyph(pixels, x, top, glyph, scale, ink);
    }
}

static void draw_frame(uint16_t *pixels, const bottle_liquid_t *liquid, const uint8_t tint[3])
{
    unsigned total = bottle_liquid_total(liquid);
    unsigned fill = total * 1000 / BOTTLE_CAPACITY;
    uint16_t glass = rgb(46, 70, 92), gleam = rgb(110, 145, 170), track = rgb(34, 44, 56);
    uint16_t tick = rgb(96, 112, 128), bright = rgb(236, 246, 255);
    uint16_t level = rgb(tint[0] + (255 - tint[0]) / 4, tint[1] + (255 - tint[1]) / 4, tint[2] + (255 - tint[2]) / 4);
    uint16_t level_tick = rgb(tint[0] + (255 - tint[0]) / 2, tint[1] + (255 - tint[1]) / 2, tint[2] + (255 - tint[2]) / 2);
    for (unsigned i = 0; i < renderer.geometry_count; i++) {
        geometry_t g = renderer.geometry[i];
        unsigned t = (g >> 17) & 1023;
        uint16_t color = glass;
        switch ((geometry_kind_t)(g >> 27)) {
            case GEO_GLASS: color = glass; break;
            case GEO_GLEAM: color = gleam; break;
            case GEO_TRACK:
                color = fill && t <= fill ? (t + 18 > fill ? bright : level) : track;
                break;
            case GEO_TICK: color = fill && t <= fill ? level_tick : tick; break;
        }
        pixels[g & 0x1FFFF] = color;
    }
    /* A flared neck marks the open mouth at the top edge. */
    for (int side = -1; side <= 1; side += 2) {
        float x = MOUTH_X + side * (MOUTH_HALF + 1);
        line(pixels, x, 14, x + side * 5, 1, gleam);
        disc(pixels, (int)x, 14, 3, gleam);
    }
}

static dirty_t *slot_for(const uint16_t *pixels)
{
    dirty_t *oldest = &renderer.dirty[0];
    for (int i = 0; i < DIRTY_SLOTS; i++) {
        if (renderer.dirty[i].buffer == pixels) return &renderer.dirty[i];
        if (renderer.dirty[i].used < oldest->used) oldest = &renderer.dirty[i];
    }
    return oldest;
}

/* Glass and gauge pixels are fully repainted each frame, so only recorded runs need clearing. */
static void clear(uint16_t *pixels)
{
    dirty_t *slot = slot_for(pixels);
    if (slot->buffer != pixels) memset(pixels, 0, 360 * 360 * sizeof(*pixels));
    else for (int y = 0; y < 360; y++)
        if (slot->x0[y] <= slot->x1[y]) memset(pixels + y * 360 + slot->x0[y], 0, (size_t)(slot->x1[y] - slot->x0[y] + 1) * sizeof(*pixels));
    for (int y = 0; y < 360; y++) { renderer.x0[y] = 360; renderer.x1[y] = -1; }
}

static void remember(const uint16_t *pixels)
{
    dirty_t *slot = slot_for(pixels);
    slot->buffer = pixels;
    slot->used = ++renderer.frame;
    memcpy(slot->x0, renderer.x0, sizeof(slot->x0));
    memcpy(slot->x1, renderer.x1, sizeof(slot->x1));
}

void fluid_render_scene(uint16_t *pixels, fluid_scene_t *scene)
{
    float dt = renderer.started ? (float)(uint32_t)(scene->time_ms - renderer.last_ms) * 0.001f : 0.016f;
    if (dt > 0.05f) dt = 0.05f;
    renderer.last_ms = scene->time_ms;
    renderer.started = true;
    scene->impact = false;
    uint8_t tint[3], source[3];
    fluid_liquid_rgb(scene->liquid, tint);
    fluid_liquid_rgb(scene->source ? scene->source : scene->liquid, source);
    unsigned total = bottle_liquid_total(scene->liquid);
    if (total != renderer.last_total) {
        renderer.last_total = total;
        renderer.value_until_ms = scene->time_ms + 2500;
    }

    clear(pixels);
    draw_particles(pixels);
    if (scene->flow) emit_stream(scene, source, dt);
    else {
        renderer.emit_credit = renderer.bubble_credit = 0;
        renderer.last_drop = -1;
    }
    update_fx(scene, dt);
    draw_fx(pixels, scene, source);
    draw_frame(pixels, scene->liquid, tint);
    draw_cue(pixels, scene->flow ? FLUID_CUE_NONE : scene->cue, scene->time_ms);
    if (scene->linked || (int32_t)(renderer.value_until_ms - scene->time_ms) > 0)
        draw_percent(pixels, (total * 100 + BOTTLE_CAPACITY / 2) / BOTTLE_CAPACITY, 330, 173);
    remember(pixels);
}
