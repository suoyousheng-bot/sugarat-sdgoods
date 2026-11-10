/* Fixed-grid frame pacing and frame statistics. Pure C: no ESP-IDF dependency, usable in host tests. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Starts frames on a fixed grid of slot_us. A frame that overruns starts at the next free slot, so a
 *  frame costing between one and two slots runs at an even half rate instead of alternating gaps. */
typedef struct { int64_t slot_us, next_us; } sugarat_frame_pacer_t;

/** 16667 us matches a 60 Hz panel; a slower frame then runs at 30 fps. */
#define SUGARAT_FRAME_SLOT_60HZ_US 16667

static inline void sugarat_frame_pacer_init(sugarat_frame_pacer_t *p,int64_t slot_us){p->slot_us=slot_us;p->next_us=0;}

/** Restarts the grid at the next call, for example after the animated screen was hidden. */
static inline void sugarat_frame_pacer_reset(sugarat_frame_pacer_t *p){p->next_us=0;}

/** Returns true when a frame should start at now_us and reserves the following slot. */
static inline bool sugarat_frame_pacer_due(sugarat_frame_pacer_t *p,int64_t now_us)
{
    if(p->next_us && now_us<p->next_us)return false;
    p->next_us=p->next_us?p->next_us+p->slot_us:now_us+p->slot_us;
    return true;
}

/** Call after a frame that sugarat_frame_pacer_due() admitted. Skips the slots the frame overran;
 *  when nothing animates, keeps the next start at most one slot away. */
static inline void sugarat_frame_pacer_finish(sugarat_frame_pacer_t *p,int64_t done_us,bool animating)
{
    if(p->next_us<=done_us)p->next_us+=((done_us-p->next_us)/p->slot_us+1)*p->slot_us;
    if(!animating && p->next_us-done_us>p->slot_us)p->next_us=done_us+p->slot_us;
}

/** Counters for one reporting window. */
typedef struct {
    int64_t window_start_us,last_start_us,sum_us,max_us,gap_max_us;
    uint32_t frames,direct_frames;
} sugarat_frame_stats_t;

/** Summary of a finished window; frame times are per rendered frame. */
typedef struct { float fps;uint32_t frames,direct_frames;int64_t avg_us,max_us,gap_max_us; } sugarat_frame_summary_t;

/** Records one rendered frame. gap_max_us covers consecutive animated frames only; direct marks a
 *  frame that bypassed the LVGL refresh. */
static inline void sugarat_frame_stats_record(sugarat_frame_stats_t *s,int64_t start_us,int64_t done_us,bool animating,bool direct)
{
    int64_t cost=done_us-start_us;
    if(!s->window_start_us)s->window_start_us=start_us;
    s->frames++;s->direct_frames+=direct;s->sum_us+=cost;
    if(cost>s->max_us)s->max_us=cost;
    if(animating && s->last_start_us && start_us-s->last_start_us>s->gap_max_us)s->gap_max_us=start_us-s->last_start_us;
    s->last_start_us=animating?start_us:0;
}

/** Returns true and fills out once interval_us has elapsed since the window started, then starts a
 *  new window. */
static inline bool sugarat_frame_stats_window(sugarat_frame_stats_t *s,int64_t now_us,int64_t interval_us,sugarat_frame_summary_t *out)
{
    if(!s->window_start_us){s->window_start_us=now_us;return false;}
    int64_t elapsed=now_us-s->window_start_us;
    if(elapsed<interval_us)return false;
    out->fps=(float)(s->frames*1e6/(double)elapsed);out->frames=s->frames;out->direct_frames=s->direct_frames;
    out->avg_us=s->frames?s->sum_us/s->frames:0;out->max_us=s->max_us;out->gap_max_us=s->gap_max_us;
    int64_t last=s->last_start_us;
    *s=(sugarat_frame_stats_t){0};s->window_start_us=now_us;s->last_start_us=last;
    return true;
}

#ifdef __cplusplus
}
#endif
