/* SPDX-License-Identifier: Apache-2.0 */
/** Three media views share one backend; visual state follows confirmed provider snapshots. */
#include "sugarat_media.h"
#include "media_copy.h"
#include "sugarat_watch.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum { ACT_PREV_PAGE = 1, ACT_NEXT_PAGE, ACT_PLAY, ACT_PREV_TRACK, ACT_NEXT_TRACK, ACT_MUTE, ACT_REPEAT, ACT_LANGUAGE };
typedef struct {
    media_config_t config;
    const media_copy_t *copy;
    media_snapshot_t state;
    lv_obj_t *screen, *content, *title, *detail, *status, *times, *play_button, *play_icon;
    lv_obj_t *progress, *volume, *volume_label, *mute_label, *repeat_label;
    lv_timer_t *timer;
    uint32_t request_id, drag_ms;
    uint8_t saved_volume;
    bool dirty, repeat, dragging, drag_playing, paused;
} media_app_t;
static void render(media_app_t *app);

static lv_obj_t *text(media_app_t *app, const char *value, sw_font_t font, sw_tone_t tone, int y)
{
    lv_obj_t *label = sw_label(app->content, value, font, tone);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}
static lv_obj_t *control(media_app_t *app, int action, sw_icon_t icon, const char *value,
                         int x, int y, int w, int h, bool filled)
{
    lv_obj_t *obj = sw_button(app->content, icon, value, filled ? SW_BUTTON_FILLED : SW_BUTTON_PLAIN, SW_TONE_TEAL);
    sw_set_action(obj, action);
    lv_obj_set_style_min_width(obj, 0, 0);
    lv_obj_set_style_pad_hor(obj, 0, 0);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    if (value && h < 32) lv_obj_set_style_text_font(lv_obj_get_child(obj, icon == SW_ICON_NONE ? 0 : 1), sw_font(SW_FONT_CAPTION), 0);
    return obj;
}
static bool playing(media_app_t *app)
{
    return app->state.phase == MEDIA_PLAYING || app->state.phase == MEDIA_STARTING;
}
static void request(media_app_t *app, uint32_t track, uint32_t position, bool play)
{
    media_request_t command = {.id = ++app->request_id, .track = track, .position_ms = position, .play = play};
    app->state.id = command.id;
    app->state.track = track;
    app->state.position_ms = position;
    app->state.error = MEDIA_ERROR_NONE;
    app->state.phase = play ? MEDIA_STARTING : MEDIA_STOPPING;
    if (!app->config.backend.request(app->config.backend.context, command)) {
        app->state.phase = MEDIA_FAILED;
        media_snapshot_t failed = app->config.backend.read(app->config.backend.context);
        app->state.error = failed.error == MEDIA_ERROR_NONE ? MEDIA_ERROR_DEVICE : failed.error;
    }
    fprintf(stderr, "[media] request=%" PRIu32 " track=%" PRIu32 " position=%" PRIu32 " play=%d\n",
            command.id, track, position, play);
}
static void refresh(media_app_t *app)
{
    const media_copy_t *c = app->copy;
    uint32_t duration = app->config.durations_ms[app->state.track];
    uint32_t pos = app->dragging ? app->drag_ms : app->state.position_ms;
    lv_label_set_text(app->title, c->tracks[app->state.track]);
    lv_label_set_text(app->detail, app->config.silent_preview ? c->preview : c->original);
    const char *status = app->state.phase == MEDIA_FAILED ?
        (app->state.error == MEDIA_ERROR_DEVICE ? c->device_error : c->timeout_error) : c->states[app->state.phase];
    lv_label_set_text(app->status, status);
    lv_label_set_text_fmt(app->times, "%02" PRIu32 ":%02" PRIu32 " / %02" PRIu32 ":%02" PRIu32,
                         pos / 60000, pos / 1000 % 60, duration / 60000, duration / 1000 % 60);
    sw_icon_set(app->play_icon, playing(app) ? SW_ICON_PAUSE : SW_ICON_PLAY, SW_TONE_BG);
    if (app->state.phase == MEDIA_STARTING || app->state.phase == MEDIA_STOPPING ||
        (app->state.phase == MEDIA_FAILED && app->state.error != MEDIA_ERROR_DEVICE))
        lv_obj_add_state(app->play_button, LV_STATE_DISABLED);
    else lv_obj_clear_state(app->play_button, LV_STATE_DISABLED);
    if (app->progress && !app->dragging) lv_slider_set_value(app->progress, (int32_t)(pos * 1000 / duration), LV_ANIM_OFF);
    if (app->volume) lv_slider_set_value(app->volume, app->state.volume, LV_ANIM_OFF);
    if (app->volume_label) lv_label_set_text_fmt(app->volume_label, c->volume, app->state.volume);
    if (app->mute_label) lv_label_set_text(app->mute_label, app->state.volume ? c->mute : c->unmute);
    if (app->repeat_label) lv_label_set_text(app->repeat_label, app->repeat ? c->repeat : c->once);
    lv_obj_align(app->title, LV_ALIGN_TOP_MID, 0, 69);
    lv_obj_align(app->detail, LV_ALIGN_TOP_MID, 0, 98);
    lv_obj_align(app->status, LV_ALIGN_TOP_MID, 0, app->config.scene == MEDIA_SCENE_CONTROLS ? 119 :
                 app->config.scene == MEDIA_SCENE_COVER ? 228 : 195);
    lv_obj_align(app->times, LV_ALIGN_TOP_MID, 0, app->config.scene == MEDIA_SCENE_CONTROLS ? 160 : 231);
    if (app->volume_label) lv_obj_align(app->volume_label, LV_ALIGN_TOP_MID, 0, 184);
}
static void progress_event(lv_event_t *event)
{
    media_app_t *app = lv_event_get_user_data(event);
    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        app->dragging = true; app->drag_ms = app->state.position_ms; app->drag_playing = playing(app); break;
    case LV_EVENT_VALUE_CHANGED:
        app->drag_ms = (uint32_t)lv_slider_get_value(app->progress) * app->config.durations_ms[app->state.track] / 1000;
        break;
    case LV_EVENT_RELEASED:
        app->drag_ms = (uint32_t)lv_slider_get_value(app->progress) * app->config.durations_ms[app->state.track] / 1000;
        app->dragging = false;
        request(app, app->state.track, app->drag_ms, app->drag_playing);
        break;
    case LV_EVENT_PRESS_LOST: app->dragging = false; break;
    default: return;
    }
    refresh(app);
}
static void volume_event(lv_event_t *event)
{
    media_app_t *app = lv_event_get_user_data(event);
    uint8_t volume = (uint8_t)lv_slider_get_value(app->volume);
    if (app->config.backend.volume(app->config.backend.context, volume)) {
        app->state.volume = volume;
        if (volume) app->saved_volume = volume;
    } else { app->state.phase = MEDIA_FAILED; app->state.error = MEDIA_ERROR_DEVICE; }
    refresh(app);
}
static void clicked(lv_event_t *event)
{
    media_app_t *app = lv_event_get_user_data(event);
    int action = sw_event_action(event);
    switch (action) {
    case ACT_PREV_PAGE: case ACT_NEXT_PAGE:
        app->config.scene = (media_scene_t)((app->config.scene + (action == ACT_NEXT_PAGE ? 1 : 2)) % 3);
        app->dirty = true;
        break;
    case ACT_PLAY:
        request(app, app->state.track, app->state.phase == MEDIA_DONE ? 0 : app->state.position_ms, !playing(app));
        break;
    case ACT_PREV_TRACK: case ACT_NEXT_TRACK: {
        uint32_t next = (app->state.track + (action == ACT_NEXT_TRACK ? 1 : app->config.track_count - 1)) % app->config.track_count;
        request(app, next, 0, playing(app)); app->dirty = true; break;
    }
    case ACT_MUTE: {
        if (app->state.volume) app->saved_volume = app->state.volume;
        uint8_t volume = app->state.volume ? 0 : app->saved_volume;
        if (app->config.backend.volume(app->config.backend.context, volume)) app->state.volume = volume;
        else { app->state.phase = MEDIA_FAILED; app->state.error = MEDIA_ERROR_DEVICE; }
        break;
    }
    case ACT_REPEAT: app->repeat = !app->repeat; break;
    case ACT_LANGUAGE:
        app->config.locale = app->config.locale == MEDIA_LOCALE_EN ? MEDIA_LOCALE_ZH : MEDIA_LOCALE_EN;
        app->copy = app->config.locale == MEDIA_LOCALE_EN ? &media_copy_en : &media_copy_zh;
        app->dirty = true; break;
    default: return;
    }
    if (!app->dirty) refresh(app);
    fprintf(stderr, "[media] action=%d scene=%d\n", action, app->config.scene);
}
static void render(media_app_t *app)
{
    lv_obj_clean(app->content);
    app->progress = app->volume = app->volume_label = app->mute_label = app->repeat_label = NULL;
    control(app, ACT_LANGUAGE, SW_ICON_NONE, app->copy->language, 156, 38, 48, 24, false);
    if (app->config.gallery_navigation) {
        control(app, ACT_PREV_PAGE, SW_ICON_LEFT, NULL, 64, 64, 40, 40, false);
        control(app, ACT_NEXT_PAGE, SW_ICON_RIGHT, NULL, 256, 64, 40, 40, false);
    }
    app->title = text(app, "", SW_FONT_HEADLINE, SW_TONE_TEAL, 69);
    app->detail = text(app, "", SW_FONT_CAPTION, SW_TONE_MUTED, 98);
    app->status = text(app, "", SW_FONT_CAPTION, SW_TONE_TEXT, 195);
    app->times = text(app, "", SW_FONT_CAPTION, SW_TONE_MUTED, 231);
    if (app->config.scene != MEDIA_SCENE_CONTROLS) {
        static const sw_tone_t tones[] = {SW_TONE_ORANGE, SW_TONE_TEAL, SW_TONE_PURPLE};
        static const sw_icon_t icons[] = {SW_ICON_SUN, SW_ICON_MUSIC, SW_ICON_MOON};
        lv_obj_t *cover = sw_card(app->content);
        lv_obj_set_layout(cover, 0);
        lv_obj_set_size(cover, app->config.scene == MEDIA_SCENE_COVER ? 152 : 96,
                        app->config.scene == MEDIA_SCENE_COVER ? 104 : 70);
        lv_obj_set_style_bg_color(cover, sw_color(tones[app->state.track]), 0);
        lv_obj_align(cover, LV_ALIGN_TOP_MID, 0, 122);
        lv_obj_t *icon = sw_icon(cover, icons[app->state.track], SW_SIZE_XL, SW_TONE_BG);
        lv_obj_center(icon);
    }
    if (app->config.scene != MEDIA_SCENE_COVER) {
        app->progress = sw_slider(app->content, 0, 1000, 0, SW_TONE_TEAL);
        lv_obj_set_size(app->progress, 200, 8);
        lv_obj_set_pos(app->progress, 80, app->config.scene == MEDIA_SCENE_CONTROLS ? 145 : 218);
        lv_obj_add_event_cb(app->progress, progress_event, LV_EVENT_PRESSED, app);
        lv_obj_add_event_cb(app->progress, progress_event, LV_EVENT_VALUE_CHANGED, app);
        lv_obj_add_event_cb(app->progress, progress_event, LV_EVENT_RELEASED, app);
        lv_obj_add_event_cb(app->progress, progress_event, LV_EVENT_PRESS_LOST, app);
    }
    if (app->config.scene == MEDIA_SCENE_CONTROLS) {
        app->volume_label = text(app, "", SW_FONT_BODY, SW_TONE_TEAL, 184);
        app->volume = sw_slider(app->content, 0, 100, app->state.volume, SW_TONE_TEAL);
        lv_obj_set_size(app->volume, 200, 8); lv_obj_set_pos(app->volume, 80, 214);
        lv_obj_add_event_cb(app->volume, volume_event, LV_EVENT_VALUE_CHANGED, app);
        lv_obj_t *repeat = control(app, ACT_REPEAT, SW_ICON_NONE, "", 110, 293, 140, 28, false);
        app->repeat_label = lv_obj_get_child(repeat, 0);
    } else {
        lv_obj_t *mute = control(app, ACT_MUTE, SW_ICON_NONE, "", 126, 307, 108, 28, false);
        app->mute_label = lv_obj_get_child(mute, 0);
    }
    int y = app->config.scene == MEDIA_SCENE_CONTROLS ? 239 : 253;
    control(app, ACT_PREV_TRACK, SW_ICON_PREV, NULL, 80, y, 52, 44, false);
    app->play_button = control(app, ACT_PLAY, SW_ICON_PLAY, NULL, 154, y, 52, 44, true);
    app->play_icon = lv_obj_get_child(app->play_button, 0);
    if (app->config.scene == MEDIA_SCENE_COVER) lv_obj_add_flag(app->times, LV_OBJ_FLAG_HIDDEN);
    control(app, ACT_NEXT_TRACK, SW_ICON_NEXT, NULL, 228, y, 52, 44, false);
    refresh(app);
    fprintf(stderr, "[media] render=%d locale=%s track=%" PRIu32 "\n", app->config.scene,
            app->config.locale == MEDIA_LOCALE_EN ? "en" : "zh", app->state.track);
}
static void tick(lv_timer_t *timer)
{
    media_app_t *app = timer->user_data;
    if (app->paused) return;
    media_snapshot_t snapshot = app->config.backend.read(app->config.backend.context);
    if (snapshot.id == app->state.id) {
        bool completed = snapshot.phase == MEDIA_DONE && app->state.phase != MEDIA_DONE;
        app->state = snapshot;
        if (completed && app->repeat) request(app, app->state.track, 0, true);
    }
    if (app->dirty) { app->dirty = false; render(app); } else refresh(app);
}
static void deleted(lv_event_t *event)
{
    media_app_t *app = lv_event_get_user_data(event);
    if (lv_event_get_target(event) != app->screen) return;
    request(app, app->state.track, app->state.position_ms, false);
    lv_timer_del(app->timer); lv_mem_free(app);
}
lv_obj_t *media_open(media_config_t config)
{
    media_app_t *app = lv_mem_alloc(sizeof(*app)); LV_ASSERT_MALLOC(app);
    memset(app, 0, sizeof(*app)); app->config = config;
    app->copy = config.locale == MEDIA_LOCALE_EN ? &media_copy_en : &media_copy_zh;
    app->state = config.backend.read(config.backend.context); app->request_id = app->state.id;
    app->saved_volume = app->state.volume ? app->state.volume : config.unmute_volume;
    sw_init(); app->screen = sw_screen(); lv_obj_set_user_data(app->screen, app);
    app->content = sw_column(app->screen); lv_obj_set_layout(app->content, 0); lv_obj_set_size(app->content, 360, 360);
    lv_obj_add_flag(app->content, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(app->screen, clicked, LV_EVENT_CLICKED, app);
    app->timer = lv_timer_create(tick, 50, app);
    lv_obj_add_event_cb(app->screen, deleted, LV_EVENT_DELETE, app); render(app);
    fprintf(stderr, "[media] Sugarat Media ready; audio=%s\n", config.silent_preview ? "silent-preview" : "device");
    return app->screen;
}
void media_set_paused(lv_obj_t *screen, bool paused)
{
    media_app_t *app = lv_obj_get_user_data(screen); app->paused = paused;
    if (paused) request(app, app->state.track, app->state.position_ms, false);
}
