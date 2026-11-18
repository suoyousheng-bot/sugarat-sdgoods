/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Deterministic care, durable-record failure recovery and actual LVGL touch/render checks. */
#include "pet_model.h"
#include "pet_storage.h"
#include "pet_view.h"
#include "pet_animation.h"
#include "pet_family_store.h"
#include "sdgoods_gesture.h"
#include "epet_module.h"
#include "ui_helloapp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static struct {
    uint8_t records[2][PET_SAVE_BYTES];
    size_t sizes[2];
    bool fail_open, fail_read, fail_write, fail_commit, fail_readback;
    unsigned writes, commits;
} disk;
void sdgoods_nvs_ensure(void) {}
esp_err_t nvs_get_stats(const char *part,nvs_stats_t *s){(void)part;memset(s,0,sizeof(*s));s->available_entries=630;return ESP_OK;}
void sdgoods_touch_flash_window(int ms){assert(ms>0);}
void sdgoods_touch_flash_window_end(void){}
void sdgoods_touch_synth_report(lv_indev_state_t state,lv_coord_t x,lv_coord_t y){(void)state;(void)x;(void)y;}
bool sdgoods_lvgl_post(void (*callback)(void *),void *context){return lv_async_call(callback,context)==LV_RES_OK;}
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle)
{
    assert(!strcmp(name, "sugarat_pet") && mode == NVS_READWRITE);
    *handle = 1;
    return disk.fail_open ? ESP_FAIL : ESP_OK;
}
static unsigned slot(const char *key)
{
    assert(!strcmp(key, "pet_a") || !strcmp(key, "pet_b"));
    return !strcmp(key, "pet_b");
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size)
{
    assert(h == 1);
    if (disk.fail_read || (disk.fail_readback && disk.commits)) return ESP_FAIL;
    if(!strcmp(key,"family_a")||!strcmp(key,"family_b"))return ESP_ERR_NVS_NOT_FOUND;
    unsigned i = slot(key);
    if (!disk.sizes[i]) return ESP_ERR_NVS_NOT_FOUND;
    if (*size < disk.sizes[i]) return ESP_ERR_NVS_INVALID_LENGTH;
    *size = disk.sizes[i]; memcpy(out, disk.records[i], *size);
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *in, size_t size)
{
    assert(h == 1 && size == PET_SAVE_BYTES);
    if (disk.fail_write) return ESP_FAIL;
    unsigned i = slot(key);
    memcpy(disk.records[i], in, size); disk.sizes[i] = size; disk.writes++;
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { assert(h == 1); disk.commits++; return disk.fail_commit ? ESP_FAIL : ESP_OK; }
void nvs_close(nvs_handle_t h) { assert(h == 1); }

static void checksum(uint8_t data[PET_SAVE_BYTES])
{
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < 52; i++) h = (h ^ data[i]) * 16777619u;
    for (unsigned i = 0; i < 4; i++) data[52 + i] = (uint8_t)(h >> (8 * i));
}

static void model_tests(void)
{
    pet_model_t m; pet_model_init(&m, 0);
    assert(!strcmp(m.pet.species->name, "EMBER"));
    assert(m.pet.display_timeout_ms == 0 && epet_age_level(&m.pet) == 1);
    float hunger = m.pet.hunger;
    assert(pet_model_action(&m, PET_FEED));
    assert(m.pet.hunger < hunger && m.page == PET_FEEDING);
    assert(!pet_model_action(&m, PET_PLAY));
    pet_model_tick(&m, 1000);
    assert(pet_model_action(&m, PET_PLAY));
    float happy = m.pet.happiness;
    pet_model_pause(&m, true, 1000);
    pet_model_tick(&m, 3600000);
    assert(m.pet.happiness == happy && !pet_model_action(&m, PET_FEED));
    pet_model_pause(&m, false, 3600000);
    pet_model_tick(&m, 3601000);
    assert(m.pet.happiness > happy - 0.1f);
    assert(pet_model_action(&m, PET_SLEEP));
    float energy = m.pet.energy;
    for (unsigned i = 1; i <= 60; i++) pet_model_tick(&m, 3601000 + i * 1000);
    assert(m.pet.energy > energy && m.pet.asleep);
    assert(!pet_model_action(&m, PET_FEED));
    assert(pet_model_action(&m, PET_WAKE));
    m.pet.hygiene = 40; m.pet.poop = 2; m.cooldown_ms = 0;
    assert(pet_model_action(&m, PET_CLEAN) && m.pet.poop == 1 && m.pet.hygiene == 70);
    m.pet.hunger = 100; m.pet.hygiene = 0; m.pet.happiness = 0; m.pet.health = 20;
    for (unsigned i = 0; i < 20000; i++) pet_model_tick(&m, 4000000ull + i * 1000);
    assert(m.pet.alive && m.pet.health >= 20 && isfinite(m.pet.health));
    assert(epet_age_level(&m.pet) > 1);
    m.pet.age_ms = 31 * 30000; pet_model_restored(&m);
    assert(!strcmp(pet_model_stage(&m), "Adult"));
    uint8_t record[PET_SAVE_BYTES]; pet_save_encode(&m, 9, record);
    pet_model_t restored; pet_model_init(&restored, 0); uint32_t seq;
    assert(pet_save_decode(&restored, record, sizeof(record), &seq) == PET_RECORD_OK && seq == 9);
    assert(restored.pet.age_ms == m.pet.age_ms && restored.pet.hunger == m.pet.hunger);
    record[16] ^= 1;
    uint32_t old_age = restored.pet.age_ms;
    assert(pet_save_decode(&restored, record, sizeof(record), &seq) == PET_RECORD_CORRUPT && restored.pet.age_ms == old_age);
    pet_save_encode(&m, 9, record); record[4] = 2; checksum(record);
    assert(pet_save_decode(&restored, record, sizeof(record), &seq) == PET_RECORD_VERSION);
    pet_save_encode(&m, 9, record); record[18] = 0xc0; record[19] = 0x7f; checksum(record);
    assert(pet_save_decode(&restored, record, sizeof(record), &seq) == PET_RECORD_CORRUPT);
    assert(pet_save_decode(&restored, record, sizeof(record) - 1, &seq) == PET_RECORD_CORRUPT);
    for (unsigned i = 0; i < 5; i++) {
        pet_save_encode(&m, 9, record);
        uint32_t bad = 0x42ca0000u; /* 101.0f crosses every stat's upper limit. */
        for (unsigned b = 0; b < 4; b++) record[16 + i * 4 + b] = (uint8_t)(bad >> (b * 8));
        checksum(record);
        assert(pet_save_decode(&restored, record, sizeof(record), &seq) == PET_RECORD_CORRUPT);
    }
    for (unsigned i = 0; i < 3; i++) {
        pet_save_encode(&m, 9, record);
        record[i == 0 ? 36 : i == 1 ? 37 : 40] = i == 0 ? 2 : i == 1 ? 5 : 'X';
        checksum(record);
        assert(pet_save_decode(&restored, record, sizeof(record), &seq) == PET_RECORD_CORRUPT);
    }
    puts("PASS: epet care, cooldown, sleep, paused time, growth, gentle recovery and invalid records");
}

static void storage_tests(void)
{
    memset(&disk, 0, sizeof(disk));
    pet_model_t m; pet_model_init(&m, 0); pet_storage_t s;
    pet_storage_open(&s, &m); assert(s.writable);
    assert(pet_storage_flush(&s, &m));
    m.pet.hunger = 75; m.dirty = true;
    assert(pet_storage_flush(&s, &m) && s.sequence == 2 && disk.writes == 2);
    pet_storage_close(&s);
    pet_model_init(&m, 0); pet_storage_open(&s, &m);
    assert(m.pet.hunger == 75 && s.sequence == 2);
    pet_storage_close(&s);
    disk.records[1][20] ^= 4;
    pet_model_init(&m, 0); pet_storage_open(&s, &m);
    assert(s.writable && s.sequence == 1 && s.notice && m.pet.hunger == 20);
    pet_storage_close(&s);
    disk.records[0][20] ^= 4;
    pet_model_init(&m, 0); pet_storage_open(&s, &m);
    unsigned writes = disk.writes;
    assert(!s.writable && !pet_storage_flush(&s, &m) && disk.writes == writes);
    pet_storage_close(&s);
    memset(&disk, 0, sizeof(disk));
    pet_save_encode(&m, 4, disk.records[0]); disk.records[0][4] = 2; checksum(disk.records[0]); disk.sizes[0] = PET_SAVE_BYTES;
    pet_storage_open(&s, &m); assert(!s.writable && s.notice && disk.writes == 0); pet_storage_close(&s);
    for (unsigned fault = 0; fault < 5; fault++) {
        memset(&disk, 0, sizeof(disk)); pet_model_init(&m, 0);
        disk.fail_open = fault == 0; disk.fail_read = fault == 1;
        pet_storage_open(&s, &m);
        disk.fail_write = fault == 2; disk.fail_commit = fault == 3; disk.fail_readback = fault == 4;
        assert(!pet_storage_flush(&s, &m) && !s.writable && s.notice && m.dirty);
        pet_storage_close(&s);
    }
    puts("PASS: dual saves, reboot restore, corrupt-slot recovery, unknown-version preservation and storage failures");
}

uint64_t host_time_ms;
static void (*overlay_pause)(void), (*overlay_resume)(void);
static bool (*short_press)(void);
static unsigned launcher_returns, shell_binds;
static char appdata[192];
static bool legacy_start;
esp_err_t sdgoods_appdata_begin(const char *id,char *out,size_t size)
{assert(!strcmp(id,"sugarat_pet"));assert(strlen(appdata)<size);strcpy(out,appdata);return ESP_OK;}
static unsigned family_sequence(void)
{
    unsigned latest=0;
    for(unsigned i=0;i<2;i++) {
        char name[256];snprintf(name,sizeof(name),"%s/family-%c.dat",appdata,'a'+i);
        FILE *f=fopen(name,"rb");if(!f)continue;uint8_t header[12];assert(fread(header,1,12,f)==12);fclose(f);
        unsigned n=(unsigned)header[8]|(unsigned)header[9]<<8|(unsigned)header[10]<<16|(unsigned)header[11]<<24;if(n>latest)latest=n;
    }
    return latest;
}
void sdgoods_app_shell_bind(lv_obj_t *screen) { assert(screen); shell_binds++; }
void sdgoods_app_shell_set_pause_cb(void (*callback)(void)) { overlay_pause = callback; }
void sdgoods_app_shell_set_resume_cb(void (*callback)(void)) { overlay_resume = callback; }
void sdgoods_set_power_short_handler(bool (*callback)(void)) { short_press = callback; }
bool sdgoods_cc_power_short(void) { return false; }
const char *sdgoods_device_mode_str(void) { return "managed"; }
esp_err_t __real_sdgoods_launcher_return_to_launcher(void) { launcher_returns++; return ESP_OK; }
esp_err_t __wrap_sdgoods_launcher_return_to_launcher(void);
void sdgoods_app_pre_poweroff(void);
static lv_point_t pointer;
static bool pressed;
static void read_pointer(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv; data->point = pointer; data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *pixels)
{
    (void)area; (void)pixels; lv_disp_flush_ready(drv);
}
static void on_input(void *context, pet_action_t action)
{
    pet_view_t *v=context;
    if(action==PET_FAMILY) {v->page=PET_VIEW_FAMILY;v->selection=v->family->active;}
    else if(action==PET_SELECT) {assert(pet_family_select(v->family,v->model,v->selection));v->page=PET_VIEW_CARE;}
    else if(action==PET_BACK) {v->page=PET_VIEW_CARE;pet_model_action(v->model,PET_BACK);}
    else pet_model_action(v->model,action);
}
static void pump(unsigned duration)
{
    for (unsigned i = 0; i < duration; i += 10) { host_time_ms += 10; lv_tick_inc(10); lv_timer_handler(); }
}
static void tap(int x, int y, unsigned hold)
{
    pointer = (lv_point_t){x, y}; pressed = true; pump(hold); pressed = false; pump(60);
}
static void swipe(int x,int from_y,int to_y)
{
    pointer=(lv_point_t){x,from_y};pressed=true;pump(60);
    for(unsigned step=1;step<=24;step++){pointer.y=from_y+(to_y-from_y)*(int)step/24;pump(10);}
    pressed=false;pump(260);
}
static void horizontal(int y,int from_x,int to_x)
{
    pointer=(lv_point_t){from_x,y};pressed=true;pump(60);
    for(unsigned step=1;step<=24;step++){pointer.x=from_x+(to_x-from_x)*(int)step/24;pump(10);}
    pressed=false;pump(260);
}
static void labels(lv_obj_t *o, FILE *text)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *s = lv_label_get_text(o); const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        uint32_t byte=0;
        while(s[byte]) {
            uint32_t codepoint=_lv_txt_encoded_next(s,&byte);if(codepoint==10)continue;
            lv_font_glyph_dsc_t glyph;assert(lv_font_get_glyph_dsc(font,&glyph,codepoint,0));
        }
        lv_point_t measure; lv_txt_get_size(&measure, s, font, 0, 0, lv_obj_get_width(o), LV_TEXT_FLAG_NONE);
        assert(measure.x <= lv_obj_get_width(o));
        lv_area_t a; lv_obj_get_coords(o, &a);
        int xs[]={a.x1,a.x2},ys[]={a.y1,a.y2};
        for(unsigned xi=0;xi<2;xi++)for(unsigned yi=0;yi<2;yi++) {
            int dx=xs[xi]-180,dy=ys[yi]-180;
            if(dx*dx+dy*dy>176*176)fprintf(stderr,"unsafe text %s at %d,%d..%d,%d\n",s,a.x1,a.y1,a.x2,a.y2);
            assert(dx*dx+dy*dy<=176*176);
        }
        fprintf(text, "%s\n", s);
    }
    for (unsigned i = 0; i < lv_obj_get_child_cnt(o); i++) labels(lv_obj_get_child(o, i), text);
}
static void shot(pet_view_t *v, const char *out, const char *name, FILE *text)
{
    pet_view_update(v, v->save_notice); lv_obj_update_layout(v->screen);
    if(text){fprintf(text,"[%s]\n",name);labels(v->screen,text);}
    uint32_t bytes = lv_snapshot_buf_size_needed(v->screen, LV_IMG_CF_TRUE_COLOR);
    void *buffer = malloc(bytes); assert(buffer);
    lv_img_dsc_t descriptor;
    assert(lv_snapshot_take_to_buf(v->screen, LV_IMG_CF_TRUE_COLOR, &descriptor, buffer, bytes) == LV_RES_OK);
    lv_img_dsc_t *image = &descriptor;
    char path[1024]; snprintf(path, sizeof(path), "%s/%s.ppm", out, name);
    FILE *f = fopen(path, "wb"); assert(f); fprintf(f, "P6\n360 360\n255\n");
    for (unsigned i = 0; i < 360 * 360; i++) {
        unsigned c = image->data[i * 2] << 8 | image->data[i * 2 + 1];
        uint8_t rgb[] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f); free(buffer);
}
static void ui_tests(const char *out)
{
    lv_init();
    static lv_color_t draw_pixels[360 * 12];
    static lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw, draw_pixels, NULL, 360 * 12);
    lv_disp_drv_t drv; lv_disp_drv_init(&drv); drv.hor_res = drv.ver_res = 360; drv.draw_buf = &draw; drv.flush_cb = flush;
    lv_disp_t *display = lv_disp_drv_register(&drv); assert(display);
    lv_indev_drv_t input; lv_indev_drv_init(&input); input.type = LV_INDEV_TYPE_POINTER; input.read_cb = read_pointer;
    lv_indev_t *device = lv_indev_drv_register(&input); assert(device);
    lv_obj_t *blank = lv_obj_create(NULL); lv_scr_load(blank);
    pump(60);
    /* LVGL owns reusable font and drawing scratch space independently of the view. */
    _lv_font_clean_up_fmt_txt(); lv_mem_buf_free_all();
    lv_mem_monitor_t before, after; lv_mem_monitor(&before);
    uint16_t *pixels = malloc(180 * 180 * 2); assert(pixels);
    pet_model_t m; pet_model_init(&m, 0); pet_view_t view;
    pet_view_create(&view, &m, pixels, on_input, &view);
    pet_family_t family;pet_id_t id={{1}};pet_family_init(&family,&m,id);
    pet_social_t social={.state=PET_SOCIAL_SEARCH,.notice="Choose each other on both devices"};
    pet_view_attach(&view,&family,&social);lv_scr_load(view.screen);pump(60);
    char path[1024]; snprintf(path, sizeof(path), "%s/ui-text.txt", out); FILE *text = fopen(path, "w"); assert(text);
    shot(&view, out, "home", text);
    assert(lv_obj_has_flag(view.buttons[1],LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(view.buttons[4],LV_OBJ_FLAG_HIDDEN));
    float before_swipe=m.pet.hunger;
    assert(lv_obj_get_width(view.buttons[1])==72&&lv_obj_get_height(view.buttons[1])==64);
    assert(lv_obj_get_x(view.buttons[2])-lv_obj_get_x(view.buttons[1])-72==10);
    assert(lv_obj_get_y(view.buttons[4])-lv_obj_get_y(view.buttons[1])-64==10);
    assert(lv_obj_get_width(view.buttons[6])==72&&lv_obj_get_height(view.buttons[6])==72);
    assert(lv_obj_get_width(view.buttons[11])==152&&lv_obj_get_height(view.buttons[11])==56);
    swipe(180,320,210);assert(view.controls_open&&lv_obj_get_y(view.drawer)==150);shot(&view,out,"controls",text);
    tap(139,206,60);tap(221,206,60);tap(180,247,60);assert(view.controls_open&&m.pet.hunger==before_swipe);
    swipe(110,210,290);assert(!view.controls_open&&m.pet.hunger==before_swipe);
    swipe(180,320,210);tap(110,216,800);assert(!view.controls_open&&m.page==PET_FEEDING&&m.pet.hunger==0);
    shot(&view, out, "feeding", text);
    m.cooldown_ms = 0;
    swipe(180,320,210);tap(110,286,60);assert(m.page==PET_STATS&&!view.controls_open);
    shot(&view, out, "stats", text);
    tap(80,94,60); assert(m.page == PET_HOME);
    pet_view_update(&view, NULL); pump(60);
    swipe(180,320,210);tap(250,216,60);assert(m.pet.asleep&&m.page==PET_SLEEPING&&!view.controls_open);
    shot(&view, out, "sleep", text);
    m.cooldown_ms = 0;
    swipe(180,320,210);shot(&view,out,"sleep-controls",text);tap(250,216,60);assert(!m.pet.asleep&&m.page==PET_HOME);
    swipe(180,320,210);tap(180,120,60);pump(250);assert(!view.controls_open);
    swipe(180,320,210);assert(view.controls_open);
    swipe(180,170,290);assert(!view.controls_open);
    for(int x=90;x<=270;x+=90)for(int y=250;y<=310;y+=20) {swipe(x,y,y-70);assert(view.controls_open);pet_view_controls(&view,false);pump(220);}
    for(int x=60;x<=300;x+=240)for(int y=250;y<=290;y+=20) {swipe(x,y,y-70);assert(view.controls_open);pet_view_controls(&view,false);pump(220);}
    swipe(180,220,160);assert(!view.controls_open);
    horizontal(270,120,235);assert(!view.controls_open);
    pointer=(lv_point_t){180,270};pressed=true;pump(60);pointer.y=246;pump(40);
    assert(view.controls_dragging&&lv_obj_get_y(view.drawer)==336);
    uint16_t frozen[180*180];memcpy(frozen,pixels,sizeof(frozen));m.animation_ms+=500;pet_view_update(&view,NULL);assert(!memcmp(frozen,pixels,sizeof(frozen)));
    pump(160);pressed=false;pump(240);assert(!view.controls_open);
    tap(180,338,60);pump(220);assert(view.controls_open);tap(180,120,60);pump(220);assert(!view.controls_open);
    puts("PASS: 18 lower-zone origins including actor and side regions, finger tracking, frozen background, tap shortcut, button gaps and slide rejection");
    m.feedback_ms=m.emote_ms=0;m.last_ms=host_time_ms;
    pet_view_controls(&view,true);
    for(unsigned step=0;step<12;step++){pump(20);pet_model_tick(&m,host_time_ms);char name[64];snprintf(name,sizeof(name),"controls-open-%u",step);shot(&view,out,name,NULL);}
    pet_view_controls(&view,false);
    for(unsigned step=0;step<12;step++){pump(20);pet_model_tick(&m,host_time_ms);char name[64];snprintf(name,sizeof(name),"controls-close-%u",step);shot(&view,out,name,NULL);}
    pet_view_update(&view, "Save failed - restart device");
    shot(&view, out, "save-error", text);
    pet_view_update(&view,NULL);m.pet.age_ms=900000;m.feedback_ms=m.emote_ms=0;m.page=PET_HOME;pet_family_capture(&family,&m);
    view.page=PET_VIEW_FAMILY;view.starter=true;view.selection=1;shot(&view,out,"starter",text);
    view.starter=false;view.selection=0;shot(&view,out,"family",text);
    family.members[1]=family.members[0];family.members[1].profile.id.bytes[0]=9;family.members[1].profile.blood[0]=family.members[1].profile.blood[1]=1;family.count=2;
    horizontal(200,120,235);assert(view.selection==0);
    horizontal(200,235,120);assert(view.selection==1&&family.active==0);
    horizontal(200,235,120);assert(view.selection==1);
    horizontal(200,180,160);assert(view.selection==1);swipe(180,180,250);assert(view.selection==1);
    tap(80,94,60);assert(view.page==PET_VIEW_CARE&&family.active==0);
    view.page=PET_VIEW_FAMILY;view.selection=family.active;pet_view_update(&view,NULL);
    horizontal(200,235,120);tap(180,329,60);assert(view.page==PET_VIEW_CARE&&family.active==1);
    view.page=PET_VIEW_FAMILY;view.selection=1;pet_view_update(&view,NULL);horizontal(200,120,235);assert(view.selection==0);
    view.starter=true;view.selection=0;pet_view_update(&view,NULL);horizontal(200,235,120);assert(view.selection==1);horizontal(200,235,120);assert(view.selection==2);horizontal(200,235,120);assert(view.selection==2);view.starter=false;
    assert(lv_obj_has_flag(view.buttons[10],LV_OBJ_FLAG_HIDDEN));
    family.active=0;family.count=1;view.selection=0;pet_family_restore(&family,&m);
    puts("PASS: bidirectional bounded family/starter swipes, jitter and vertical rejection, browsing without activation, independent Back and explicit Select");
    view.page=PET_VIEW_NEARBY;social.nearby_count=1;social.nearby[0].kind=PET_LEAF;shot(&view,out,"nearby",text);
    social.state=PET_SOCIAL_VERIFY;social.code=482631;social.notice="Compare code, then confirm on both";shot(&view,out,"verify",text);
    social.state=PET_SOCIAL_LINKED;social.self=family.members[0].profile;social.peer=social.self;social.peer.id.bytes[0]=2;social.peer.blood[0]=social.peer.blood[1]=1;
    social.notice="Connected: greet, gift and play";pet_family_friend(&family,&social.peer,true);shot(&view,out,"linked",text);
    pet_profile_t parents[]={social.self,social.peer},children[2];uint8_t seed[32]={4};assert(pet_genetics(parents,seed,children));
    family.has_egg=true;family.egg=children[0];family.tending_egg=true;view.page=PET_VIEW_CARE;shot(&view,out,"egg",text);
    view.page=PET_VIEW_FAMILY;view.selection=0;pet_view_update(&view,NULL);horizontal(200,235,120);assert(view.selection==family.count);
    shot(&view,out,"family-egg",NULL);view.page=PET_VIEW_CARE;
    family.egg_ms=PET_EGG_TIME_MS;shot(&view,out,"hatch-ready",text);
    assert(pet_family_hatch(&family,&m));m.emote=PET_TOUCH;m.emote_ms=2200;shot(&view,out,"hatch-result",text);
    family.active=0;family.tending_egg=false;m.pet.age_ms=900000;m.pet.asleep=false;m.page=PET_HOME;view.page=PET_VIEW_CARE;
    static const uint8_t blood[6][2]={{0,0},{1,1},{2,2},{0,1},{0,2},{1,2}};
    FILE *animation_text=tmpfile();assert(animation_text);
    for(unsigned k=0;k<6;k++) {
        memcpy(family.members[0].profile.blood,blood[k],2);
        for(unsigned clip=0;clip<4;clip++) {
            uint32_t hashes[4];
            for(unsigned frame=0;frame<4;frame++) {
                m.animation_ms=clip==1?5400+frame*150:frame*200;m.emote_ms=clip>=2?2200-frame*180:0;m.emote=clip==2?PET_FEED:PET_PLAY;
                if(clip==2) {m.page=PET_FEEDING;m.feedback_ms=2200;}else {m.page=PET_HOME;m.feedback_ms=0;}
                char name[64];snprintf(name,sizeof(name),"character-%u-%u-%u",k,clip,frame);shot(&view,out,name,animation_text);
                hashes[frame]=2166136261u;for(unsigned i=0;i<180*180;i++)hashes[frame]=(hashes[frame]^pixels[i])*16777619u;
            }
            assert(hashes[0]!=hashes[1]||hashes[1]!=hashes[2]||hashes[2]!=hashes[3]);
        }
    }
    fclose(animation_text);
    puts("PASS: all six characters have distinct rendered idle/blink/feed/happy poses; family, nearby, verification and hatch pages fit the round-screen safe area");
    fclose(text);
    lv_scr_load(blank); pet_view_destroy(&view); pump(60);
    _lv_font_clean_up_fmt_txt(); lv_mem_buf_free_all();
    lv_mem_monitor(&after);
    assert(after.used_cnt == before.used_cnt);
    for (unsigned repeat = 0; repeat < 4; repeat++) {
        pet_view_create(&view, &m, pixels, on_input, &view);
        lv_scr_load(view.screen); pump(60);
        if(repeat%2) {
            pet_view_attach(&view,&family,&social);view.starter=true;view.page=PET_VIEW_FAMILY;pet_view_update(&view,NULL);
            pointer=(lv_point_t){235,200};pressed=true;pump(60);pointer.x=120;pump(40);pressed=false;pump(40);assert(view.family_animating);
        } else {pet_view_controls(&view,true);pump(20);assert(view.controls_animating);}
        lv_scr_load(blank); pet_view_destroy(&view); pump(60);
        _lv_font_clean_up_fmt_txt(); lv_mem_buf_free_all();
        lv_mem_monitor_t repeated; lv_mem_monitor(&repeated);
        assert(repeated.free_size == after.free_size && repeated.used_cnt == after.used_cnt);
    }
    printf("PASS: real LVGL touches, held press, English glyphs, round-safe labels, 64 KiB heap and repeated disposal; free=%u\n", after.free_size);
    free(pixels);
    memset(&disk, 0, sizeof(disk));
    strcpy(appdata,"/tmp/sugarat-app-XXXXXX");assert(mkdtemp(appdata));
    uint8_t old_record[PET_SAVE_BYTES]={0};
    if(legacy_start) {
        pet_model_t legacy;pet_model_init(&legacy,0);legacy.pet.age_ms=900000;legacy.pet.hunger=62;legacy.pet.health=55;
        pet_save_encode(&legacy,7,old_record);memcpy(disk.records[0],old_record,PET_SAVE_BYTES);disk.sizes[0]=PET_SAVE_BYTES;
    }
    ui_helloapp_start();pump(60);
    assert(shell_binds==1&&overlay_pause&&overlay_resume&&short_press&&disk.writes==0);
    if(!legacy_start) {horizontal(200,235,120);tap(180,329,60);ui_helloapp_poll();}
    assert(family_sequence()==1);
    if(legacy_start) {
        pet_family_store_t store;pet_family_t imported;assert(pet_family_store_open(&store,appdata,&imported));
        assert(imported.count==1&&imported.members[0].profile.age_ms==900000&&imported.members[0].care[0]==62&&imported.members[0].care[4]==55);
        assert(!pet_id_empty(imported.members[0].profile.id)&&!memcmp(disk.records[0],old_record,PET_SAVE_BYTES));
        puts("PASS: actual application V1-to-family migration preserves age, care and the original NVS bytes");
    }
    swipe(180,320,210);tap(110,216,60);
    for(unsigned i=0;i<20;i++){pump(100);ui_helloapp_poll();}
    assert(family_sequence()>=2&&disk.writes==0);
    overlay_pause();unsigned saved=family_sequence();
    for(unsigned i=0;i<10;i++){pump(100);ui_helloapp_poll();}
    assert(family_sequence()==saved);overlay_resume();
    assert(__wrap_sdgoods_launcher_return_to_launcher()==ESP_OK&&launcher_returns==1);
    char moved[224];snprintf(moved,sizeof(moved),"%s-moved",appdata);assert(rename(appdata,moved)==0);
    assert(__wrap_sdgoods_launcher_return_to_launcher()==ESP_FAIL&&launcher_returns==1);assert(short_press());sdgoods_app_pre_poweroff();
    for(unsigned i=0;i<2;i++){char name[256];snprintf(name,sizeof(name),"%s/family-%c.dat",moved,'a'+i);assert(unlink(name)==0);}assert(rmdir(moved)==0);
    puts("PASS: actual application selection/autosave, overlay pause/resume, SDK exit containment on real filesystem failure; legacy NVS remains untouched");
    lv_indev_delete(device); lv_disp_remove(display); lv_deinit();
}
int main(int argc, char **argv)
{
    assert(argc==2||argc==3);legacy_start=argc==3;pet_species_register(); model_tests(); storage_tests(); ui_tests(argv[1]);
    epet_modules_reset();
    return 0;
}
