/* SPDX-License-Identifier: Apache-2.0 */
/** Firmware-lifetime audio owner; queues never retain UI pointers. */
#ifndef SUGARAT_MEDIA_DEVICE_H
#define SUGARAT_MEDIA_DEVICE_H
#include "sugarat_media.h"
/** Initialize instead of BSP audio initialization; failures remain visible in backend status. */
void media_device_init(void);
/** Return the initialized device backend; its static context and PCM buffers outlive all screens. */
media_backend_t media_device_backend(void);
#endif
