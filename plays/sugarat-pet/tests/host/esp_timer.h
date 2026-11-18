#pragma once
#include <stdint.h>
extern uint64_t host_time_ms;
static inline int64_t esp_timer_get_time(void) { return (int64_t)host_time_ms * 1000; }
