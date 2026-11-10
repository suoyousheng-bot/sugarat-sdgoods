/* SPDX-License-Identifier: Apache-2.0 */
/** Paired companion text, including every character name and voice state. */
#ifndef SUGARAT_COMPANION_COPY_H
#define SUGARAT_COMPANION_COPY_H
#include "companion_model.h"
typedef enum { SC_LOCALE_ZH, SC_LOCALE_EN } sc_locale_t;
typedef struct {
    const char *characters[SP_CHAR_COUNT], *moods[SP_MOOD_COUNT], *voice[5];
    const char *language;
    const char *pet, *play, *sleep, *wake, *mood, *animate, *still;
    const char *pet_detail, *mood_detail, *demo_detail, *external_detail;
    const char *hold, *listening, *thinking, *speaking, *offline, *go_offline, *go_online;
} sc_copy_t;
extern const sc_copy_t sc_copy_zh;
extern const sc_copy_t sc_copy_en;
#endif
