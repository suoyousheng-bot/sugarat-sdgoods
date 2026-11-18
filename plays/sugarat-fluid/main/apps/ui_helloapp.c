/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Magic Bottle: one worker owns the solver; LVGL receives synchronized RGB565 frames. */
#include "ui_helloapp.h"
#include "bottle_view.h"
#include "sdgoods_board.h"
#include "sdgoods_nvs.h"
#include "sugarat_watch.h"
#include "sugarat_imu.h"
#include "sugarat_frame.h"
#include "fluid/config.h"
#include "fluid/sim.h"
#include "fluid/render.h"
#include "fluid/frames.h"
#include "fluid/radio.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/semphr.h"
#include <math.h>
#include <string.h>

_Static_assert(BOTTLE_CAPACITY==PARTICLE_MAX,"Liquid units must match particle capacity");
#define FRAME_BYTES (360 * 360 * 2)
static const char *TAG = "sugarat_fluid";
static const char *const themes[] = {"Ocean", "Starlight", "Amber", "Forest"};
/** Linked-play guidance the frame cannot express on its own. */
typedef enum { NOTICE_NONE, NOTICE_REARM, NOTICE_EMPTY } bottle_notice_t;
static struct {
    bottle_view_t view;
    sugarat_frame_stream_t *stream;
    sugarat_frame_stats_t frame_stats;
    sugarat_frame_pacer_t pacer;
    bool stream_disabled;
    fluid_frames_t frames;
    SemaphoreHandle_t lock, stopped;
    TaskHandle_t worker;
    bool stop, paused, motion_ok, prefs_failed;
    bool menu_open, nearby_menu, radio_failed;
    bottle_notice_t notice, shown_notice;
    unsigned action;
    bottle_link_t session;
    bottle_phase_t shown_phase;
    uint32_t shown_room;
    unsigned shown_amount, shown_theme;
    bool shown_radio_failed;
    unsigned theme, saved_theme;
    int64_t theme_changed_us, hint_until_us;
} bottle;

static unsigned load_theme(void)
{
    nvs_handle_t handle;
    uint8_t theme = 0;
    sdgoods_nvs_ensure();
    esp_err_t err = nvs_open("sugarat_fluid", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, "theme", &theme);
        nvs_close(handle);
    }
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Cannot read theme: %s; using Ocean for this run", esp_err_to_name(err));
        bottle.prefs_failed = true;
    }
    if (theme >= FLUID_THEMES) {
        ESP_LOGE(TAG, "Stored theme %u is invalid; using Ocean", theme);
        theme = 0;
        bottle.prefs_failed = true;
    }
    return theme;
}

static esp_err_t start_motion(void)
{
    sugarat_imu_config_t config = sugarat_imu_default_config();
    config.step_detection = false;
    return sugarat_imu_start(&config);
}

static bool save_theme(unsigned theme)
{
    /* Both sensor sampling and touch transactions must be idle during Flash writes. */
    sugarat_imu_state_t state;
    sugarat_imu_get_state(&state);
    sugarat_imu_stop();
    sdgoods_touch_flash_window(1000);
    nvs_handle_t handle;
    sdgoods_nvs_ensure();
    esp_err_t err = nvs_open("sugarat_fluid", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "theme", (uint8_t)theme);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    sdgoods_touch_flash_window_end();
    if (state.running) {
        esp_err_t motion = start_motion();
        if (motion != ESP_OK) ESP_LOGE(TAG, "Motion restart failed: %s", esp_err_to_name(motion));
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "Theme was not saved: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

static void fluid_worker(void *argument)
{
    (void)argument;
    sim_init();
    fluid_render_init();
    uint32_t samples = 0, frames = 0;
    int64_t max_sim_us = 0, max_render_us = 0, max_sensor_age_us = 0;
    int64_t last_sample = 0, previous = esp_timer_get_time(), report = previous;
    bottle_link_t link;
    bottle_radio_t radio={0};
    bottle_link_solo(&link,bottle.theme,PARTICLE_COUNT);
    float gravity[3] = {0};
    bool gravity_ready = false;
    /* Visible particles trail the authoritative quantity so arrivals follow the stream. */
    int shown_count = PARTICLE_COUNT;
    bool repaint = true;
    fluid_scene_t scene = {0};
    uint32_t impact_ms = 0;
    sim_inlet_t impact = {0};
    while (true) {
        int64_t now = esp_timer_get_time();
        xSemaphoreTake(bottle.lock, portMAX_DELAY);
        bool stop = bottle.stop, paused = bottle.paused;
        paused = paused || sdgoods_power_is_suspended();
        unsigned theme = bottle.theme;
        unsigned action=bottle.action;
        bool menu_open=bottle.menu_open;
        bottle.action=0;
        bool save = (theme != bottle.saved_theme) && (stop || paused || action==BOTTLE_ACTION_HOST || action==BOTTLE_ACTION_SCAN || now - bottle.theme_changed_us >= 700000);
        xSemaphoreGive(bottle.lock);
        if (save && !radio.now_owned) {
            bool saved = save_theme(theme);
            xSemaphoreTake(bottle.lock, portMAX_DELAY);
            bottle.prefs_failed = !saved;
            /* Failure is visible; another theme change permits a new save attempt. */
            bottle.saved_theme = theme;
            xSemaphoreGive(bottle.lock);
            gravity_ready = false;
            samples = 0;
            last_sample = 0;
        }
        if (stop) break;
        uint32_t now_ms=(uint32_t)(now/1000);
        if(action==BOTTLE_ACTION_SOLO) {
            bottle_link_leave(&link,bottle_radio_send,&radio); bottle_radio_close(&radio);
            bottle_link_solo(&link,theme,PARTICLE_COUNT); sim_reset(); shown_count=PARTICLE_COUNT; repaint=true;
            xSemaphoreTake(bottle.lock,portMAX_DELAY); bottle.radio_failed=false; xSemaphoreGive(bottle.lock);
        }
        if((action==BOTTLE_ACTION_HOST || action==BOTTLE_ACTION_SCAN) &&
           (link.phase==BOTTLE_SOLO || link.phase==BOTTLE_FOUND)) {
            if(!radio.now_owned && bottle_radio_open(&radio)!=ESP_OK) {
                xSemaphoreTake(bottle.lock,portMAX_DELAY); bottle.radio_failed=true; xSemaphoreGive(bottle.lock);
            } else {
                uint32_t identity; do { identity=esp_random(); } while(!identity);
                if(action==BOTTLE_ACTION_HOST) bottle_link_host(&link,identity,now_ms);
                else bottle_link_scan(&link,identity,now_ms);
            }
        }
        if(action==BOTTLE_ACTION_JOIN) bottle_link_join(&link,now_ms);
        if(action==BOTTLE_ACTION_ACCEPT) bottle_link_accept(&link,now_ms);
        if(link.phase==BOTTLE_SOLO) {
            if(action==BOTTLE_ACTION_COLOR) {
                theme=(theme+1)%FLUID_THEMES;
                bottle_liquid_fill(&link.liquid[1],bottle_liquid_total(&link.liquid[1]),theme);
                repaint=true;
                xSemaphoreTake(bottle.lock,portMAX_DELAY);
                bottle.theme=theme; bottle.theme_changed_us=now;
                xSemaphoreGive(bottle.lock);
            }
            if(action==BOTTLE_ACTION_FILL) {
                unsigned count=bottle_liquid_total(&link.liquid[1]);
                unsigned next=count<240?240:count<480?480:count<600?600:0;
                bottle_liquid_fill(&link.liquid[1],next,theme);
                if(!count) repaint=true;
            }
            if(action==BOTTLE_ACTION_RESET) {
                bottle_liquid_fill(&link.liquid[1],PARTICLE_COUNT,theme); sim_reset(); shown_count=PARTICLE_COUNT; repaint=true;
            }
        }
        bottle_radio_poll(&radio,&link,now_ms);
        sugarat_imu_state_t state;
        sugarat_imu_get_state(&state);
        if (state.samples != samples) {
            samples = state.samples;
            last_sample = now;
        }
        if (last_sample && now - last_sample > max_sensor_age_us) max_sensor_age_us = now - last_sample;
        bool motion = state.running && state.sensor_ok && last_sample && now - last_sample < 300000;
        if (motion && !paused) {
            float dt = (now - previous) * 0.000001f;
            if (dt > 0.04f) dt = 0.04f;
            if (dt < 0.001f) dt = 0.001f;
            /* IMU axes: right/up/out; simulation: right/down/in. Acceleration is support force. */
            float acceleration[3] = {-state.accel_g.x * 9.81f, state.accel_g.y * 9.81f, state.accel_g.z * 9.81f};
            sim_forces_t forces = {0};
            for (int a = 0; a < 3; a++) {
                if (!gravity_ready) gravity[a] = acceleration[a];
                gravity[a] += (acceleration[a] - gravity[a]) * fminf(dt * 7.5f, 1.0f);
                forces.gravity[a] = (gravity[a] + (acceleration[a] - gravity[a]) * SHAKE_GAIN) * PX_PER_METER * GRAVITY_GAIN;
            }
            gravity_ready = true;
            forces.omega[0] = state.gyro_dps.x * 0.017453293f;
            forces.omega[1] = -state.gyro_dps.y * 0.017453293f;
            forces.omega[2] = -state.gyro_dps.z * 0.017453293f;
            int64_t started = esp_timer_get_time();
            sim_step(dt, &forces);
            int64_t cost = esp_timer_get_time() - started;
            if (cost > max_sim_us) max_sim_us = cost;
        } else gravity_ready = false;
        bottle_input_t posture=bottle_input_from_gravity(gravity[0],gravity[1],gravity[2],gravity_ready);
        bool steady=!paused && !menu_open && motion;
        if(!steady) {
            link.rearm=true;
            bottle_link_input(&link,(bottle_input_t){BOTTLE_CLOSED,0});
        } else bottle_link_input(&link,posture);
        bottle_link_tick(&link,now_ms,bottle_radio_send,&radio);
        unsigned local_index=bottle_link_local_index(&link);
        const bottle_liquid_t *own=&link.liquid[local_index],*peer=&link.liquid[1-local_index];
        int flow=link.flow?(link.flow==(local_index==0?1:-1)?-1:1):0;
        bool linked=link.phase==BOTTLE_LINKED;
        fluid_cue_t cue=FLUID_CUE_NONE;
        bottle_notice_t notice=NOTICE_NONE;
        if(linked && steady) {
            if(posture.pose==BOTTLE_RECEIVE) cue=FLUID_CUE_CATCH;
            else if(posture.pose==BOTTLE_POUR) {
                if(link.rearm) notice=NOTICE_REARM;
                else if(!bottle_liquid_total(own)) notice=NOTICE_EMPTY;
                else if(link.remote.pose!=BOTTLE_RECEIVE) cue=FLUID_CUE_WAIT;
            }
        }
        uint8_t mix[3],incoming[3];
        fluid_liquid_rgb(own,mix);
        fluid_liquid_rgb(flow>0?peer:own,incoming);
        sim_set_mix(mix,repaint);
        repaint=false;
        if(scene.impact) {
            /* Arrivals join the liquid where the stream lands, carrying its momentum. */
            impact_ms=now_ms;
            impact=(sim_inlet_t){.x=scene.impact_x,.y=scene.impact_y,
                .vx=scene.impact_vx*0.5f/TIME_SCALE,.vy=scene.impact_vy*0.5f/TIME_SCALE};
        }
        int difference=(int)bottle_liquid_total(own)-shown_count,step=0;
        sim_inlet_t inlet={.x=MOUTH_X,.y=MOUTH_Y+10,.vx=0,.vy=2000};
        if(difference<0) step=difference/4<-1?difference/4:-1;
        else if(difference>0 && impact_ms && now_ms-impact_ms<300) { step=difference/4>1?difference/4:1; inlet=impact; }
        else if(difference>0 && flow<=0) step=difference/6>1?difference/6:1;
        /* While liquid is still falling in, wait for the stream to reach the surface. */
        if(step) {
            memcpy(inlet.rgb,incoming,3);
            shown_count+=step;
            sim_set_count(shown_count,&inlet);
        }
        sim_set_outlet(flow<0?60000.0f:0);
        xSemaphoreTake(bottle.lock,portMAX_DELAY);
        bottle.session=link;
        bottle.notice=notice;
        xSemaphoreGive(bottle.lock);
        if (!paused && !menu_open) {
            int64_t started = esp_timer_get_time();
            scene=(fluid_scene_t){.liquid=own,.source=flow>0?peer:own,.flow=flow,
                .rate=flow>0?link.remote.rate:flow<0?link.local.rate:0,
                .down_x=gravity_ready?gravity[0]/9.81f:0,.down_y=gravity_ready?gravity[1]/9.81f:1,
                .cue=cue,.linked=linked,.time_ms=now_ms};
            fluid_render_scene(bottle.frames.rendering,&scene);
            int64_t cost = esp_timer_get_time() - started;
            if (cost > max_render_us) max_render_us = cost;
            xSemaphoreTake(bottle.lock, portMAX_DELAY);
            fluid_frames_publish(&bottle.frames);
            bottle.motion_ok = motion;
            xSemaphoreGive(bottle.lock);
            frames++;
        } else {
            xSemaphoreTake(bottle.lock, portMAX_DELAY);
            bottle.motion_ok = motion;
            xSemaphoreGive(bottle.lock);
        }
        previous = now;
        if (now - report >= 5000000) {
            ESP_LOGI(TAG, "frames=%.1f/s motion=%d internal_free=%u dma_largest=%u psram_free=%u",
                     (double)frames * 1000000 / (now - report), motion,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            sim_stats_t stats;
            sim_stats(&stats);
            ESP_LOGI(TAG, "profile sim_max_us=%lld render_max_us=%lld sensor_age_max_us=%lld sample_hz=%.1f pairs=%d",
                     (long long)max_sim_us, (long long)max_render_us, (long long)max_sensor_age_us, (double)state.sample_hz, stats.pairs);
            max_sim_us = max_render_us = max_sensor_age_us = 0;
            report = now;
            frames = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(paused ? 30 : 10));
    }
    bottle_link_leave(&link,bottle_radio_send,&radio);
    bottle_radio_close(&radio);
    sugarat_imu_stop();
    xSemaphoreGive(bottle.stopped);
    vTaskDelete(NULL);
}

static void pause_bottle(void)
{
    xSemaphoreTake(bottle.lock, portMAX_DELAY);
    bottle.paused = true;
    xSemaphoreGive(bottle.lock);
    int64_t deadline = esp_timer_get_time() + 1500000;
    while (bottle.worker && esp_timer_get_time() < deadline) {
        xSemaphoreTake(bottle.lock, portMAX_DELAY);
        bool saved = bottle.theme == bottle.saved_theme;
        xSemaphoreGive(bottle.lock);
        if (saved) return;
        vTaskDelay(1);
    }
    ESP_LOGW(TAG, "Theme save deadline exceeded; latest theme may be lost");
}

static void resume_bottle(void)
{
    xSemaphoreTake(bottle.lock, portMAX_DELAY);
    bottle.paused = false;
    xSemaphoreGive(bottle.lock);
}

static void close_bottle(void)
{
    xSemaphoreTake(bottle.lock, portMAX_DELAY);
    bottle.stop = true;
    xSemaphoreGive(bottle.lock);
    /* Keep frame memory until the producer stops accessing it. */
    if (xSemaphoreTake(bottle.stopped, pdMS_TO_TICKS(4000)) != pdTRUE) {
        ESP_LOGE(TAG, "Worker did not stop; retaining frame memory and halting cleanup");
        return;
    }
    lv_obj_del(bottle.view.screen);
    heap_caps_free(bottle.frames.rendering);
    heap_caps_free(bottle.frames.ready);
    heap_caps_free(bottle.frames.visible);
    vSemaphoreDelete(bottle.lock);
    vSemaphoreDelete(bottle.stopped);
    /* The stream sender owns its buffers for the firmware lifetime. */
    sugarat_frame_stream_t *stream = bottle.stream;
    bool disabled = bottle.stream_disabled;
    memset(&bottle, 0, sizeof(bottle));
    bottle.stream = stream;
    bottle.stream_disabled = disabled;
    sdgoods_lvgl_fast_loop(false);
}

static void show_menu(void)
{
    xSemaphoreTake(bottle.lock,portMAX_DELAY);
    bottle.menu_open=true;
    bottle_link_t link=bottle.session;
    bool failed=bottle.radio_failed;
    xSemaphoreGive(bottle.lock);
    bottle_view_menu(&bottle.view,&link,bottle.nearby_menu,failed);
    bottle.shown_phase=link.phase; bottle.shown_room=link.room;
    bottle.shown_amount=bottle_liquid_total(&link.liquid[bottle_link_local_index(&link)]);
    bottle.shown_radio_failed=failed;
}

static void gesture(lv_event_t *event)
{
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_CLICKED && bottle.view.menu) {
        int action=sw_event_action(event);
        if(action==SW_ACTION_NONE) return;
        if(action==BOTTLE_ACTION_BACK) {
            bottle_view_hide_menu(&bottle.view);
            xSemaphoreTake(bottle.lock,portMAX_DELAY); bottle.menu_open=false; xSemaphoreGive(bottle.lock);
            return;
        }
        if(action==BOTTLE_ACTION_NEARBY) { bottle.nearby_menu=true; show_menu(); return; }
        xSemaphoreTake(bottle.lock,portMAX_DELAY);
        if(!bottle.action) bottle.action=(unsigned)action;
        xSemaphoreGive(bottle.lock);
        if(action==BOTTLE_ACTION_SOLO) bottle.nearby_menu=false;
        return;
    }
    if(lv_event_get_target(event)!=bottle.view.screen || bottle.view.menu) return;
    lv_indev_t *input=lv_indev_get_act(); if(!input) return;
    if(code==LV_EVENT_GESTURE && lv_indev_get_gesture_dir(input)==LV_DIR_RIGHT) { show_menu(); return; }
    if(code!=LV_EVENT_SHORT_CLICKED && code!=LV_EVENT_LONG_PRESSED) return;
    lv_point_t point; lv_indev_get_point(input,&point);
    int dx=point.x-180,dy=point.y-180;
    if(dx*dx+dy*dy>150*150) return;
    xSemaphoreTake(bottle.lock,portMAX_DELAY);
    bool solo=bottle.session.phase==BOTTLE_SOLO;
    if(!bottle.paused && solo && !bottle.action)
        bottle.action=code==LV_EVENT_LONG_PRESSED?BOTTLE_ACTION_RESET:BOTTLE_ACTION_COLOR;
    xSemaphoreGive(bottle.lock);
    if(!solo) { show_menu(); return; }
    bottle.hint_until_us=esp_timer_get_time()+2000000;
    lv_label_set_text(bottle.view.hint,code==LV_EVENT_LONG_PRESSED?"Bottle reset":"Tap: color · Swipe right: menu");
    lv_obj_clear_flag(bottle.view.hint,LV_OBJ_FLAG_HIDDEN);
}

void ui_helloapp_start(void)
{
    if (bottle.view.screen) { lv_scr_load(bottle.view.screen); return; }
    sw_init();
    bottle.view.screen = sw_screen();
    bottle.frames.rendering = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bottle.frames.ready = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bottle.frames.visible = heap_caps_calloc(1, FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bottle.lock = xSemaphoreCreateMutex();
    bottle.stopped = xSemaphoreCreateBinary();
    if (!bottle.frames.rendering || !bottle.frames.ready || !bottle.frames.visible || !bottle.lock || !bottle.stopped) {
        heap_caps_free(bottle.frames.rendering); heap_caps_free(bottle.frames.ready); heap_caps_free(bottle.frames.visible);
        if (bottle.lock) vSemaphoreDelete(bottle.lock);
        if (bottle.stopped) vSemaphoreDelete(bottle.stopped);
        bottle.lock = NULL;
        lv_obj_t *label = sw_label(bottle.view.screen, "Not enough memory.\nRestart the device.", SW_FONT_BODY, SW_TONE_RED);
        lv_obj_center(label);
        lv_scr_load(bottle.view.screen);
        sdgoods_app_shell_bind(bottle.view.screen);
        ESP_LOGE(TAG, "Frame or synchronization allocation failed; simulation is stopped");
        return;
    }
    bottle.theme = load_theme();
    bottle.saved_theme = bottle.theme;
    bottle.shown_theme = bottle.theme;
    bottle_link_solo(&bottle.session,bottle.theme,PARTICLE_COUNT);
    bottle_view_create(&bottle.view, bottle.frames.visible);
    bottle.hint_until_us = esp_timer_get_time() + 4500000;
    lv_obj_add_event_cb(bottle.view.screen, gesture, LV_EVENT_ALL, NULL);
    lv_scr_load(bottle.view.screen);
    sdgoods_app_shell_bind(bottle.view.screen);
    sdgoods_app_shell_set_pause_cb(pause_bottle);
    sdgoods_app_shell_set_resume_cb(resume_bottle);
    sdgoods_app_shell_set_exit_cb(close_bottle);
    if (!bottle.stream && !bottle.stream_disabled) {
        sugarat_frame_stream_config_t config = sugarat_frame_stream_default_config();
        config.sender_core = 0;
        esp_err_t streaming = sugarat_frame_stream_create(&config, &bottle.stream);
        if (streaming != ESP_OK) {
            bottle.stream_disabled = true;
            ESP_LOGW(TAG, "Direct frames unavailable (%s); using LVGL", esp_err_to_name(streaming));
        }
    }
    sugarat_frame_pacer_init(&bottle.pacer, 33333);
    sdgoods_lvgl_fast_loop(false);
    esp_err_t motion = start_motion();
    if (motion != ESP_OK) ESP_LOGE(TAG, "Motion initialization failed: %s", esp_err_to_name(motion));
    if (xTaskCreatePinnedToCore(fluid_worker, "fluid", 6144, NULL, 3, &bottle.worker, 1) != pdPASS) {
        sugarat_imu_stop();
        lv_label_set_text(bottle.view.hint, "Could not start.\nRestart the device.");
        bottle.hint_until_us = INT64_MAX;
        xSemaphoreGive(bottle.stopped);
        ESP_LOGE(TAG, "Worker creation failed; simulation is stopped");
        return;
    }
    ESP_LOGI(TAG, "SUGARAT_FLUID_READY version=0.2.1 particles=%d", PARTICLE_COUNT);
}

static void fill_strip(uint16_t *pixels, int x1, int y, int x2, int rows, void *argument)
{
    const uint16_t *source = argument;
    for (int row = 0; row < rows; row++)
        memcpy(pixels + row * (x2 - x1), source + (y + row) * 360 + x1, (size_t)(x2 - x1) * 2);
}

void ui_helloapp_poll(void)
{
    if (!bottle.lock || !bottle.worker) return;
    int64_t poll_us = esp_timer_get_time();
    if (bottle.pacer.next_us && poll_us < bottle.pacer.next_us) return;
    if (xSemaphoreTake(bottle.lock, 0) != pdTRUE) return;
    /* The opaque menu hides the frame; skipping it keeps touch handling responsive. */
    bool changed = !bottle.paused && !bottle.menu_open && !sdgoods_power_is_suspended() &&
        fluid_frames_present(&bottle.frames, &bottle.pacer, poll_us);
    if (changed) bottle.view.descriptor.data = (uint8_t *)bottle.frames.visible;
    bool motion = bottle.motion_ok, prefs_failed = bottle.prefs_failed;
    unsigned theme=bottle.theme;
    bottle_link_t link=bottle.session;
    bool radio_failed=bottle.radio_failed,menu_open=bottle.menu_open;
    bottle_notice_t notice=bottle.notice;
    xSemaphoreGive(bottle.lock);
    if (changed) lv_img_cache_invalidate_src(&bottle.view.descriptor);
    int64_t now = esp_timer_get_time();
    if(theme!=bottle.shown_theme) {
        bottle.shown_theme=theme;
        lv_label_set_text(bottle.view.title,themes[theme]);
        lv_obj_clear_flag(bottle.view.title,LV_OBJ_FLAG_HIDDEN);
        bottle.hint_until_us=now+2000000;
    }
    unsigned amount=bottle_liquid_total(&link.liquid[bottle_link_local_index(&link)]);
    if(link.phase!=bottle.shown_phase || link.room!=bottle.shown_room || amount!=bottle.shown_amount || radio_failed!=bottle.shown_radio_failed) {
        bool phase_changed=link.phase!=bottle.shown_phase;
        if(phase_changed && link.phase==BOTTLE_LINKED) {
            bottle_view_hide_menu(&bottle.view);
            xSemaphoreTake(bottle.lock,portMAX_DELAY); bottle.menu_open=false; xSemaphoreGive(bottle.lock);
            menu_open=false;
            bottle.hint_until_us=now+6000000;
            lv_label_set_text(bottle.view.title,"Bottles connected");
            lv_label_set_text(bottle.view.hint,"Top edge is the bottle mouth\nUpright: catch · Mouth down: pour");
            lv_obj_clear_flag(bottle.view.title,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(bottle.view.hint,LV_OBJ_FLAG_HIDDEN);
        } else if(menu_open || link.phase==BOTTLE_LOST || link.phase==BOTTLE_INVITED || radio_failed) show_menu();
        bottle.shown_phase=link.phase; bottle.shown_room=link.room; bottle.shown_amount=amount; bottle.shown_radio_failed=radio_failed;
    }
    if(notice!=bottle.shown_notice) {
        bottle.shown_notice=notice;
        if(notice!=NOTICE_NONE) {
            lv_label_set_text(bottle.view.hint,notice==NOTICE_REARM?"Hold upright once,\nthen turn mouth down":"Your bottle is empty.\nHold upright to catch");
            lv_obj_clear_flag(bottle.view.hint,LV_OBJ_FLAG_HIDDEN);
            bottle.hint_until_us=now+2500000;
        }
    }
    if (now > bottle.hint_until_us) {
        lv_obj_add_flag(bottle.view.title, LV_OBJ_FLAG_HIDDEN);
        if (!motion || prefs_failed) {
            lv_label_set_text(bottle.view.hint, !motion ? "Motion unavailable.\nRestart the device." : "Settings not saved.\nPrevious settings will be used.");
            lv_obj_clear_flag(bottle.view.hint, LV_OBJ_FLAG_HIDDEN);
        } else if(radio_failed || link.phase==BOTTLE_LOST) {
            lv_label_set_text(bottle.view.hint,radio_failed?"Radio unavailable.\nOpen menu to return to Solo.":"Transfer stopped.\nKeep nearby or open menu.");
            lv_obj_clear_flag(bottle.view.hint,LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(bottle.view.hint, LV_OBJ_FLAG_HIDDEN);
    }
    bool labels_hidden = !bottle.view.menu && lv_obj_has_flag(bottle.view.title, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(bottle.view.hint, LV_OBJ_FLAG_HIDDEN);
    sdgoods_lvgl_fast_loop(labels_hidden);
    if (changed) {
        int64_t start = esp_timer_get_time();
        bool clear = labels_hidden;
        bool direct = false;
        if (clear && bottle.stream && !bottle.stream_disabled &&
            lv_scr_act() == bottle.view.screen && sugarat_frame_lvgl_layers_clear() &&
            sdgoods_touch_get_state() != LV_INDEV_STATE_PRESSED && !sdgoods_power_is_suspended()) {
            esp_err_t sent = sugarat_frame_stream_send(bottle.stream, 0, 0, 360, 360, fill_strip, bottle.frames.visible);
            direct = sent == ESP_OK;
            if (!direct) {
                ESP_LOGE(TAG, "Direct frame failed (%s); using LVGL", esp_err_to_name(sent));
                bottle.stream_disabled = true;
            }
        }
        if (!direct) lv_obj_invalidate(bottle.view.image);
        sugarat_frame_stats_record(&bottle.frame_stats, start, esp_timer_get_time(), true, direct);
    }
    if (changed) sugarat_frame_pacer_finish(&bottle.pacer, esp_timer_get_time(), true);
    sugarat_frame_summary_t summary;
    if (sugarat_frame_stats_window(&bottle.frame_stats, esp_timer_get_time(), 5000000, &summary)) {
        ESP_LOGI(TAG, "present=%.1f/s direct=%lu/%lu frame_ms=%.1f max_ms=%.1f gap_max_ms=%.1f",
                 (double)summary.fps, (unsigned long)summary.direct_frames, (unsigned long)summary.frames,
                 (double)summary.avg_us / 1000, (double)summary.max_us / 1000, (double)summary.gap_max_us / 1000);
    }
}

/** Flush the pending preference through its worker before the BSP cuts power. */
void sdgoods_app_pre_poweroff(void)
{
    if (!bottle.worker) return;
    xSemaphoreTake(bottle.lock, portMAX_DELAY);
    bottle.stop = true;
    xSemaphoreGive(bottle.lock);
    if (xSemaphoreTake(bottle.stopped, pdMS_TO_TICKS(3000)) == pdTRUE) {
        bottle.worker = NULL;
    } else ESP_LOGE(TAG, "Power-off save deadline exceeded; latest theme may be lost");
}
