/** Single-track speaker playback and its DMA clock. */
#pragma once

#include <stdint.h>

typedef enum { BEAT_AUDIO_WAITING, BEAT_AUDIO_PLAYING, BEAT_AUDIO_DONE, BEAT_AUDIO_ERROR } beat_audio_state_t;

/** Prepare the speaker task. Playback reports initialization errors to the screen. */
void beat_audio_init(void);

/** Request playback from a previously reported position and return its request ID. */
uint32_t beat_audio_start(uint32_t position_ms);

/** Schedule the chirp in a run at the given music position. Late requests use the next writable block. */
void beat_audio_queue_chirp(uint32_t id, uint32_t position_ms);

/** Cancel the current request without blocking the display thread. */
void beat_audio_stop(void);

/** Read transmitted song position for one request. Waiting includes amplifier warm-up. */
beat_audio_state_t beat_audio_position(uint32_t id, uint32_t *position_ms);
