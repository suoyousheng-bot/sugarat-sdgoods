/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Deterministic single-player flight, fixed 8 ms steps and bounded obstacle recycling. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define BIRD_FIELD_W 240
#define BIRD_FIELD_H 168
#define BIRD_X 48
#define BIRD_SIZE 18
#define BIRD_PIPE_W 32
#define BIRD_PIPE_COUNT 3

typedef enum { BIRD_READY, BIRD_PLAYING, BIRD_OVER } bird_phase_t;
typedef struct { float x, center, gap; bool scored; } bird_pipe_t;
typedef struct {
    bird_phase_t phase;
    bool paused;
    float y, velocity;
    uint32_t score, best, random, elapsed_ms, remainder_ms;
    bird_pipe_t pipes[BIRD_PIPE_COUNT];
} bird_model_t;
/** Create a ready game; seed 0 is supported and best is caller-loaded persistent data.
 * @param game Destination game state.
 * @param best Previous highest score.
 * @param seed Reproducible obstacle seed.
 */
void bird_model_init(bird_model_t *game, uint32_t best, uint32_t seed);
/** Start or restart a run with an initial flap; retain best and obstacle random state.
 * @param game Game that is ready or over; paused/running games are unchanged.
 */
void bird_model_start(bird_model_t *game);
/** Apply one upward impulse only during unpaused play.
 * @param game Game receiving a screen press.
 */
void bird_model_flap(bird_model_t *game);
/** Advance up to 100 ms, with 8 ms fixed steps; paused/finished games do not advance.
 * @param game Mutable flight state.
 * @param elapsed_ms Monotonic elapsed time since the caller's preceding update.
 */
void bird_model_tick(bird_model_t *game, uint32_t elapsed_ms);
/** @returns Difficulty level, increasing every five passed pipes, capped at eight.
 * @param game Current game.
 */
unsigned bird_model_level(const bird_model_t *game);
