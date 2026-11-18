/* Copyright (c) 2026 V4C38. SPDX-License-Identifier: MIT
 * Modified by Deotaland: SDGoods circular container and single-worker ownership.
 */
#include "sim.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"

// Cells cover at least SMOOTH_RADIUS on all axes; depth-major ordering supports back-to-front drawing.
#define GRID_CX 16
#define GRID_CY 16
#define GRID_CZ 3
#define GRID_CELLS (GRID_CX * GRID_CY * GRID_CZ)

// Cell index is z-major, so sorting particles by cell also sorts them by
// depth. The renderer walks the result backwards to draw back-to-front.
#define CELL_INDEX(cx, cy, cz) ((((cz) * GRID_CY) + (cy)) * GRID_CX + (cx))

#define WALL_MARGIN 5.0f

static const char *TAG = "sim";

// Coordinates are interleaved rather than kept in parallel arrays: the inner
// loop reads all three components of a neighbour together, so keeping them on
// one cache line matters far more than vectorisation would.
typedef struct {
    float pos[PARTICLE_MAX][3];
    float vel[PARTICLE_MAX][3];
    float old[PARTICLE_MAX][3];
} particles_t;

static particles_t s_pool[2];
// Colours are visual only, 8.8 fixed point, reordered with the particles.
static EXT_RAM_BSS_ATTR uint16_t s_rgb_pool[2][PARTICLE_MAX][3];
static uint16_t (*s_rgb)[3] = s_rgb_pool[0];
static uint16_t (*s_rgb_alt)[3] = s_rgb_pool[1];
static uint16_t s_mix[3] = {30 << 8, 160 << 8, 245 << 8};
static float s_outlet_accel;
static particles_t *s_p = &s_pool[0];
static particles_t *s_alt = &s_pool[1];

static int s_count = PARTICLE_COUNT;

static float s_density[PARTICLE_MAX];
static float s_density_near[PARTICLE_MAX];
// Viscosity offsets, accumulated during the density pass and applied after it.
// Moving a particle mid-walk would corrupt the densities still being summed.
static float s_visc_delta[PARTICLE_MAX][3];

// Pressure and near-pressure, kept side by side so reading a neighbour's pair
// touches one cache line instead of two.
static float s_press[PARTICLE_MAX][2];

static uint16_t s_cell_of[PARTICLE_MAX];
static uint16_t s_order[PARTICLE_MAX];
static uint16_t s_cell_start[GRID_CELLS + 1];

// Neighbour pairs found by the density pass, for the relaxation pass to reuse.
//
// Both passes need exactly the same pairs, and finding them is far more
// expensive than using them: the 3x3x3 block of cells around a particle holds
// about 54 candidates with j > i, of which only about 9 are actually within the
// smoothing radius. Recording the survivors the first time lets relaxation skip
// the other 45 entirely, which measured at roughly a third of the solver's total
// runtime.
//
// Stored compressed-row style: pairs for particle i are s_pair[s_pair_off[i] ..
// s_pair_off[i + 1]). The density pass visits i in ascending order, because
// particles are sorted by cell and cells are numbered in order, so the offsets
// can be filled in as it goes without a second pass.
//
// Sized for 27 pairs per particle, about twice the worst compression measured.
// If that is ever not enough the pass says so rather than quietly dropping
// pairs, and relaxation falls back to walking the grid itself.
#define PAIR_MAX 16384
static EXT_RAM_BSS_ATTR uint16_t s_pair[PAIR_MAX];
static uint32_t s_pair_off[PARTICLE_MAX + 1];
static bool s_pairs_valid;
static uint16_t s_cursor[GRID_CELLS];

static float s_rest_density;
static float s_cell_scale_x, s_cell_scale_y, s_cell_scale_z;

// The fluid worker owns simulation, snapshots and rendering; LVGL receives pixels only.
static EXT_RAM_BSS_ATTR sim_particle_view_t s_view_pool[2][PARTICLE_MAX];
static sim_particle_view_t *s_view_work = s_view_pool[0];
static sim_particle_view_t *s_view_pub = s_view_pool[1];
static int s_view_pub_count;


static sim_stats_t s_stats;
static uint32_t s_rng = 0x2545F491u;

static inline float rand_unit(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return (float)(s_rng & 0xFFFFFF) * (1.0f / 16777215.0f);
}

// The rest density is whatever the kernel sums to for a perfect lattice at the
// target spacing. Measuring it here means REST_SPACING and SMOOTH_RADIUS can
// be changed independently without hand-retuning a magic number.
static void calibrate_rest_density(void)
{
    const float h = SMOOTH_RADIUS;
    const int reach = (int)ceilf(h / REST_SPACING);
    float rho = 0.0f;

    for (int i = -reach; i <= reach; i++) {
        for (int j = -reach; j <= reach; j++) {
            for (int k = -reach; k <= reach; k++) {
                if (i == 0 && j == 0 && k == 0) {
                    continue;
                }
                const float d = REST_SPACING * sqrtf((float)(i * i + j * j + k * k));
                if (d >= h) {
                    continue;
                }
                const float q = 1.0f - d / h;
                rho += q * q;
            }
        }
    }
    s_rest_density = rho;
}

void sim_reset(void)
{
    s_count = PARTICLE_COUNT;
    const float spacing = REST_SPACING;
    int count = 0;
    const float radius = FLUID_RADIUS - WALL_MARGIN - spacing;
    for (float y = BOX_H * 0.5f + radius; y >= BOX_H * 0.5f - radius && count < s_count; y -= spacing) {
        for (float z = WALL_MARGIN + spacing * 0.5f; z < BOX_D - WALL_MARGIN && count < s_count; z += spacing) {
            for (float x = BOX_W * 0.5f - radius; x <= BOX_W * 0.5f + radius && count < s_count; x += spacing) {
                const float dx = x - BOX_W * 0.5f, dy = y - BOX_H * 0.5f;
                if (dx * dx + dy * dy > radius * radius) continue;
                s_p->pos[count][0] = x + (rand_unit() - 0.5f);
                s_p->pos[count][1] = y + (rand_unit() - 0.5f);
                s_p->pos[count][2] = z;
                for (int a = 0; a < 3; a++) {
                    s_rgb[count][a] = s_mix[a];
                    s_p->vel[count][a] = 0;
                    s_p->old[count][a] = s_p->pos[count][a];
                }
                count++;
            }
        }
    }
    s_count = count;
    ESP_LOGI(TAG, "seeded %d particles", s_count);
}

void sim_init(void)
{
    s_p = &s_pool[0];
    s_alt = &s_pool[1];
    s_rgb = s_rgb_pool[0];
    s_rgb_alt = s_rgb_pool[1];
    s_outlet_accel = 0;
    s_view_pub_count = 0;
    s_rng = 0x2545F491u;
    memset(&s_stats, 0, sizeof(s_stats));

    s_cell_scale_x = (float)GRID_CX / BOX_W;
    s_cell_scale_y = (float)GRID_CY / BOX_H;
    s_cell_scale_z = (float)GRID_CZ / BOX_D;

    // Neighbour search is only correct while each cell spans at least one
    // smoothing radius.
    const float cw = BOX_W / GRID_CX;
    const float ch = BOX_H / GRID_CY;
    const float cd = BOX_D / GRID_CZ;
    if (cw < SMOOTH_RADIUS || ch < SMOOTH_RADIUS || cd < SMOOTH_RADIUS) {
        ESP_LOGE(TAG, "grid cell %.1fx%.1fx%.1f smaller than radius %.1f",
                 (double)cw, (double)ch, (double)cd, (double)SMOOTH_RADIUS);
    }

    calibrate_rest_density();
    ESP_LOGI(TAG, "rest density %.3f (spacing %.1f, radius %.1f)",
             (double)s_rest_density, (double)REST_SPACING, (double)SMOOTH_RADIUS);

    sim_reset();
}

// Counting sort of every particle into its grid cell. Besides building the
// lookup structure this also reorders the particle arrays, so neighbours end
// up adjacent in memory, which matters a lot on a CPU with a small cache.
static void rebuild_grid(void)
{
    memset(s_cell_start, 0, sizeof(s_cell_start));

    for (int i = 0; i < s_count; i++) {
        int cx = (int)(s_p->pos[i][0] * s_cell_scale_x);
        int cy = (int)(s_p->pos[i][1] * s_cell_scale_y);
        int cz = (int)(s_p->pos[i][2] * s_cell_scale_z);
        cx = cx < 0 ? 0 : (cx >= GRID_CX ? GRID_CX - 1 : cx);
        cy = cy < 0 ? 0 : (cy >= GRID_CY ? GRID_CY - 1 : cy);
        cz = cz < 0 ? 0 : (cz >= GRID_CZ ? GRID_CZ - 1 : cz);

        const uint16_t cell = (uint16_t)CELL_INDEX(cx, cy, cz);
        s_cell_of[i] = cell;
        s_cell_start[cell + 1]++;
    }

    for (int c = 0; c < GRID_CELLS; c++) {
        s_cell_start[c + 1] = (uint16_t)(s_cell_start[c + 1] + s_cell_start[c]);
        s_cursor[c] = s_cell_start[c];
    }

    for (int i = 0; i < s_count; i++) {
        s_order[s_cursor[s_cell_of[i]]++] = (uint16_t)i;
    }

    for (int k = 0; k < s_count; k++) {
        const uint16_t src = s_order[k];
        for (int a = 0; a < 3; a++) {
            s_alt->pos[k][a] = s_p->pos[src][a];
            s_alt->vel[k][a] = s_p->vel[src][a];
            s_alt->old[k][a] = s_p->old[src][a];
            s_rgb_alt[k][a] = s_rgb[src][a];
        }
    }

    particles_t *swap = s_p;
    s_p = s_alt;
    s_alt = swap;
    uint16_t (*rgb_swap)[3] = s_rgb;
    s_rgb = s_rgb_alt;
    s_rgb_alt = rgb_swap;
}

// The contiguous runs of sorted particle indices that make up the 3x3x3
// neighbourhood of a cell. Runs are contiguous because cells differing only in
// x are adjacent in the cell numbering. Every particle in a cell shares the
// same neighbourhood, so this is computed once per cell rather than per
// particle.
typedef struct {
    int start[9];
    int end[9];
    int runs;
} neighbourhood_t;

static void neighbourhood_for_cell(int cx, int cy, int cz, neighbourhood_t *nb)
{
    const int x0 = cx > 0 ? cx - 1 : 0;
    const int x1 = cx < GRID_CX - 1 ? cx + 1 : GRID_CX - 1;

    nb->runs = 0;
    for (int dz = -1; dz <= 1; dz++) {
        const int nz = cz + dz;
        if (nz < 0 || nz >= GRID_CZ) {
            continue;
        }
        for (int dy = -1; dy <= 1; dy++) {
            const int ny = cy + dy;
            if (ny < 0 || ny >= GRID_CY) {
                continue;
            }
            const int begin = s_cell_start[CELL_INDEX(x0, ny, nz)];
            const int stop = s_cell_start[CELL_INDEX(x1, ny, nz) + 1];
            if (stop > begin) {
                nb->start[nb->runs] = begin;
                nb->end[nb->runs] = stop;
                nb->runs++;
            }
        }
    }
}

// The three solver passes all walk the grid the same way. Cells are numbered
// so that incrementing by one steps along x, which keeps the loop free of any
// integer division.
#define FOR_EACH_CELL(cx, cy, cz, cell)                     \
    for (int cz = 0, cell = 0; cz < GRID_CZ; cz++)          \
        for (int cy = 0; cy < GRID_CY; cy++)                \
            for (int cx = 0; cx < GRID_CX; cx++, cell++)

// Densities and Clavet viscosity in a single pass over the grid.
//
// Walking the neighbour grid is the most expensive thing the solver does, and
// these two need exactly the same pairs and the same square root, so they
// share one walk. Viscosity normally acts on velocity before positions are
// integrated; here positions have already moved, so the impulse is applied as
// a position offset of the same size (impulse * dt). Velocity is recovered
// from total displacement at the end of the substep, so the result is
// identical. Offsets go to a separate buffer, otherwise moving a particle
// mid-walk would corrupt the densities still being summed.
static void compute_densities_and_viscosity(float dt)
{
    const float h2 = SMOOTH_RADIUS * SMOOTH_RADIUS;
    const float inv_h = 1.0f / SMOOTH_RADIUS;

    memset(s_density, 0, sizeof(float) * (size_t)s_count);
    memset(s_density_near, 0, sizeof(float) * (size_t)s_count);
    memset(s_visc_delta, 0, sizeof(float) * 3 * (size_t)s_count);

    uint32_t pairs = 0;
    s_pairs_valid = true;

    FOR_EACH_CELL(cx, cy, cz, cell) {
        const int i0 = s_cell_start[cell];
        const int i1 = s_cell_start[cell + 1];
        if (i0 == i1) {
            continue;
        }

        neighbourhood_t nb;
        neighbourhood_for_cell(cx, cy, cz, &nb);

        for (int i = i0; i < i1; i++) {
            const float xi = s_p->pos[i][0], yi = s_p->pos[i][1], zi = s_p->pos[i][2];
            const float vxi = s_p->vel[i][0], vyi = s_p->vel[i][1], vzi = s_p->vel[i][2];

            s_pair_off[i] = pairs;

            float rho = s_density[i];
            float rho_near = s_density_near[i];
            float dvx = 0.0f, dvy = 0.0f, dvz = 0.0f;

            for (int run = 0; run < nb.runs; run++) {
                int j = nb.start[run];
                if (j <= i) {
                    j = i + 1;  // every pair is visited exactly once
                }
                for (; j < nb.end[run]; j++) {
                    const float dx = s_p->pos[j][0] - xi;
                    const float dy = s_p->pos[j][1] - yi;
                    const float dz = s_p->pos[j][2] - zi;
                    const float r2 = dx * dx + dy * dy + dz * dz;
                    if (r2 >= h2 || r2 < 1e-6f) {
                        continue;
                    }

                    if (pairs < PAIR_MAX) {
                        s_pair[pairs++] = (uint16_t)j;
                    } else {
                        s_pairs_valid = false;
                    }

                    const float r = sqrtf(r2);
                    const float inv_r = 1.0f / r;
                    const float q = 1.0f - r * inv_h;
                    const float q2 = q * q;

                    rho += q2;
                    rho_near += q2 * q;
                    s_density[j] += q2;
                    s_density_near[j] += q2 * q;

                    const float ux = dx * inv_r, uy = dy * inv_r, uz = dz * inv_r;
                    const float u = (vxi - s_p->vel[j][0]) * ux +
                                    (vyi - s_p->vel[j][1]) * uy +
                                    (vzi - s_p->vel[j][2]) * uz;
                    if (u <= 0.0f) {
                        continue;  // only damp approach, never pull apart
                    }
                    // Half the impulse goes to each particle. Never remove
                    // more than half the closing speed, or strong viscosity
                    // would push the pair back apart and oscillate.
                    float dv = 0.5f * dt * q * (VISC_SIGMA * u + VISC_BETA * u * u);
                    if (dv > 0.5f * u) {
                        dv = 0.5f * u;
                    }
                    const float imp = dv * dt;

                    dvx -= imp * ux;
                    dvy -= imp * uy;
                    dvz -= imp * uz;
                    s_visc_delta[j][0] += imp * ux;
                    s_visc_delta[j][1] += imp * uy;
                    s_visc_delta[j][2] += imp * uz;
                }
            }

            s_density[i] = rho;
            s_density_near[i] = rho_near;
            s_visc_delta[i][0] += dvx;
            s_visc_delta[i][1] += dvy;
            s_visc_delta[i][2] += dvz;
        }
    }

    s_pair_off[s_count] = pairs;
    s_stats.pairs = (int)pairs;

    for (int i = 0; i < s_count; i++) {
        for (int a = 0; a < 3; a++) {
            s_p->pos[i][a] += s_visc_delta[i][a];
        }
    }
}

// One relaxation pair. Both loop shells below share this so the physics exists
// once; only the way j is arrived at differs.
//
// The r2 test stays even on the cached path. Pushes are applied immediately, so
// a pair recorded by the density pass may already have been driven apart past
// the smoothing radius by the time it is reached, and it has to drop out exactly
// as it would have when rediscovered.
static inline void relax_pair(int j, float xi, float yi, float zi,
                              float p_i, float pn_i, float dt2, float h2,
                              float inv_h, float *mx, float *my, float *mz,
                              int *clamped)
{
    const float dx = s_p->pos[j][0] - xi;
    const float dy = s_p->pos[j][1] - yi;
    const float dz = s_p->pos[j][2] - zi;
    const float r2 = dx * dx + dy * dy + dz * dz;
    if (r2 >= h2 || r2 < 1e-6f) {
        return;
    }

    const float r = sqrtf(r2);
    const float inv_r = 1.0f / r;
    const float q = 1.0f - r * inv_h;
    const float q2 = q * q;

    const float p_j = s_press[j][0];
    const float pn_j = s_press[j][1];

    // Each particle pushes according to its own pressure; summing both halves
    // keeps the pair symmetric, so momentum is conserved and the pair is
    // visited once.
    float d = 0.5f * dt2 * ((p_i + p_j) * q + (pn_i + pn_j) * q2);
    if (d > MAX_DISPLACEMENT) {
        d = MAX_DISPLACEMENT;
        (*clamped)++;
    } else if (d < -MAX_DISPLACEMENT) {
        d = -MAX_DISPLACEMENT;
        (*clamped)++;
    }

    const float sx = dx * inv_r * d;
    const float sy = dy * inv_r * d;
    const float sz = dz * inv_r * d;

    s_p->pos[j][0] += sx;
    s_p->pos[j][1] += sy;
    s_p->pos[j][2] += sz;
    *mx -= sx;
    *my -= sy;
    *mz -= sz;
}

// Double density relaxation. Ordinary pressure is signed, so a particle in a
// too-sparse region is pulled back towards its neighbours, which gives the
// fluid a cohesive surface. The near pressure is always repulsive and is what
// stops particles from piling up into clumps.
//
// Pushes are applied straight away rather than gathered up and applied at the
// end, so later pairs already see the corrected positions. That converges faster
// in dense regions, at the cost of the rare corner blow-up noted in the README.
static void relax_positions(float dt)
{
    const float h2 = SMOOTH_RADIUS * SMOOTH_RADIUS;
    const float inv_h = 1.0f / SMOOTH_RADIUS;
    const float dt2 = dt * dt;

    int clamped = 0;

    // Turn densities into pressures once, rather than for every pair.
    for (int i = 0; i < s_count; i++) {
        s_press[i][0] = K_PRESSURE * (s_density[i] - s_rest_density);
        s_press[i][1] = K_NEAR_PRESSURE * s_density_near[i];
    }

    if (s_pairs_valid) {
        for (int i = 0; i < s_count; i++) {
            const float xi = s_p->pos[i][0], yi = s_p->pos[i][1], zi = s_p->pos[i][2];
            const float p_i = s_press[i][0];
            const float pn_i = s_press[i][1];

            float mx = 0.0f, my = 0.0f, mz = 0.0f;  // accumulated move for i

            const uint32_t k1 = s_pair_off[i + 1];
            for (uint32_t k = s_pair_off[i]; k < k1; k++) {
                relax_pair(s_pair[k], xi, yi, zi, p_i, pn_i, dt2, h2, inv_h,
                           &mx, &my, &mz, &clamped);
            }

            s_p->pos[i][0] += mx;
            s_p->pos[i][1] += my;
            s_p->pos[i][2] += mz;
        }
    } else {
        // The pair list overflowed, so find the neighbours the hard way.
        FOR_EACH_CELL(cx, cy, cz, cell) {
            const int i0 = s_cell_start[cell];
            const int i1 = s_cell_start[cell + 1];
            if (i0 == i1) {
                continue;
            }

            neighbourhood_t nb;
            neighbourhood_for_cell(cx, cy, cz, &nb);

            for (int i = i0; i < i1; i++) {
                const float xi = s_p->pos[i][0], yi = s_p->pos[i][1],
                            zi = s_p->pos[i][2];
                const float p_i = s_press[i][0];
                const float pn_i = s_press[i][1];

                float mx = 0.0f, my = 0.0f, mz = 0.0f;

                for (int run = 0; run < nb.runs; run++) {
                    int j = nb.start[run];
                    if (j <= i) {
                        j = i + 1;
                    }
                    for (; j < nb.end[run]; j++) {
                        relax_pair(j, xi, yi, zi, p_i, pn_i, dt2, h2, inv_h,
                                   &mx, &my, &mz, &clamped);
                    }
                }

                s_p->pos[i][0] += mx;
                s_p->pos[i][1] += my;
                s_p->pos[i][2] += mz;
            }
        }
    }

    s_stats.clamped = clamped;
}

// The interior of the case: a rounded rectangle in x and y, extruded through
// the depth of the box, and curved into the glass at the front and the back
// panel at the back. Every wall, edge and corner is one continuous surface.
//
// It resolves as the same trick applied twice. Clamp a point to the inner
// rectangle joining the four corner-arc centres, and whatever offset remains
// points straight out from the nearest part of the outline; its length is how
// far out the particle is, which collapses x and y into a single radial
// coordinate. That leaves a 2D problem in (radial, depth), which is again a
// rectangle with rounded corners, so the same clamp-and-project applies.
//
// Every case falls out of it with no branching. Against a flat wall one
// component of the offset is zero; in a vertical corner the first stage gives
// the arc; in the fillet the second stage does; where a corner meets the back
// panel both do at once, which is the doubly-curved patch.
/* Circular wall projection preserves tangential velocity and reflects outward motion. */
static void resolve_walls(void)
{
    const float radius = FLUID_RADIUS - WALL_MARGIN;
    for (int i = 0; i < s_count; i++) {
        float *p = s_p->pos[i], *v = s_p->vel[i];
        const float dx = p[0] - BOX_W * 0.5f, dy = p[1] - BOX_H * 0.5f;
        const float distance = sqrtf(dx * dx + dy * dy);
        if (distance > radius) {
            const float nx = dx / distance, ny = dy / distance;
            const float inset = radius - WALL_JITTER * rand_unit();
            p[0] = BOX_W * 0.5f + nx * inset;
            p[1] = BOX_H * 0.5f + ny * inset;
            const float outward = v[0] * nx + v[1] * ny;
            if (outward > 0) {
                v[0] -= (1.0f + WALL_RESTITUTION) * outward * nx;
                v[1] -= (1.0f + WALL_RESTITUTION) * outward * ny;
            }
            v[0] *= WALL_FRICTION;
            v[1] *= WALL_FRICTION;
        }
        for (int side = 0; side < 2; side++) {
            const float edge = side ? BOX_D - WALL_MARGIN : WALL_MARGIN;
            if ((side && p[2] > edge) || (!side && p[2] < edge)) {
                p[2] = edge;
                if ((side && v[2] > 0) || (!side && v[2] < 0)) {
                    v[2] *= -WALL_RESTITUTION;
                }
            }
        }
    }
}

// Gravity plus the pseudo-forces that appear because the box itself is an
// accelerating, rotating reference frame: centrifugal, Euler (angular
// acceleration) and Coriolis (motion inside a spinning frame).
static void integrate_velocities(float dt, const sim_forces_t *f)
{
    const float gx = f->gravity[0], gy = f->gravity[1], gz = f->gravity[2];
    const float wx = f->omega[0], wy = f->omega[1], wz = f->omega[2];
    const float ax = f->alpha[0], ay = f->alpha[1], az = f->alpha[2];

    const float w2 = wx * wx + wy * wy + wz * wz;
    const bool rotating = (ROTATION_GAIN != 0.0f) &&
                          (w2 > 1e-8f || (ax * ax + ay * ay + az * az) > 1e-8f);

    const float cx = BOX_W * 0.5f, cy = BOX_H * 0.5f, cz = BOX_D * 0.5f;

    for (int i = 0; i < s_count; i++) {
        float accx = gx, accy = gy, accz = gz;

        if (rotating) {
            const float rx = s_p->pos[i][0] - cx;
            const float ry = s_p->pos[i][1] - cy;
            const float rz = s_p->pos[i][2] - cz;

            // Centrifugal: |w|^2 * r_perpendicular, i.e. r*|w|^2 - w*(w.r)
            const float wdotr = wx * rx + wy * ry + wz * rz;
            accx += ROTATION_GAIN * (rx * w2 - wx * wdotr);
            accy += ROTATION_GAIN * (ry * w2 - wy * wdotr);
            accz += ROTATION_GAIN * (rz * w2 - wz * wdotr);

            // Euler: -alpha x r
            accx -= ROTATION_GAIN * (ay * rz - az * ry);
            accy -= ROTATION_GAIN * (az * rx - ax * rz);
            accz -= ROTATION_GAIN * (ax * ry - ay * rx);

            // Coriolis: -2 w x v
            const float vx = s_p->vel[i][0], vy = s_p->vel[i][1], vz = s_p->vel[i][2];
            accx -= ROTATION_GAIN * 2.0f * (wy * vz - wz * vy);
            accy -= ROTATION_GAIN * 2.0f * (wz * vx - wx * vz);
            accz -= ROTATION_GAIN * 2.0f * (wx * vy - wy * vx);
        }

        if (s_outlet_accel > 0) {
            const float dx = MOUTH_X - s_p->pos[i][0], dy = MOUTH_Y - s_p->pos[i][1];
            const float d2 = dx * dx + dy * dy;
            if (d2 < OUTLET_RADIUS * OUTLET_RADIUS && d2 > 1.0f) {
                const float d = sqrtf(d2), pull = s_outlet_accel * (1.0f - d / OUTLET_RADIUS) / d;
                accx += dx * pull;
                accy += dy * pull;
            }
        }

        s_p->vel[i][0] += accx * dt;
        s_p->vel[i][1] += accy * dt;
        s_p->vel[i][2] += accz * dt;
    }
}

static void substep(float dt, const sim_forces_t *f)
{
    integrate_velocities(dt, f);

    const float inv_dt = 1.0f / dt;

    for (int i = 0; i < s_count; i++) {
        for (int a = 0; a < 3; a++) {
            s_p->old[i][a] = s_p->pos[i][a];
            s_p->pos[i][a] += s_p->vel[i][a] * dt;
        }
    }

    int64_t t0 = esp_timer_get_time();
    rebuild_grid();
    s_stats.us_grid = (int)(esp_timer_get_time() - t0);

    t0 = esp_timer_get_time();
    compute_densities_and_viscosity(dt);
    s_stats.us_density = (int)(esp_timer_get_time() - t0);

    t0 = esp_timer_get_time();
    relax_positions(dt);
    s_stats.us_relax = (int)(esp_timer_get_time() - t0);

    // Recover velocity from the actual motion, so the relaxation displacement
    // shows up as real momentum.
    for (int i = 0; i < s_count; i++) {
        for (int a = 0; a < 3; a++) {
            s_p->vel[i][a] = (s_p->pos[i][a] - s_p->old[i][a]) * inv_dt;
        }
    }

    resolve_walls();
}

static void publish(void)
{
    float sum_rho = 0.0f, sum_speed = 0.0f, max_speed = 0.0f;

    // Split the same sums over the two fillet bands, to compare how the fluid
    // behaves against the glass with how it behaves against the back panel.
    const float front_edge = WALL_MARGIN + BOX_FRONT_FILLET;
    const float back_edge = BOX_D - WALL_MARGIN - BOX_BACK_FILLET;
    float f_rho = 0.0f, f_speed = 0.0f, b_rho = 0.0f, b_speed = 0.0f;
    int f_n = 0, b_n = 0;

    for (int i = 0; i < s_count; i++) {
        const float vx = s_p->vel[i][0], vy = s_p->vel[i][1], vz = s_p->vel[i][2];
        const float speed = sqrtf(vx * vx + vy * vy + vz * vz);

        s_view_work[i].x = s_p->pos[i][0];
        s_view_work[i].y = s_p->pos[i][1];
        s_view_work[i].z = s_p->pos[i][2];
        s_view_work[i].speed = speed;
        // Calm liquid blends slowly; sloshing and splashing stir it faster.
        const float stir = 0.006f + fminf(speed * (1.0f / 4000.0f), 1.0f) * 0.05f;
        for (int a = 0; a < 3; a++) {
            s_rgb[i][a] = (uint16_t)(s_rgb[i][a] + ((float)s_mix[a] - s_rgb[i][a]) * stir);
            s_view_work[i].rgb[a] = (uint8_t)(s_rgb[i][a] >> 8);
        }

        sum_rho += s_density[i];
        sum_speed += speed;
        if (speed > max_speed) {
            max_speed = speed;
        }

        const float z = s_p->pos[i][2];
        if (z < front_edge) {
            f_rho += s_density[i];
            f_speed += speed;
            f_n++;
        } else if (z > back_edge) {
            b_rho += s_density[i];
            b_speed += speed;
            b_n++;
        }
    }

    const float inv_n = s_count ? 1.0f / (float)s_count : 0.0f;
    s_stats.mean_density = sum_rho * inv_n;
    s_stats.rest_density = s_rest_density;
    s_stats.mean_speed = sum_speed * inv_n;
    s_stats.max_speed = max_speed;

    s_stats.front_count = f_n;
    s_stats.back_count = b_n;
    s_stats.front_density = f_n ? f_rho / (float)f_n : 0.0f;
    s_stats.back_density = b_n ? b_rho / (float)b_n : 0.0f;
    s_stats.front_speed = f_n ? f_speed / (float)f_n : 0.0f;
    s_stats.back_speed = b_n ? b_speed / (float)b_n : 0.0f;
    if (s_stats.front_hits) {
        s_stats.front_push /= (float)s_stats.front_hits;
    }
    if (s_stats.back_hits) {
        s_stats.back_push /= (float)s_stats.back_hits;
    }

    sim_particle_view_t *swap = s_view_pub;
    s_view_pub = s_view_work;
    s_view_work = swap;
    s_view_pub_count = s_count;
}

void sim_step(float dt_real, const sim_forces_t *forces)
{
    // Everything runs in slowed-down time, including the angular rates, so
    // gravity, shake and rotation stay in proportion to each other.
    //
    // Capped, because the solver's stability depends on dt but its runtime does
    // not depend on dt at all: a slow step must not also be a stiff one. See
    // SIM_DT_MAX.
    if (!isfinite(dt_real) || dt_real <= 0) return;
    float dt = (dt_real * TIME_SCALE) / (float)SUBSTEPS;
    if (dt > SIM_DT_MAX) {
        dt = SIM_DT_MAX;
    }

    sim_forces_t scaled = *forces;
    for (int a = 0; a < 3; a++) {
        scaled.omega[a] *= TIME_SCALE;
        scaled.alpha[a] *= TIME_SCALE * TIME_SCALE;
    }

    for (int s = 0; s < SUBSTEPS; s++) {
        substep(dt, &scaled);
    }
    publish();
}

int sim_snapshot(sim_particle_view_t *out, int max)
{
    int n = s_view_pub_count;
    if (n > max) {
        n = max;
    }
    if (n > 0) {
        memcpy(out, s_view_pub, (size_t)n * sizeof(sim_particle_view_t));
    }
    return n;
}

void sim_stats(sim_stats_t *out)
{
    *out = s_stats;
}

void sim_set_count(int count, const sim_inlet_t *inlet)
{
    if(count<0) count=0;
    if(count>PARTICLE_MAX) count=PARTICLE_MAX;
    if(count==s_count) return;
    while(s_count>count) {
        int mouth=0;
        float best=1e12f;
        for(int i=0;i<s_count;i++) {
            const float dx=s_p->pos[i][0]-MOUTH_X, dy=s_p->pos[i][1]-MOUTH_Y, d2=dx*dx+dy*dy;
            if(d2<best) { best=d2; mouth=i; }
        }
        --s_count;
        for(int a=0;a<3;a++) {
            s_p->pos[mouth][a]=s_p->pos[s_count][a];
            s_p->vel[mouth][a]=s_p->vel[s_count][a];
            s_p->old[mouth][a]=s_p->old[s_count][a];
            s_rgb[mouth][a]=s_rgb[s_count][a];
        }
    }
    while(s_count<count) {
        int i=s_count++;
        float x=inlet->x+(rand_unit()-0.5f)*10, y=inlet->y+(rand_unit()-0.5f)*6;
        const float dx=x-BOX_W*0.5f, dy=y-BOX_H*0.5f, d=sqrtf(dx*dx+dy*dy), limit=FLUID_RADIUS-WALL_MARGIN-1;
        if(d>limit) { x=BOX_W*0.5f+dx*limit/d; y=BOX_H*0.5f+dy*limit/d; }
        s_p->pos[i][0]=x;
        s_p->pos[i][1]=y;
        s_p->pos[i][2]=25+rand_unit()*24;
        s_p->vel[i][0]=inlet->vx;
        s_p->vel[i][1]=inlet->vy;
        s_p->vel[i][2]=0;
        for(int a=0;a<3;a++) {
            s_p->old[i][a]=s_p->pos[i][a];
            s_rgb[i][a]=(uint16_t)(inlet->rgb[a]<<8);
        }
        s_density[i]=0;
    }
    publish();
}

void sim_set_mix(const uint8_t rgb[3], bool immediate)
{
    for(int a=0;a<3;a++) s_mix[a]=(uint16_t)(rgb[a]<<8);
    if(immediate) for(int i=0;i<s_count;i++) for(int a=0;a<3;a++) s_rgb[i][a]=s_mix[a];
}

void sim_set_outlet(float accel)
{
    s_outlet_accel=accel>0?accel:0;
}
