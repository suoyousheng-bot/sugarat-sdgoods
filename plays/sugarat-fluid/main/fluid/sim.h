/* Copyright (c) 2026 V4C38. SPDX-License-Identifier: MIT
 * Modified by Deotaland: SDGoods circular container and single-worker ownership.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

// Per-particle state handed to the renderer. Positions are in simulation
// space: x and y in screen pixels, z in pixels of depth into the case.
typedef struct {
    float x, y, z;
    float speed;  // px/s, used to pick a colour
    uint8_t rgb[3];  // visual ingredient colour; drifts toward the bottle mix
} sim_particle_view_t;

/** Entry point and velocity for particles added by sim_set_count(). */
typedef struct {
    float x, y;      // screen pixels inside the container
    float vx, vy;    // simulation px/s
    uint8_t rgb[3];  // colour of the entering liquid
} sim_inlet_t;

// External accelerations for one step, expressed in simulation axes.
typedef struct {
    float gravity[3];  // px/s^2, gravity plus the shake pseudo-force
    float omega[3];    // rad/s, box angular velocity
    float alpha[3];    // rad/s^2, box angular acceleration
} sim_forces_t;

// Diagnostics, logged periodically so the solver can be tuned from the serial
// console without watching the screen. The timings are microseconds spent in
// each pass of the most recent substep.
typedef struct {
    float mean_density;
    float rest_density;
    float mean_speed;
    float max_speed;
    int us_grid;
    int us_density;  // includes viscosity; the two share one pass
    int us_relax;

    // Front and back fillet bands compared against each other. The fluid
    // settles against the back of the case but not against the glass, and these
    // are here to say why: whether the front is denser, moving faster, being
    // pushed further by the wall, or saturating the per-pair displacement cap.
    float front_density, back_density;
    float front_speed, back_speed;
    float front_push, back_push;  // mean wall projection distance, px
    int front_count, back_count;
    int front_hits, back_hits;    // particles the wall had to move
    int clamped;                  // pairs that saturated MAX_DISPLACEMENT
    int pairs;                    // neighbour pairs cached for relaxation
} sim_stats_t;

void sim_init(void);

// Re-seeds particles inside the bottom of the circular container.
void sim_reset(void);

// Advances the fluid by dt_real seconds of wall-clock time.
void sim_step(float dt_real, const sim_forces_t *forces);

// Copies the most recently published particle state. Returns the count.
int sim_snapshot(sim_particle_view_t *out, int max);

void sim_stats(sim_stats_t *out);

/** Match a liquid quantity (0..PARTICLE_MAX). Removed particles leave nearest the bottle
 * mouth (MOUTH_X, MOUTH_Y); added particles enter at the inlet. Called by the solver owner only.
 * @param count Desired particle quantity, clamped to the simulation capacity.
 * @param inlet Entry point, velocity and colour of added particles.
 */
void sim_set_count(int count, const sim_inlet_t *inlet);

/** Set the colour particles drift toward; stirring speeds the drift.
 * @param rgb Mixed bottle colour.
 * @param immediate Repaint every particle now instead of blending.
 */
void sim_set_mix(const uint8_t rgb[3], bool immediate);

/** Pull particles within OUTLET_RADIUS of the mouth toward it, as liquid draining out.
 * @param accel Peak pull in simulation px/s^2; 0 disables it.
 */
void sim_set_outlet(float accel);
