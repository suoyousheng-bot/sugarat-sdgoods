/* SPDX-License-Identifier: Apache-2.0 */
/** Deterministic producer/consumer interleavings exercise real frame ownership and cadence. */
#include "fluid/frames.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint16_t a[2] = {0}, b[2] = {0}, c[2] = {0};
    fluid_frames_t frames = {.rendering = a, .ready = b, .visible = c};
    sugarat_frame_pacer_t pacer;
    sugarat_frame_pacer_init(&pacer, 33333);
    assert(!fluid_frames_present(&frames, &pacer, 100));
    assert(pacer.next_us == 0);
    frames.rendering[0] = 11;
    frames.rendering[1] = 11;
    fluid_frames_publish(&frames);
    assert(fluid_frames_present(&frames, &pacer, 1000));
    assert(frames.visible[0] == 11 && frames.visible[1] == 11);
    uint16_t *first_visible = frames.visible;

    /* An unfinished producer frame never changes the retained display frame. */
    frames.rendering[0] = 22;
    assert(!fluid_frames_present(&frames, &pacer, 40000));
    assert(frames.visible == first_visible && frames.visible[0] == 11 && frames.visible[1] == 11);
    frames.rendering[1] = 22;
    fluid_frames_publish(&frames);
    assert(fluid_frames_present(&frames, &pacer, 40001));
    assert(frames.visible[0] == 22 && frames.visible[1] == 22);

    /* A slow consumer takes the newest completed frame, without a backlog. */
    for (uint16_t value = 30; value < 60; value++) {
        frames.rendering[0] = value;
        frames.rendering[1] = value;
        fluid_frames_publish(&frames);
        assert(frames.visible[0] == 22 && frames.visible[1] == 22);
        assert(frames.rendering != frames.ready && frames.ready != frames.visible && frames.rendering != frames.visible);
    }
    assert(!fluid_frames_present(&frames, &pacer, 50000));
    assert(fluid_frames_present(&frames, &pacer, 70000));
    assert(frames.visible[0] == 59 && frames.visible[1] == 59);
    assert(!fluid_frames_present(&frames, &pacer, 71000));
    puts("PASS: no incomplete display frames, newest-frame delivery and no empty-slot reservation.");
    return 0;
}
