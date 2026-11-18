#pragma once
#include <stdint.h>
static inline int64_t esp_timer_get_time(void){extern uint64_t host_time_ms;return (int64_t)host_time_ms*1000;}
