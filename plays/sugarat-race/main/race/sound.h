/* SPDX-License-Identifier: Apache-2.0 */
/** Speaker output for the game: owns I2S0 TX and the amplifier pin while the application is open. */
#pragma once
#include "esp_err.h"
#include "synth.h"
/**
 * Claim the speaker and start the 10 ms synthesis task; playback follows the control-centre volume.
 * @returns ESP_OK; ESP_ERR_INVALID_STATE when already running; ESP_ERR_NO_MEM; or the I2S driver error.
 */
esp_err_t race_sound_start(void);
/** Fade out, switch the amplifier off and release I2S0; waits up to 500 ms for the task. */
void race_sound_stop(void);
/**
 * Replace the continuous layer targets.
 * @param params Copied before return.
 */
void race_sound_set(const race_synth_params *params);
/**
 * Queue a one-shot effect without blocking; a full queue drops it.
 * @param sfx Effect.
 * @param strength 0..1 for impact effects.
 */
void race_sound_play(race_sfx sfx,float strength);
