/* Host check for lz3d: renders the badge frame strip by strip, as the device does, into PPM files.
 * cc -std=c11 -O2 -fsanitize=address,undefined -I../firmware/main/model -I../firmware/main/render \
 *    host_render.c ../firmware/main/render/lz3d.c ../firmware/main/model/lz_model_data.c -lm -o host_render
 * ./host_render <out_dir> */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lz3d.h"

#define W 360
#define ROWS 12
static void *alloc(unsigned n) { return malloc(n); }
static uint16_t frame[W * W], zrow[W * ROWS], strip[W * ROWS];

static uint16_t swap16(uint16_t c) { return (uint16_t)((c << 8) | (c >> 8)); }

static double render(lz3d_t *r, int anim, float t, float yaw, float pitch)
{
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    lz3d_pose(r, anim, t);
    lz3d_prepare(r, yaw, pitch);
    for (int i = 0; i < W * W; i++) frame[i] = r->cfg.bg;
    int x0 = r->x0, x1 = r->x1, w = x1 - x0;
    if (!lz3d_bin(r, r->y0, r->y1)) { fprintf(stderr, "bin overflow\n"); exit(1); }
    for (int y = r->y0; y < r->y1; y += ROWS) {
        int rows = r->y1 - y < ROWS ? r->y1 - y : ROWS;
        /* Two column halves, as the two cores render them. */
        int mid = x0 + w / 2;
        lz3d_render(r, strip, zrow, x0, w, y, rows, x0, mid);
        lz3d_render(r, strip, zrow, x0, w, y, rows, mid, x1);
        for (int i = 0; i < rows; i++) memcpy(frame + (y + i) * W + x0, strip + i * w, w * 2);
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    return (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6;
}

static void save(const char *path)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6 %d %d 255\n", W, W);
    for (int i = 0; i < W * W; i++) {
        uint16_t c = swap16(frame[i]);
        unsigned char p[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
        fwrite(p, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";
    lz3d_config_t cfg = {W, W, 180, 164, 0, 304, swap16(0x1083), lz_model.texture, true};
    cfg.scale = 136.0f / lz_model.radius;
    lz3d_t on, off;
    if (!lz3d_init(&on, &lz_model, &cfg, ROWS, alloc, alloc)) return 1;
    cfg.cull = false;
    if (!lz3d_init(&off, &lz_model, &cfg, ROWS, alloc, alloc)) return 1;
    static uint16_t ref[W * W];
    struct { int anim; float t, yaw, pitch; const char *name; } shots[] = {
        {0, 0, 0, 0, "idle_front"}, {0, 0, 0.785f, 0, "idle_45"}, {0, 0, 1.571f, 0, "idle_90"},
        {0, 0, 3.1416f, 0, "idle_back"}, {0, 0, -1.571f, 0, "idle_270"}, {0, 0, 0.5f, 0.4f, "idle_tilt_down"},
        {0, 0, -0.5f, -0.4f, "idle_tilt_up"}, {1, 1.5f, 0, 0, "hello_1.5s"}, {2, 2.0f, 0, 0, "listen_2s"},
        {3, 1.0f, 0, 0, "talk_1s"}, {3, 2.5f, 0.6f, 0, "talk_2.5s_yaw"},
    };
    int worst = 0;
    for (unsigned i = 0; i < sizeof(shots) / sizeof(shots[0]); i++) {
        render(&off, shots[i].anim, shots[i].t, shots[i].yaw, shots[i].pitch);
        char npath[512];
        snprintf(npath, sizeof(npath), "%s/%s_nocull.ppm", out, shots[i].name);
        save(npath);
        memcpy(ref, frame, sizeof(frame));
        double ms = render(&on, shots[i].anim, shots[i].t, shots[i].yaw, shots[i].pitch);
        int diff = 0;
        for (int k = 0; k < W * W; k++) diff += ref[k] != frame[k];
        if (diff > worst) worst = diff;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.ppm", out, shots[i].name);
        save(path);
        printf("%-16s tris=%3d region=%dx%d cull_diff_px=%d host_ms=%.2f\n", shots[i].name, on.tri_visible,
               on.x1 - on.x0, on.y1 - on.y0, diff, ms);
    }
    /* Sweep a full turn to find any angle where culling changes pixels. */
    int sweep_worst = 0; float sweep_yaw = 0;
    for (int d = 0; d < 360; d += 5) for (int p = -2; p <= 2; p++) {
        float yaw = d * 3.14159265f / 180, pitch = p * 0.2f;
        render(&off, 0, 0, yaw, pitch); memcpy(ref, frame, sizeof(frame));
        render(&on, 0, 0, yaw, pitch);
        int diff = 0;
        for (int k = 0; k < W * W; k++) diff += ref[k] != frame[k];
        if (diff > sweep_worst) { sweep_worst = diff; sweep_yaw = d + p * 1000; }
    }
    printf("sweep worst cull diff: %d px at code %.0f (deg + pitch_step*1000)\n", sweep_worst, sweep_yaw);
    return 0;
}
