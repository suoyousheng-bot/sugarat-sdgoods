/* SPDX-License-Identifier: Apache-2.0 */
/** Deterministic local physics; the worker owns a game and advances it with bounded time steps. */
#pragma once
#include "renderer.h"
/** Laps in one road race; distance_m accumulates up to laps times the lap length. */
#define RACE_LAPS 3
typedef enum { RACE_IDLE, RACE_COUNTDOWN, RACE_RUNNING, RACE_FINISHED } race_phase;
/** One-shot gameplay events accumulated by race_game_step(); the caller clears them after reading. */
enum {
    RACE_EVENT_WALL = 1,        /* marble hit a wall; impact holds the strongest normal speed in px/s */
    RACE_EVENT_CHECKPOINT = 2,  /* marble collected a new checkpoint */
    RACE_EVENT_BOOST = 4,       /* car drove over a boost pad */
    RACE_EVENT_BUMP = 8,        /* car touched a rival; impact holds the closing speed in m/s */
    RACE_EVENT_LAP = 16,        /* car started a new lap other than the last */
    RACE_EVENT_FINAL_LAP = 32,  /* car started the last lap */
};
typedef struct { float distance, lateral, speed, skill, lane, phase; } race_rival;
typedef struct {
    rr_state frame;
    race_phase phase;
    float countdown, elapsed, speed, lateral, x, y, vx, vy;
    unsigned checkpoint_mask;
    int rival_count, pad_lap[RR_BOOST_PADS];
    race_rival rivals[RR_RIVALS];
    float boost, bump_cooldown, lap_flash, impact;
    unsigned events;
} race_game;
/** Calibrated hold: two screen-plane axes perpendicular to the gravity seen at calibration. */
typedef struct { float ex[3], ey[3]; } race_tilt;
/** Start a new fixed map with a three-second countdown; rivals adds computer cars to a road race. */
void race_game_begin(race_game *game,int mode,int map,bool rivals);
/** Advance by dt seconds; input axes are race_tilt_read() values, the sine of tilt from the calibrated hold. */
void race_game_step(race_game *game,float dt,float input_x,float input_y);
/** Circle collision against fixed maze cells; radius is in screen pixels. */
bool race_game_ball_free(int map,float x,float y,float radius);
/**
 * Treat the current hold as level, whether the badge lies flat, stands upright or anything between.
 * @param tilt Receives the calibrated axes.
 * @param x,y,z Accelerometer reading in g in the device frame.
 * @returns false when the reading is not close to 1 g; tilt is then unchanged.
 */
bool race_tilt_calibrate(race_tilt *tilt,float x,float y,float z);
/**
 * Convert a reading to game input axes relative to the calibrated hold.
 * @param tilt Axes from race_tilt_calibrate().
 * @param x,y,z Accelerometer reading in g in the device frame.
 * @param input_x,input_y Receive screen-right and screen-down slope, -1..1.
 * @returns false when the reading is not close to 1 g; outputs are then unchanged.
 */
bool race_tilt_read(const race_tilt *tilt,float x,float y,float z,float *input_x,float *input_y);
