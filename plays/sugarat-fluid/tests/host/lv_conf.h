#pragma once
#define LV_CONF_H
#include <stdlib.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_USE_LOG 0
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_NULL 1
#define LV_ASSERT_HANDLER abort();
#define LV_USE_GIF 0
#define LV_USE_PNG 0
#define LV_USE_SNAPSHOT 1
