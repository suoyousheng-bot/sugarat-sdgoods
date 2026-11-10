/* SPDX-License-Identifier: Apache-2.0 */
/** Screen-owned simulated switches, levels, scenes and remote buttons. */
#include "control_app.h"
#include "sugarat_watch.h"
#include <stdio.h>

enum { PREV = 1, NEXT, LANGUAGE, LAMP = 10, FAN, BRIGHTNESS, VOLUME,
       READING = 20, NIGHT, AWAY, UP = 30, LEFT, OK, RIGHT, DOWN };
#include "control_copy.h"
typedef struct {
    lv_obj_t *screen, *content, *value, *detail, *brightness_label, *volume_label;
    lv_timer_t *timer;
    const control_copy_t *copy;
    unsigned page, brightness, volume, count;
    int scene, direction;
    bool lamp, fan, dirty, gallery_navigation, paused;
} control_t;

static lv_obj_t *label(control_t *app, const char *value, int y, sw_font_t font, sw_tone_t tone)
{
    lv_obj_t *obj = sw_label(app->content, value, font, tone);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    return obj;
}
static lv_obj_t *button(control_t *app, const char *value, sw_icon_t icon, int action,
                        int x, int y, int w, int h, bool active)
{
    lv_obj_t *obj = sw_button(app->content, icon, value,
                             active ? SW_BUTTON_FILLED : SW_BUTTON_PLAIN, SW_TONE_TEAL);
    sw_set_action(obj, action);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_min_width(obj, 0, 0);
    lv_obj_set_style_pad_hor(obj, 0, 0);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    return obj;
}
static void update_values(control_t *app)
{
    const control_copy_t *c = app->copy;
    if (app->page == 1) {
        lv_label_set_text_fmt(app->brightness_label, "%s  %u%%", c->brightness, app->brightness);
        lv_label_set_text_fmt(app->volume_label, "%s  %u%%", c->volume, app->volume);
        lv_obj_align(app->brightness_label, LV_ALIGN_TOP_MID, 0, 132);
        lv_obj_align(app->volume_label, LV_ALIGN_TOP_MID, 0, 214);
    } else if (app->page == 3) {
        lv_label_set_text(app->value, app->direction < 0 ? c->idle : c->directions[app->direction]);
        lv_label_set_text_fmt(app->detail, "%s  %u", c->commands, app->count);
        lv_obj_align(app->value, LV_ALIGN_TOP_MID, 0, 122);
        lv_obj_align(app->detail, LV_ALIGN_TOP_MID, 0, 300);
    }
}
static void render(control_t *app)
{
    lv_obj_clean(app->content);
    const control_copy_t *c = app->copy;
    if (app->gallery_navigation) button(app, NULL, SW_ICON_LEFT, PREV, 64, 64, 40, 40, false);
    button(app, c->titles[app->page], SW_ICON_NONE, LANGUAGE, 108, 64, 144, 40, false);
    if (app->gallery_navigation) button(app, NULL, SW_ICON_RIGHT, NEXT, 256, 64, 40, 40, false);
    label(app, c->simulation, 101, SW_FONT_CAPTION, SW_TONE_MUTED);
    if (app->page == 0) {
        label(app, c->lamp, 139, SW_FONT_HEADLINE, SW_TONE_TEXT);
        button(app, app->lamp ? c->on : c->off, SW_ICON_NONE, LAMP, 112, 165, 136, 48, app->lamp);
        label(app, c->fan, 229, SW_FONT_HEADLINE, SW_TONE_TEXT);
        button(app, app->fan ? c->on : c->off, SW_ICON_NONE, FAN, 112, 255, 136, 48, app->fan);
    } else if (app->page == 1) {
        app->brightness_label = label(app, "", 132, SW_FONT_HEADLINE, SW_TONE_TEXT);
        app->volume_label = label(app, "", 214, SW_FONT_HEADLINE, SW_TONE_TEXT);
        lv_obj_t *slider = sw_slider(app->content, 0, 100, (int32_t)app->brightness, SW_TONE_YELLOW);
        lv_obj_set_pos(slider, 80, 184); lv_obj_set_width(slider, 200); sw_set_action(slider, BRIGHTNESS);
        slider = sw_slider(app->content, 0, 100, (int32_t)app->volume, SW_TONE_TEAL);
        lv_obj_set_pos(slider, 80, 266); lv_obj_set_width(slider, 200); sw_set_action(slider, VOLUME);
        update_values(app);
    } else if (app->page == 2) {
        for (int i = 0; i < 3; ++i)
            button(app, c->scenes[i], SW_ICON_NONE, READING + i, 104, 132 + 58 * i, 152, 48, app->scene == i);
        label(app, app->scene < 0 ? c->idle : c->scenes[app->scene], 310, SW_FONT_CAPTION, SW_TONE_TEAL);
    } else {
        app->value = label(app, "", 111, SW_FONT_CAPTION, SW_TONE_TEAL);
        const int xs[] = {156, 100, 156, 212, 156}, ys[] = {141, 194, 194, 194, 247};
        for (int i = 0; i < 5; ++i)
            button(app, c->directions[i], SW_ICON_NONE, UP + i, xs[i], ys[i], 48, 48, i == 2);
        app->detail = label(app, "", 300, SW_FONT_CAPTION, SW_TONE_MUTED);
        update_values(app);
    }
    fprintf(stderr, "[control-test] render=%u locale=%s lamp=%d fan=%d brightness=%u volume=%u scene=%d\n",
            app->page, c == &control_copy_en ? "en" : "zh", app->lamp, app->fan, app->brightness, app->volume, app->scene);
}
static void event(lv_event_t *e)
{
    control_t *app = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        if (lv_event_get_target(e) == app->screen) { lv_timer_del(app->timer); lv_mem_free(app); }
        return;
    }
    if (app->paused) return;
    lv_obj_t *target = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED && lv_obj_check_type(target, &lv_slider_class)) {
        int action = (int)(intptr_t)lv_obj_get_user_data(target);
        unsigned value = (unsigned)lv_slider_get_value(target);
        if (action == BRIGHTNESS) app->brightness = value;
        else if (action == VOLUME) app->volume = value;
        else return;
        app->scene = -1;
        update_values(app);
        fprintf(stderr, "[control-test] level=%d value=%u\n", action, value);
        return;
    }
    if (code != LV_EVENT_CLICKED) return;
    int action = sw_event_action(e);
    switch (action) {
    case PREV: if (!app->gallery_navigation) return; app->page = (app->page + 3) % 4; break;
    case NEXT: if (!app->gallery_navigation) return; app->page = (app->page + 1) % 4; break;
    case LANGUAGE: app->copy = app->copy == &control_copy_zh ? &control_copy_en : &control_copy_zh; break;
    case LAMP: app->lamp = !app->lamp; app->scene = -1; break;
    case FAN: app->fan = !app->fan; app->scene = -1; break;
    case READING: case NIGHT: case AWAY:
        app->scene = action - READING;
        app->lamp = action != AWAY; app->fan = action == READING;
        app->brightness = action == READING ? 80 : action == NIGHT ? 15 : 0;
        app->volume = action == READING ? 35 : 0;
        break;
    case UP: case LEFT: case OK: case RIGHT: case DOWN:
        app->direction = action - UP;
        if (app->count < 9999) ++app->count;
        update_values(app);
        fprintf(stderr, "[control-test] remote=%d count=%u\n", action, app->count);
        return;
    default: return;
    }
    app->dirty = true;
    fprintf(stderr, "[control-test] action=%d page=%u\n", action, app->page);
}
static void tick(lv_timer_t *timer)
{
    control_t *app = timer->user_data;
    if (app->dirty) { app->dirty = false; render(app); }
}
lv_obj_t *control_app_open(control_app_config_t config)
{
    if ((config.locale != CONTROL_LOCALE_ZH && config.locale != CONTROL_LOCALE_EN) ||
        config.scene < CONTROL_SCENE_SWITCHES || config.scene > CONTROL_SCENE_REMOTE) return NULL;
    sw_init();
    control_t *app = lv_mem_alloc(sizeof(*app));
    if (!app) return NULL;
    *app = (control_t){.copy = config.locale == CONTROL_LOCALE_EN ? &control_copy_en : &control_copy_zh,
        .page = (unsigned)config.scene, .gallery_navigation = config.gallery_navigation,
        .brightness = 60, .volume = 30, .scene = -1, .direction = -1};
    app->screen = sw_screen();
    lv_obj_set_user_data(app->screen, app);
    app->content = lv_obj_create(app->screen);
    lv_obj_remove_style_all(app->content);
    lv_obj_set_size(app->content, 360, 360);
    lv_obj_clear_flag(app->content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(app->content, LV_OBJ_FLAG_EVENT_BUBBLE);
    app->timer = lv_timer_create(tick, 30, app);
    lv_obj_add_event_cb(app->screen, event, LV_EVENT_ALL, app);
    render(app);
    return app->screen;
}

void control_app_set_paused(lv_obj_t *screen, bool paused)
{
    control_t *app = lv_obj_get_user_data(screen);
    app->paused = paused;
    if (paused) lv_timer_pause(app->timer);
    else { lv_timer_resume(app->timer); app->dirty = true; }
}
