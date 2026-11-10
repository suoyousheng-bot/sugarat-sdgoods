/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Sugarat Game gallery: one screen per component group, built only from the public API.
 * Read it as usage examples. The scenes also draw sugarat_pixel avatars (sdgoods-pixel-characters).
 * Call on the LVGL thread after sg_init(); load a returned screen with lv_scr_load().
 */
#ifndef SG_GALLERY_H
#define SG_GALLERY_H

#include "lvgl.h"

/** @returns The number of gallery scenes. */
int sg_gallery_count(void);

/** @returns A new screen for scene index, or NULL when index is out of range. */
lv_obj_t *sg_gallery_create(int index);

/** @returns The scene's short id, such as "settings", or NULL when index is out of range. */
const char *sg_gallery_id(int index);

#endif
