/* SPDX-License-Identifier: Apache-2.0 */
/** Paired music titles, transport states and errors. */
#ifndef SUGARAT_MEDIA_COPY_H
#define SUGARAT_MEDIA_COPY_H
#include "sugarat_media.h"
typedef struct {
    const char *tracks[3], *views[3], *states[6];
    const char *language, *original, *preview, *volume, *mute, *unmute, *repeat, *once;
    const char *device_error, *timeout_error;
} media_copy_t;
extern const media_copy_t media_copy_zh;
extern const media_copy_t media_copy_en;
#endif
