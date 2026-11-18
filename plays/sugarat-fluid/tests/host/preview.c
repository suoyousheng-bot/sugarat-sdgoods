/* SPDX-License-Identifier: Apache-2.0 */
/** Exercise the real solver under changing gravity and render its real LVGL view. */
#include "fluid/config.h"
#include "fluid/sim.h"
#include "fluid/render.h"
#include "bottle_view.h"
#include "sugarat_watch.h"
#include "esp_timer.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sim_particle_view_t particles[PARTICLE_MAX];
static int expected_count=PARTICLE_COUNT;
static void check_particles(void)
{
    int n = sim_snapshot(particles, PARTICLE_MAX);
    assert(n == expected_count);
    for (int i = 0; i < n; i++) {
        sim_particle_view_t p = particles[i];
        assert(isfinite(p.x) && isfinite(p.y) && isfinite(p.z) && isfinite(p.speed));
        float dx = p.x - 180, dy = p.y - 180;
        assert(dx * dx + dy * dy <= (FLUID_RADIUS - 4.9f) * (FLUID_RADIUS - 4.9f));
        assert(p.z >= 4.99f && p.z <= BOX_D - 4.99f);
    }
}

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    (void)area; (void)pixels;
    lv_disp_flush_ready(driver);
}

static void check_labels(lv_obj_t *object, FILE *text)
{
    if (lv_obj_check_type(object, &lv_label_class)) {
        const char *label = lv_label_get_text(object);
        fprintf(text, "%s\n", label);
        const lv_font_t *font = lv_obj_get_style_text_font(object, LV_PART_MAIN);
        uint32_t at = 0;
        while (label[at]) {
            uint32_t ch = _lv_txt_encoded_next(label, &at);
            if (ch == '\n') continue;
            lv_font_glyph_dsc_t glyph;
            assert(lv_font_get_glyph_dsc(font, &glyph, ch, 0));
        }
        lv_obj_update_layout(object);
        lv_area_t area;
        lv_obj_get_coords(object, &area);
        /* Every label corner stays on the round panel. */
        const int xs[2] = {area.x1, area.x2}, ys[2] = {area.y1, area.y2};
        for (int i = 0; i < 4; i++) {
            int dx = xs[i & 1] - 180, dy = ys[i >> 1] - 180;
            if (dx * dx + dy * dy >= 176 * 176) fprintf(stderr, "outside: %s (%d,%d)-(%d,%d)\n", label, area.x1, area.y1, area.x2, area.y2);
            assert(dx * dx + dy * dy < 176 * 176);
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(object); i++) check_labels(lv_obj_get_child(object, i), text);
}

static void save_frame(const char *directory, int theme, bottle_view_t *view)
{
    char path[1024];
    lv_scr_load(view->screen);
    lv_obj_update_layout(view->screen);
    lv_img_dsc_t *image = lv_snapshot_take(view->screen, LV_IMG_CF_TRUE_COLOR);
    assert(image);
    snprintf(path, sizeof(path), "%s/theme-%d.ppm", directory, theme);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n360 360\n255\n");
    const uint8_t *pixels = image->data;
    for (int i = 0; i < 360 * 360; i++) {
        unsigned c = (pixels[i * 2] << 8) | pixels[i * 2 + 1];
        unsigned char rgb[3] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
    lv_snapshot_free(image);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    sim_init(); fluid_render_init();
    const sim_forces_t down = {.gravity = {0, 215820, 0}};
    int64_t began = esp_timer_get_time();
    for (int step = 0; step < 2400; step++) {
        sim_forces_t forces = {.gravity = {sinf(step * 0.02f) * 215820, cosf(step * 0.02f) * 215820, sinf(step * 0.013f) * 215820}};
        if (step % 200 < 12) {
            forces.gravity[0] *= 8;
            forces.gravity[1] *= 8;
            forces.omega[2] = 15;
        }
        sim_step(step % 100 == 0 ? 1.0f : 0.02f, &forces);
        check_particles();
    }
    printf("PASS: 2400 tilt/shake steps; finite particles confined to circular walls (%.2f ms/step host).\n",
           (double)(esp_timer_get_time() - began) / 2400000);
    sim_reset();
    for (int i = 0; i < 250; i++) sim_step(0.02f, &down);
    check_particles();
    float initial_x = 0;
    for (int i = 0; i < PARTICLE_COUNT; i++) initial_x += particles[i].x / PARTICLE_COUNT;
    sim_forces_t right = {.gravity = {215820, 0, 0}};
    for (int i = 0; i < 300; i++) sim_step(0.02f, &right);
    check_particles();
    float moved_x = 0;
    for (int i = 0; i < PARTICLE_COUNT; i++) moved_x += particles[i].x / PARTICLE_COUNT;
    assert(moved_x > initial_x + 15);
    printf("PASS: changing gravity moves the liquid centroid right (%.1f -> %.1f).\n", (double)initial_x, (double)moved_x);
    sim_reset();
    for (int i = 0; i < 250; i++) sim_step(0.02f, &down);
    lv_init(); sw_init();
    lv_disp_draw_buf_t draw;
    lv_color_t *buffer = malloc(360 * 40 * sizeof(*buffer)); assert(buffer);
    lv_disp_draw_buf_init(&draw, buffer, NULL, 360 * 40);
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver); driver.hor_res = 360; driver.ver_res = 360;
    driver.flush_cb = flush; driver.draw_buf = &draw;
    lv_disp_t *display = lv_disp_drv_register(&driver); assert(display);
    uint16_t *pixels = malloc(360 * 360 * 2); assert(pixels);
    bottle_view_t view = {.screen = sw_screen()};
    bottle_view_create(&view, pixels);
    char path[1024]; snprintf(path, sizeof(path), "%s/ui-text.txt", argv[1]);
    FILE *text = fopen(path, "w"); assert(text);
    check_labels(view.screen, text); fclose(text);
    for (int theme = 0; theme < FLUID_THEMES; theme++) {
        bottle_liquid_t pure; bottle_liquid_fill(&pure, PARTICLE_COUNT, (unsigned)theme);
        sim_set_mix(fluid_theme_rgb[theme], true);
        fluid_scene_t scene = {.liquid = &pure, .down_y = 1, .time_ms = 10000 + (uint32_t)theme * 5000};
        fluid_render_scene(pixels, &scene);
        save_frame(argv[1], theme, &view);
    }
    const bottle_phase_t phases[]={BOTTLE_SOLO,BOTTLE_HOSTING,BOTTLE_SCANNING,BOTTLE_FOUND,BOTTLE_JOINING,BOTTLE_INVITED,BOTTLE_LINKED,BOTTLE_LOST};
    text=fopen(path,"a"); assert(text);
    bottle_link_t link;
    bottle_link_solo(&link,0,240); link.room=0x1234; link.peer=0x5678;
    for(unsigned i=0;i<sizeof(phases)/sizeof(phases[0]);i++) {
        link.phase=phases[i]; link.revision=1;
        bottle_view_menu(&view,&link,false,false); check_labels(view.menu,text);
        save_frame(argv[1],10+(int)i,&view);
    }
    link.phase=BOTTLE_SOLO;
    bottle_view_menu(&view,&link,true,false); check_labels(view.menu,text);
    bottle_view_menu(&view,&link,true,true); check_labels(view.menu,text);
    bottle_view_hide_menu(&view); fclose(text);
    const int levels[]={0,1,120,600,0,480};
    for(unsigned level=0;level<sizeof(levels)/sizeof(levels[0]);level++) {
        sim_inlet_t inlet = {.x = MOUTH_X, .y = MOUTH_Y + 10, .vy = 2000, .rgb = {245, 130, 25}};
        expected_count=levels[level]; sim_set_count(expected_count, &inlet);
        for(int i=0;i<160;i++) { sim_step(0.02f,&down); check_particles(); }
    }
    lv_obj_add_flag(view.title, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(view.hint, LV_OBJ_FLAG_HIDDEN);
    /* Pour sequence: an orange stream falls into a half-full blue bottle and lands as particles. */
    bottle_liquid_t mine, theirs;
    bottle_liquid_fill(&mine, 240, 0); bottle_liquid_fill(&theirs, 360, 2);
    expected_count = 240; sim_set_mix(fluid_theme_rgb[0], true);
    sim_inlet_t seed = {.x = MOUTH_X, .y = MOUTH_Y + 10, .vy = 2000};
    sim_set_count(240, &seed);
    for (int i = 0; i < 200; i++) sim_step(0.02f, &down);
    bool landed = false;
    for (int frame = 0; frame < 60; frame++) {
        fluid_scene_t scene = {.liquid = &mine, .source = &theirs, .flow = frame < 45 ? 1 : 0, .rate = 5,
            .down_y = 1, .linked = true, .time_ms = 40000 + (uint32_t)frame * 20};
        fluid_render_scene(pixels, &scene);
        if (scene.impact) {
            landed = true;
            if (expected_count < 300) {
                bottle_liquid_transfer(&theirs, &mine, 2);
                sim_inlet_t inlet = {.x = scene.impact_x, .y = scene.impact_y, .vx = scene.impact_vx * 5, .vy = scene.impact_vy * 5, .rgb = {245, 130, 25}};
                expected_count += 2; sim_set_count(expected_count, &inlet);
            }
        }
        uint8_t mix[3]; fluid_liquid_rgb(&mine, mix); sim_set_mix(mix, false);
        sim_step(0.02f, &down); check_particles();
        if (frame == 6 || frame == 30 || frame == 52) save_frame(argv[1], 20 + frame / 20, &view);
    }
    assert(landed);
    /* Pouring out: inverted bottle draining through the mouth, then the catch and wait cues. */
    const sim_forces_t up = {.gravity = {0, -215820, 0}};
    sim_set_outlet(60000);
    for (int i = 0; i < 120; i++) sim_step(0.02f, &up);
    for (int frame = 0; frame < 20; frame++) {
        fluid_scene_t scene = {.liquid = &mine, .flow = -1, .rate = 5, .down_y = -1, .linked = true, .time_ms = 50000 + (uint32_t)frame * 20};
        fluid_render_scene(pixels, &scene); sim_step(0.02f, &up);
    }
    save_frame(argv[1], 23, &view);
    sim_set_outlet(0);
    for (int i = 0; i < 200; i++) sim_step(0.02f, &down);
    fluid_scene_t catch_scene = {.liquid = &mine, .cue = FLUID_CUE_CATCH, .down_y = 1, .linked = true, .time_ms = 60000};
    fluid_render_scene(pixels, &catch_scene); save_frame(argv[1], 24, &view);
    for (int i = 0; i < 200; i++) sim_step(0.02f, &up);
    fluid_scene_t wait_scene = {.liquid = &mine, .cue = FLUID_CUE_WAIT, .down_y = -1, .linked = true, .time_ms = 60300};
    fluid_render_scene(pixels, &wait_scene); save_frame(argv[1], 25, &view);
    bottle_liquid_fill(&mine, 0, 0); expected_count = 0; sim_set_count(0, &seed);
    fluid_scene_t empty = {.liquid = &mine, .down_y = 1, .time_ms = 70000};
    fluid_render_scene(pixels, &empty); save_frame(argv[1], 26, &view);
    puts("PASS: variable quantity including empty/full, menu states, mixing and stream frames.");
    lv_disp_remove(display);
    free(pixels); free(buffer);
    puts("PASS: four real LVGL frames, displayed glyph coverage and safe touch-area labels.");
    return 0;
}
