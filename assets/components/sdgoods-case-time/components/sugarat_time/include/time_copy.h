/* SPDX-License-Identifier: Apache-2.0 */
/** Paired UI text; both locales share one application and identical timer behavior. */
#ifndef SUGARAT_TIME_COPY_H
#define SUGARAT_TIME_COPY_H
typedef enum { TIME_LOCALE_ZH, TIME_LOCALE_EN } time_locale_t;
typedef struct {
    const char *titles[4];
    const char *ready, *running, *paused, *done, *focus, *rest;
    const char *focus_detail, *countdown_detail, *lap_detail, *lap_hint;
    const char *seconds, *clock_unset, *clock_detail;
    const char *clock_columns, *duration_columns, *set_time, *set_duration;
    const char *cancel, *save, *reset, *lap, *start, *pause;
    const char *focus_presets[3], *countdown_presets[3];
    const char *rest_complete, *focus_complete, *countdown_complete, *invalid_duration;
} time_copy_t;
extern const time_copy_t time_copy_zh;
extern const time_copy_t time_copy_en;
#endif
