/* SPDX-License-Identifier: Apache-2.0 */
/** Scene-owned bilingual strings shared by all data scenes. */
#ifndef SUGARAT_DATA_COPY_H
#define SUGARAT_DATA_COPY_H
typedef struct {
    const char *scenes[4], *statuses[5], *language, *demo, *external;
    const char *temperature, *humidity, *battery, *normal, *alarm, *acknowledged, *acknowledge;
    const char *range, *history, *empty, *threshold, *recovery, *monitoring, *waiting;
} sd_copy_t;
extern const sd_copy_t sd_copy_zh, sd_copy_en;
#endif
