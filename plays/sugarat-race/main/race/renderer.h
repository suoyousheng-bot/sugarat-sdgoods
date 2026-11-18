#ifndef ROUND_RACE_RENDERER_H
#define ROUND_RACE_RENDERER_H
#include <stdint.h>
#include <stdbool.h>

/* RGB565 frame in row-major order. The caller owns 360 * 360 pixels. */
enum { RR_WIDTH = 360, RR_HEIGHT = 360 };
/* Racing rivals drawn in single-player races and fixed boost pads per lap. */
enum { RR_RIVALS = 3, RR_BOOST_PADS = 4 };
/* The player car is drawn this far ahead of the camera distance; collisions and pads use the car position. */
#define RR_CAR_AHEAD_M 3.3f
#define RR_PAD_LENGTH_M 7.0f
#define RR_PAD_HALF_WIDTH_M 1.1f
typedef struct {
    int mode; /* 0: road race; 1: marble maze */
    int map;
    float distance_m;
    float lateral_m;
    float opponent_distance_m;
    int elapsed_ms;
    int speed_kmh;
    int marble_x, marble_y;
    int opponent_x, opponent_y;
    int checkpoints;
    bool hide_hud, live_hud, opponent_visible;
    int players, rank;
    unsigned checkpoint_mask;
    /* Live racing only (live_hud): laps 0 hides the lap counter; lap_flash_ms shows the lap banner. */
    int lap, laps, lap_flash_ms;
    int rival_count;
    float rival_distance_m[RR_RIVALS], rival_lateral_m[RR_RIVALS];
    float boost_s; /* remaining boost; draws speed streaks */
} rr_state;

/* Draws the supplied state; does not advance physics, read inputs or use radios. */
void rr_render(uint16_t *frame, const rr_state *state);
/* Optional caller-owned 360 * 360 buffer that keeps the static maze between frames; NULL disables it. */
void rr_set_maze_cache(uint16_t *buffer);
/* Supplies the fixed state used by the six published reference PNGs. */
rr_state rr_reference_state(int mode, int map);
/** Lap distance in metres; map is 0..2. Racing distance accumulates across laps. */
float rr_track_length(int map);
/** Signed change in tangent per metre near distance. */
float rr_track_curvature(int map, float distance);
/** Boost pad index 0..RR_BOOST_PADS-1: start distance within one lap and lateral centre in metres. */
void rr_boost_pad(int map, int index, float *distance, float *lateral);
/** True for a passable point in the fixed 17x17 maze. */
bool rr_maze_open(int map, int x, int y);
/** Point index: 0 start, 1 finish, 2..4 checkpoints. */
void rr_maze_point(int map, int index, int *x, int *y);
#endif
