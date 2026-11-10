/* SPDX-License-Identifier: Apache-2.0 */
/** Interactive pets, mood badges and explicitly labeled voice demos from existing components. */
#include "sugarat_companion.h"
#include "sugarat_watch.h"
#include "sugarat_pixel.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum { ACT_PREV = 1, ACT_NEXT, ACT_PET, ACT_PLAY, ACT_SLEEP, ACT_CHARACTER, ACT_MOOD, ACT_ANIMATE,
       ACT_OFFLINE, ACT_VOICE, ACT_LANGUAGE };
typedef struct { lv_obj_t *obj, *label; } control_t;
typedef struct {
    sc_config_t config;
    sc_model_t model;
    const sc_copy_t *copy;
    lv_obj_t *screen, *content, *avatar, *detail, *name, *status, *wave;
    control_t primary, secondary, footer;
    lv_timer_t *timer;
    bool dirty, paused, deleting, voice_requested;
} companion_t;

static void render(companion_t *app);

static lv_obj_t *label(companion_t *app, const char *value, sw_font_t font, sw_tone_t tone, int y)
{
    lv_obj_t *obj = sw_label(app->content, value, font, tone);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    return obj;
}

static control_t control(companion_t *app, const char *value, int action, int x, int y, int w, int h, bool filled)
{
    lv_obj_t *obj = sw_button(app->content, SW_ICON_NONE, NULL,
                             filled ? SW_BUTTON_FILLED : SW_BUTTON_PLAIN, SW_TONE_TEAL);
    lv_obj_set_style_min_width(obj, 0, 0);
    lv_obj_set_style_pad_hor(obj, 0, 0);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    sw_set_action(obj, action);
    lv_obj_t *text = sw_label(obj, value, h < 32 ? SW_FONT_CAPTION : SW_FONT_HEADLINE,
                              filled ? SW_TONE_BG : SW_TONE_TEXT);
    return (control_t){obj, text};
}

static void cancel_voice(companion_t *app)
{
    bool listening = app->voice_requested;
    app->voice_requested = false;
    sc_model_voice_set(&app->model, app->model.voice == SC_VOICE_OFFLINE ? SC_VOICE_OFFLINE : SC_VOICE_IDLE);
    if (listening && app->config.voice_mode == SC_VOICE_EXTERNAL)
        app->config.on_voice(false, app->config.user_data);
}

static void refresh(companion_t *app)
{
    sc_model_t *m = &app->model;
    const sc_copy_t *c = app->copy;
    sp_mood_t mood = m->pet_mood;
    lv_label_set_text(app->name, c->characters[app->config.character]);
    if (app->config.scene == SC_SCENE_PET) {
        lv_label_set_text_fmt(app->detail, c->pet_detail, m->interactions);
        lv_label_set_text(app->status, c->moods[mood]);
        lv_label_set_text(app->footer.label, m->sleeping ? c->wake : c->sleep);
    } else if (app->config.scene == SC_SCENE_MOODS) {
        mood = m->selected_mood;
        lv_label_set_text(app->detail, c->mood_detail);
        lv_label_set_text(app->status, c->moods[mood]);
        lv_label_set_text(app->secondary.label, m->animated ? c->still : c->animate);
    } else {
        static const sp_mood_t moods[] = {SP_MOOD_IDLE, SP_MOOD_SURPRISED, SP_MOOD_WINK, SP_MOOD_TALK, SP_MOOD_SAD};
        mood = moods[m->voice];
        lv_label_set_text(app->detail, app->config.voice_mode == SC_VOICE_DEMO ? c->demo_detail : c->external_detail);
        lv_label_set_text(app->status, c->voice[m->voice]);
        const char *const states[] = {c->hold, c->listening, c->thinking, c->speaking, c->offline};
        lv_label_set_text(app->primary.label, states[m->voice]);
        if (m->voice == SC_VOICE_OFFLINE) lv_obj_add_state(app->primary.obj, LV_STATE_DISABLED);
        else lv_obj_clear_state(app->primary.obj, LV_STATE_DISABLED);
        if (app->footer.label) lv_label_set_text(app->footer.label, m->voice == SC_VOICE_OFFLINE ? c->go_online : c->go_offline);
        sw_waveform_set_active(app->wave, !app->paused && (m->voice == SC_VOICE_LISTENING || m->voice == SC_VOICE_SPEAKING));
    }
    sp_avatar_set_mood(app->avatar, mood);
    sp_avatar_set_animated(app->avatar, m->animated && !app->paused);
    lv_obj_align(app->name, LV_ALIGN_TOP_MID, 0, 69);
    lv_obj_align(app->detail, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_align(app->status, LV_ALIGN_TOP_MID, 0, 239);
}

static void voice_input(lv_event_t *event)
{
    companion_t *app = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if (app->paused || app->config.scene != SC_SCENE_VOICE) return;
    if (code == LV_EVENT_PRESSED && !app->voice_requested && sc_model_voice_start(&app->model)) {
        app->voice_requested = true;
        if (app->config.voice_mode == SC_VOICE_EXTERNAL) app->config.on_voice(true, app->config.user_data);
    } else if (code == LV_EVENT_RELEASED && app->voice_requested) {
        app->voice_requested = false;
        sc_model_voice_end(&app->model, app->config.now_ms(), app->config.voice_mode == SC_VOICE_DEMO);
        if (app->config.voice_mode == SC_VOICE_EXTERNAL) app->config.on_voice(false, app->config.user_data);
    } else if (code == LV_EVENT_PRESS_LOST) cancel_voice(app);
    else return;
    refresh(app);
    fprintf(stderr, "[companion] voice=%d\n", app->model.voice);
}

static void clicked(lv_event_t *event)
{
    companion_t *app = lv_event_get_user_data(event);
    int action = sw_event_action(event);
    if (app->paused || action < ACT_PREV || action > ACT_LANGUAGE || action == ACT_VOICE) return;
    uint64_t now = app->config.now_ms();
    sc_model_tick(&app->model, now);
    switch (action) {
    case ACT_PREV: case ACT_NEXT:
        cancel_voice(app);
        app->config.scene = (sc_scene_t)((app->config.scene + (action == ACT_NEXT ? 1 : 2)) % 3);
        app->dirty = true;
        break;
    case ACT_PET: case ACT_PLAY: sc_model_interact(&app->model, now, action == ACT_PLAY); break;
    case ACT_SLEEP: sc_model_sleep(&app->model); break;
    case ACT_CHARACTER:
        app->config.character = (sp_char_id_t)((app->config.character + 1) % SP_CHAR_COUNT);
        sp_avatar_set_char(app->avatar, app->config.character);
        break;
    case ACT_MOOD: app->model.selected_mood = (sp_mood_t)((app->model.selected_mood + 1) % SP_MOOD_COUNT); break;
    case ACT_ANIMATE: app->model.animated = !app->model.animated; break;
    case ACT_OFFLINE:
        cancel_voice(app);
        sc_model_voice_set(&app->model, app->model.voice == SC_VOICE_OFFLINE ? SC_VOICE_IDLE : SC_VOICE_OFFLINE);
        break;
    case ACT_LANGUAGE:
        cancel_voice(app);
        app->config.locale = app->config.locale == SC_LOCALE_EN ? SC_LOCALE_ZH : SC_LOCALE_EN;
        app->copy = app->config.locale == SC_LOCALE_EN ? &sc_copy_en : &sc_copy_zh;
        app->dirty = true;
        break;
    default: return;
    }
    if (!app->dirty) refresh(app);
    fprintf(stderr, "[companion] action=%d scene=%d mood=%d interactions=%" PRIu32 "\n",
            action, app->config.scene, app->model.pet_mood, app->model.interactions);
}

static void render(companion_t *app)
{
    lv_obj_clean(app->content);
    app->primary = app->secondary = app->footer = (control_t){0};
    app->wave = NULL;
    if (app->config.gallery_navigation) {
        control_t prev = control(app, "", ACT_PREV, 64, 64, 40, 40, false);
        control_t next = control(app, "", ACT_NEXT, 256, 64, 40, 40, false);
        lv_obj_del(prev.label); lv_obj_del(next.label);
        sw_icon(prev.obj, SW_ICON_LEFT, SW_SIZE_M, SW_TONE_TEAL);
        sw_icon(next.obj, SW_ICON_RIGHT, SW_SIZE_M, SW_TONE_TEAL);
    }
    control(app, app->copy->language, ACT_LANGUAGE, 156, 38, 48, 24, false);
    app->name = label(app, "", SW_FONT_HEADLINE, SW_TONE_TEAL, 69);
    lv_obj_set_width(app->name, 148);
    lv_obj_set_style_text_align(app->name, LV_TEXT_ALIGN_CENTER, 0);
    sw_set_action(app->name, ACT_CHARACTER);
    lv_obj_add_flag(app->name, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_ext_click_area(app->name, 6);
    app->detail = label(app, "", SW_FONT_CAPTION, SW_TONE_MUTED, 100);
    app->status = label(app, "", SW_FONT_BODY, SW_TONE_TEAL, 239);
    lv_obj_t *portrait = sw_column(app->content);
    lv_obj_set_layout(portrait, 0);
    lv_obj_set_size(portrait, 120, 120);
    lv_obj_set_pos(portrait, 120, 114);
    app->avatar = sp_avatar(portrait, app->config.character, 3, SP_MOOD_IDLE);
    lv_obj_center(app->avatar);
    if (app->config.scene == SC_SCENE_PET) {
        sw_set_action(portrait, ACT_PET);
        lv_obj_add_flag(portrait, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
        app->primary = control(app, app->copy->pet, ACT_PET, 78, 260, 96, 40, true);
        app->secondary = control(app, app->copy->play, ACT_PLAY, 186, 260, 96, 40, false);
        app->footer = control(app, app->copy->sleep, ACT_SLEEP, 144, 307, 72, 28, false);
    } else if (app->config.scene == SC_SCENE_MOODS) {
        app->primary = control(app, app->copy->mood, ACT_MOOD, 78, 266, 96, 40, true);
        app->secondary = control(app, app->copy->still, ACT_ANIMATE, 186, 266, 96, 40, false);
    } else {
        app->wave = sw_waveform(app->content, 80, 16, SW_TONE_TEAL);
        lv_obj_set_pos(app->wave, 140, 220);
        app->primary = control(app, app->copy->hold, ACT_VOICE, 90, 266, 180, 40, true);
        lv_obj_add_event_cb(app->primary.obj, voice_input, LV_EVENT_PRESSED, app);
        lv_obj_add_event_cb(app->primary.obj, voice_input, LV_EVENT_RELEASED, app);
        lv_obj_add_event_cb(app->primary.obj, voice_input, LV_EVENT_PRESS_LOST, app);
        if (app->config.voice_mode == SC_VOICE_DEMO)
            app->footer = control(app, app->copy->go_offline, ACT_OFFLINE, 132, 312, 96, 26, false);
    }
    refresh(app);
    fprintf(stderr, "[companion] render=%d locale=%s mode=%s\n", app->config.scene,
            app->config.locale == SC_LOCALE_EN ? "en" : "zh", app->config.voice_mode == SC_VOICE_DEMO ? "demo" : "external");
}

static void tick(lv_timer_t *timer)
{
    companion_t *app = timer->user_data;
    if (app->paused) return;
    if (app->dirty) { app->dirty = false; render(app); }
    if (sc_model_tick(&app->model, app->config.now_ms())) {
        refresh(app);
        fprintf(stderr, "[companion] voice=%d mood=%d\n", app->model.voice, app->model.pet_mood);
    }
}

static void deleted(lv_event_t *event)
{
    companion_t *app = lv_event_get_user_data(event);
    if (lv_event_get_target(event) != app->screen) return;
    app->deleting = true;
    lv_timer_del(app->timer);
    cancel_voice(app);
    lv_mem_free(app);
}

lv_obj_t *sc_open(sc_config_t config)
{
    companion_t *app = lv_mem_alloc(sizeof(*app));
    LV_ASSERT_MALLOC(app);
    memset(app, 0, sizeof(*app));
    app->config = config;
    app->copy = config.locale == SC_LOCALE_EN ? &sc_copy_en : &sc_copy_zh;
    sc_model_init(&app->model, config.timing);
    if (config.voice_mode == SC_VOICE_EXTERNAL) sc_model_voice_set(&app->model, SC_VOICE_OFFLINE);
    sw_init();
    app->screen = sw_screen();
    lv_obj_set_user_data(app->screen, app);
    app->content = sw_column(app->screen);
    lv_obj_set_layout(app->content, 0);
    lv_obj_set_size(app->content, 360, 360);
    lv_obj_add_flag(app->content, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(app->screen, clicked, LV_EVENT_CLICKED, app);
    app->timer = lv_timer_create(tick, 50, app);
    lv_obj_add_event_cb(app->screen, deleted, LV_EVENT_DELETE, app);
    render(app);
    fprintf(stderr, "[companion] Sugarat Companion ready; no audio or network\n");
    return app->screen;
}

void sc_set_voice_state(lv_obj_t *screen, sc_voice_t state)
{
    companion_t *app = lv_obj_get_user_data(screen);
    if (app->deleting) return;
    sc_model_voice_set(&app->model, state);
    if (!app->paused && !app->dirty && app->config.scene == SC_SCENE_VOICE) refresh(app);
}

void sc_set_paused(lv_obj_t *screen, bool paused)
{
    companion_t *app = lv_obj_get_user_data(screen);
    app->paused = paused;
    if (paused) cancel_voice(app);
    if (!app->dirty) refresh(app);
}
