/* SPDX-License-Identifier: Apache-2.0 */
/** Text fields shared by both control-case languages. */
#ifndef SUGARAT_CONTROL_COPY_H
#define SUGARAT_CONTROL_COPY_H
typedef struct {
    const char *titles[4], *simulation, *lamp, *fan, *on, *off, *brightness, *volume;
    const char *scenes[3], *directions[5], *idle, *commands;
} control_copy_t;
extern const control_copy_t control_copy_zh;
extern const control_copy_t control_copy_en;
#endif
