/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define heap_caps_malloc(size,caps) malloc(size)
#define heap_caps_calloc(count,size,caps) calloc(count,size)
#define heap_caps_free free
