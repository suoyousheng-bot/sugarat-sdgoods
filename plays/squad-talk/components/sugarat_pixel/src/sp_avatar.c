/* SPDX-License-Identifier: Apache-2.0 */
/** Pixel character widget: run-length rectangle drawing and a 100 ms animation clock. */
#include "sugarat_pixel.h"

typedef struct {
    sp_char_id_t id;
    sp_mood_t mood;
    uint8_t scale, frame;
    bool animated;
    uint32_t tick;
    lv_timer_t *timer;
} avatar_t;

const sp_char_t *sp_char_get(sp_char_id_t id)
{
    return id < SP_CHAR_COUNT ? &sp_characters[id] : NULL;
}

static uint8_t pixel(const uint8_t *frame, int x, int y)
{
    uint8_t pair = frame[(y * SP_SPRITE_SIZE + x) / 2];
    return x & 1 ? pair & 0x0F : pair >> 4;
}

static void on_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const avatar_t *a = lv_obj_get_user_data(obj);
    const sp_char_t *c = &sp_characters[a->id];
    const uint8_t *frame = c->frames[a->frame];
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t box;
    lv_obj_get_coords(obj, &box);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    const int s = a->scale;
    for (int y = 0; y < SP_SPRITE_SIZE; ++y) {
        lv_coord_t y1 = box.y1 + y * s;
        if (y1 > ctx->clip_area->y2 || y1 + s - 1 < ctx->clip_area->y1) continue;
        for (int x = 0; x < SP_SPRITE_SIZE;) {
            uint8_t index = pixel(frame, x, y);
            int start = x++;
            while (x < SP_SPRITE_SIZE && pixel(frame, x, y) == index) ++x;
            if (!index) continue;
            lv_area_t run = {box.x1 + start * s, y1, box.x1 + x * s - 1, y1 + s - 1};
            dsc.bg_color = lv_color_hex(c->palette[index]);
            lv_draw_rect(ctx, &dsc, &run);
        }
    }
}

static void show(lv_obj_t *obj, avatar_t *a, uint8_t frame, int offset)
{
    if (a->frame != frame) {
        a->frame = frame;
        lv_obj_invalidate(obj);
    }
    if (lv_obj_get_style_translate_y(obj, 0) != offset * a->scale) lv_obj_set_style_translate_y(obj, offset * a->scale, 0);
}

static void step(lv_obj_t *obj, avatar_t *a)
{
    uint32_t t = a->tick;
    uint8_t frame = (uint8_t)a->mood;
    int offset = 0;
    if (a->animated) {
        switch (a->mood) {
            case SP_MOOD_IDLE:
                offset = (t / 6) % 2;
                if (t % 42 == 30 || t % 42 == 31) frame = SP_FRAME_BLINK;
                break;
            case SP_MOOD_TALK:
                offset = (t / 6) % 2;
                if ((t / 2) % 2) frame = SP_MOOD_IDLE;
                break;
            case SP_MOOD_HAPPY: offset = t % 6 < 2 ? -2 : 0; break;
            case SP_MOOD_LOVE: offset = t % 10 < 2 ? -1 : 0; break;
            case SP_MOOD_SLEEP: offset = (t / 12) % 2; break;
            case SP_MOOD_SAD: offset = (t / 10) % 2; break;
            case SP_MOOD_WINK: offset = (t / 6) % 2; break;
            case SP_MOOD_SURPRISED: offset = t % 30 < 2 ? -1 : 0; break;
            case SP_MOOD_COUNT: break;
        }
    }
    show(obj, a, frame, offset);
}

static void on_tick(lv_timer_t *timer)
{
    lv_obj_t *obj = timer->user_data;
    avatar_t *a = lv_obj_get_user_data(obj);
    ++a->tick;
    step(obj, a);
}

static void on_delete(lv_event_t *e)
{
    avatar_t *a = lv_obj_get_user_data(lv_event_get_target(e));
    lv_timer_del(a->timer);
    lv_mem_free(a);
}

lv_obj_t *sp_avatar(lv_obj_t *parent, sp_char_id_t id, uint8_t scale, sp_mood_t mood)
{
    avatar_t *a = lv_mem_alloc(sizeof(*a));
    LV_ASSERT_MALLOC(a);
    *a = (avatar_t){.id = id, .mood = mood, .scale = LV_CLAMP(1, scale, 10), .frame = (uint8_t)mood, .animated = true};
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(obj, SP_SPRITE_SIZE * a->scale, SP_SPRITE_SIZE * a->scale);
    lv_obj_set_user_data(obj, a);
    lv_obj_add_event_cb(obj, on_draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, on_delete, LV_EVENT_DELETE, NULL);
    a->timer = lv_timer_create(on_tick, 100, obj);
    return obj;
}

void sp_avatar_set_char(lv_obj_t *avatar, sp_char_id_t id)
{
    avatar_t *a = lv_obj_get_user_data(avatar);
    a->id = id;
    lv_obj_invalidate(avatar);
}

void sp_avatar_set_mood(lv_obj_t *avatar, sp_mood_t mood)
{
    avatar_t *a = lv_obj_get_user_data(avatar);
    a->mood = mood;
    a->tick = 0;
    step(avatar, a);
}

void sp_avatar_set_animated(lv_obj_t *avatar, bool animated)
{
    avatar_t *a = lv_obj_get_user_data(avatar);
    a->animated = animated;
    step(avatar, a);
}
