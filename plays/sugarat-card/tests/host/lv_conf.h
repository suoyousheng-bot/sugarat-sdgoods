#pragma once
#define LV_CONF_H
#include <stdio.h>
#include <stdlib.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64 * 1024)
#define LV_USE_LOG 0
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_NULL 1
#define LV_ASSERT_HANDLER fprintf(stderr,"LVGL assertion %s:%d\n",__FILE__,__LINE__); abort();
#define LV_USE_GIF 0
#define LV_USE_PNG 0
#define LV_USE_SNAPSHOT 1
#define LV_USE_QRCODE 1
