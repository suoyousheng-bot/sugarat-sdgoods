/* Copyright (c) 2026 SDGOODS; modified by Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Adapted from SDGOODS ui_flappy.c: isolated time-based flight and round-screen obstacle bounds. */
#include "bird_model.h"
#include <string.h>

static float random_unit(bird_model_t *g)
{
    g->random = g->random * 1664525u + 1013904223u;
    return (g->random >> 8) * (1.0f / 16777216);
}
unsigned bird_model_level(const bird_model_t *g)
{
    unsigned level = g->score / 5 + 1;
    return level > 8 ? 8 : level;
}
static void obstacle(bird_model_t *g, bird_pipe_t *pipe, float x, float previous)
{
    unsigned level = bird_model_level(g);
    pipe->x = x;
    pipe->gap = 88 - (level - 1) * 3;
    float center = previous + (random_unit(g) - .5f) * 34;
    float minimum = pipe->gap * .5f + 12, maximum = BIRD_FIELD_H - minimum;
    pipe->center = center < minimum ? minimum : center > maximum ? maximum : center;
    pipe->scored = false;
}
void bird_model_init(bird_model_t *g, uint32_t best, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->best = best; g->random = seed; g->y = BIRD_FIELD_H * .5f - BIRD_SIZE * .5f;
}
void bird_model_start(bird_model_t *g)
{
    if (g->paused || g->phase == BIRD_PLAYING) return;
    g->score = 0; g->elapsed_ms = g->remainder_ms = 0;
    g->y = BIRD_FIELD_H * .5f - BIRD_SIZE * .5f;
    g->velocity = -205;
    g->phase = BIRD_PLAYING;
    float center = BIRD_FIELD_H * .5f;
    for (unsigned i = 0; i < BIRD_PIPE_COUNT; i++) {
        obstacle(g, &g->pipes[i], 270 + i * 155, center);
        center = g->pipes[i].center;
    }
}
void bird_model_flap(bird_model_t *g)
{
    if (g->phase == BIRD_PLAYING && !g->paused) g->velocity = -205;
}
static void step(bird_model_t *g)
{
    const float dt = .008f;
    g->elapsed_ms += 8;
    g->velocity += 820 * dt;
    if (g->velocity > 330) g->velocity = 330;
    g->y += g->velocity * dt;
    bool dead = g->y <= 0 || g->y + BIRD_SIZE >= BIRD_FIELD_H;
    float speed = 68 + (bird_model_level(g) - 1) * 6;
    for (unsigned i = 0; i < BIRD_PIPE_COUNT; i++) g->pipes[i].x -= speed * dt;
    for (unsigned i = 0; i < BIRD_PIPE_COUNT; i++) {
        bird_pipe_t *p = &g->pipes[i];
        if (BIRD_X + BIRD_SIZE > p->x && BIRD_X < p->x + BIRD_PIPE_W &&
            (g->y < p->center - p->gap * .5f || g->y + BIRD_SIZE > p->center + p->gap * .5f)) dead = true;
    }
    if (dead) { g->phase = BIRD_OVER; return; }
    for (unsigned i = 0; i < BIRD_PIPE_COUNT; i++) {
        bird_pipe_t *p = &g->pipes[i];
        if (!p->scored && p->x + BIRD_PIPE_W < BIRD_X) {
            p->scored = true;
            if (g->score < 9999) g->score++;
            if (g->score > g->best) g->best = g->score;
        }
        if (p->x + BIRD_PIPE_W < 0) {
            unsigned last = 0;
            for (unsigned j = 1; j < BIRD_PIPE_COUNT; j++)
                if (g->pipes[j].x > g->pipes[last].x) last = j;
            obstacle(g, p, g->pipes[last].x + 155, g->pipes[last].center);
        }
    }
}
void bird_model_tick(bird_model_t *g, uint32_t elapsed_ms)
{
    if (g->paused || g->phase != BIRD_PLAYING) return;
    g->remainder_ms += elapsed_ms > 100 ? 100 : elapsed_ms;
    while (g->remainder_ms >= 8 && g->phase == BIRD_PLAYING) {
        g->remainder_ms -= 8;
        step(g);
    }
}
