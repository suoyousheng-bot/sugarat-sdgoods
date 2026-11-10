/* Retry pacing for HTTPS requests. Pure C: no ESP-IDF dependency, usable in host tests. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Tracks when the next request to one service may start. */
typedef struct { int64_t until_ms;uint32_t failures,base_ms,max_ms; } sugarat_https_backoff_t;

/** base_ms is the first delay after a failure; each further failure doubles it up to max_ms. */
static inline void sugarat_https_backoff_init(sugarat_https_backoff_t *b,uint32_t base_ms,uint32_t max_ms)
{ b->until_ms=0;b->failures=0;b->base_ms=base_ms;b->max_ms=max_ms; }

static inline bool sugarat_https_backoff_ready(const sugarat_https_backoff_t *b,int64_t now_ms){return now_ms>=b->until_ms;}

/** Milliseconds until the next request may start; 0 when ready. */
static inline int64_t sugarat_https_backoff_wait_ms(const sugarat_https_backoff_t *b,int64_t now_ms)
{ return now_ms>=b->until_ms?0:b->until_ms-now_ms; }

/** Records one request outcome. transport_ok is false when no HTTP status arrived. A 429 waits
 *  retry_after_s when it is positive, otherwise like other failures; 5xx and transport failures wait
 *  the doubling delay; any other status clears the failure count. */
static inline void sugarat_https_backoff_record(sugarat_https_backoff_t *b,int64_t now_ms,bool transport_ok,int status,int retry_after_s)
{
    bool failed=!transport_ok || status==429 || status>=500;
    if(!failed){b->failures=0;b->until_ms=0;return;}
    uint32_t shift=b->failures<16?b->failures:16;
    uint64_t delay=(uint64_t)b->base_ms<<shift;
    if(delay>b->max_ms)delay=b->max_ms;
    if(transport_ok && status==429 && retry_after_s>0)delay=(uint64_t)retry_after_s*1000;
    b->until_ms=now_ms+(int64_t)delay;
    if(b->failures<UINT32_MAX)b->failures++;
}

#ifdef __cplusplus
}
#endif
