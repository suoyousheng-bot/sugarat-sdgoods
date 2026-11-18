/* SPDX-License-Identifier: MIT */
/** SDGoods circular FluidBox dimensions and solver coefficients. */
#pragma once
#define LCD_H_RES 360
#define LCD_V_RES 360
#define BOX_W 360.0f
#define BOX_H 360.0f
#define BOX_D 75.0f
#define FLUID_RADIUS 165.0f
#define PARTICLE_MAX 600
#define PARTICLE_COUNT 480
#define REST_SPACING 13.5f
#define SMOOTH_RADIUS 22.5f
#define TIME_SCALE 0.1f
#define SIM_DT_MAX 0.0022f
#define SUBSTEPS 1
#define K_PRESSURE 400000.0f
#define K_NEAR_PRESSURE 800000.0f
#define MAX_DISPLACEMENT 4.0f
#define WALL_JITTER 0.35f
#define VISC_SIGMA 45.0f
#define VISC_BETA 0.03f
#define WALL_RESTITUTION 0.25f
#define WALL_FRICTION 0.96f
#define ROTATION_GAIN 0.5f
#define BOX_FRONT_FILLET 6.0f
#define BOX_BACK_FILLET 24.0f
#define PROJ_FOCAL 260.0f
#define PX_PER_METER 10000.0f
#define GRAVITY_GAIN 2.2f
#define SHAKE_GAIN 1.6f
#define MOUTH_X 180.0f
#define MOUTH_Y 24.0f
#define OUTLET_RADIUS 110.0f
