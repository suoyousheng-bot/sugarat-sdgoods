/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Clean a finished 16 kHz mono recording in place: remove DC and rumble, suppress stationary
 * noise estimated from the quietest frames, level speech toward a fixed loudness, and attenuate
 * pauses. Output peaks stay at or below 9000 so the badge speaker does not crackle; no sample is
 * clipped. Run it once per recording: processing already-processed audio raises its noise again.
 * Clips shorter than 0.25 s are left unchanged.
 * @param pcm Samples to replace with the processed audio.
 * @param samples Number of samples.
 * @returns false when working memory could not be allocated; pcm is then unchanged.
 */
bool voice_enhance(int16_t *pcm, size_t samples);
