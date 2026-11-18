/* Host check for the boot animation decoder: decodes bootanim.bin with firmware/main/bootanim and
 * compares every frame and the audio with the CRCs written by encode_bootanim.py.
 * cc -std=c11 -O2 -Wall -Wextra -fsanitize=address,undefined -DBOOTANIM_HOST -I../firmware/main/bootanim \
 *    host_bootanim.c ../firmware/main/bootanim/bootanim.c -lz -o host_bootanim
 * ./host_bootanim <bootanim.bin> <out_dir>   (writes frame PPMs every 20 frames and audio.wav) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include "bootanim.h"

static void *alloc(size_t n) { return malloc(n); }

static void ppm(const char *path, const uint16_t *px, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint16_t c = (uint16_t)((px[i] << 8) | (px[i] >> 8));
        unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void put32(FILE *f, unsigned v) { unsigned char b[4] = {v, v >> 8, v >> 16, v >> 24}; fwrite(b, 1, 4, f); }
static void put16(FILE *f, unsigned v) { unsigned char b[2] = {v, v >> 8}; fwrite(b, 1, 2, f); }

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: %s <bootanim.bin> <out_dir>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (fread(data, 1, (size_t)size, f) != (size_t)size) { fprintf(stderr, "short read\n"); return 1; }
    fclose(f);

    char path[1024];
    snprintf(path, sizeof(path), "%s.crc", argv[1]);
    FILE *cf = fopen(path, "r");
    if (!cf) { perror(path); return 1; }

    bootanim_t b;
    if (!bootanim_open(&b, data, (size_t)size, alloc, alloc, free)) { fprintf(stderr, "open failed\n"); return 1; }
    printf("%dx%d %d fps, %d frames, audio %u samples at %d Hz\n", b.width, b.height, b.fps, b.frame_count, b.audio_samples, b.audio_rate);

    int bad = 0;
    long px_sum = 0;
    clock_t t0 = clock();
    for (int i = 0; i < b.frame_count; i++) {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        if (!bootanim_decode_next(&b, &x0, &y0, &x1, &y1)) { fprintf(stderr, "frame %d: decode failed\n", i); return 1; }
        if (x1 > x0) px_sum += (long)(x1 - x0) * (y1 - y0);
        unsigned want;
        if (fscanf(cf, "%x", &want) != 1) { fprintf(stderr, "crc file too short\n"); return 1; }
        unsigned got = (unsigned)crc32(0, (const Bytef *)b.frame, (uInt)(b.width * b.height * 2));
        if (got != want) { fprintf(stderr, "frame %d: crc %08x, expected %08x\n", i, got, want); bad++; }
        if (i % 20 == 0 || i == b.frame_count - 1) {
            snprintf(path, sizeof(path), "%s/boot_%03d.ppm", argv[2], i);
            ppm(path, b.frame, b.width, b.height);
        }
    }
    double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (bootanim_decode_next(&b, &x0, &y0, &x1, &y1)) { fprintf(stderr, "decoded past the last frame\n"); return 1; }
    printf("frames: %d mismatches, avg changed region %ld px, host decode %.1f ms total\n", bad, px_sum / b.frame_count, ms);

    bootanim_audio_t a;
    bootanim_audio_init(&b, &a);
    bootanim_close(&b);
    bootanim_close(&b);
    int16_t *pcm = malloc(sizeof(int16_t) * (a.samples + 1));
    size_t n = 0, got;
    while ((got = bootanim_audio_read(&a, pcm + n, 300)) > 0) n += got;
    unsigned want;
    if (fscanf(cf, " audio %x", &want) != 1) { fprintf(stderr, "crc file has no audio line\n"); return 1; }
    unsigned crc = (unsigned)crc32(0, (const Bytef *)pcm, (uInt)(n * 2));
    int peak = 0;
    for (size_t i = 0; i < n; i++) { int v = abs(pcm[i]); if (v > peak) peak = v; }
    printf("audio: %zu samples, peak %d, crc %s\n", n, peak, crc == want ? "ok" : "MISMATCH");
    if (crc != want || n != b.audio_samples) bad++;

    snprintf(path, sizeof(path), "%s/audio.wav", argv[2]);
    FILE *w = fopen(path, "wb");
    fwrite("RIFF", 1, 4, w); put32(w, 36 + (unsigned)n * 2); fwrite("WAVEfmt ", 1, 8, w);
    put32(w, 16); put16(w, 1); put16(w, 1); put32(w, (unsigned)b.audio_rate); put32(w, (unsigned)b.audio_rate * 2);
    put16(w, 2); put16(w, 16); fwrite("data", 1, 4, w); put32(w, (unsigned)n * 2);
    fwrite(pcm, 2, n, w);
    fclose(w);
    fclose(cf);
    free(pcm);
    free(data);
    return bad ? 1 : 0;
}
