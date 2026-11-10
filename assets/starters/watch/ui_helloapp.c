/* SPDX-License-Identifier: MIT */
/** Connect a theme page to the existing badge shell; the button changes only local UI. */
#include "ui_helloapp.h"
#include "sdgoods_board.h"
#include "sugarat_watch.h"

static lv_obj_t *screen;

static void screen_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) == screen) screen = NULL;
}

static void close_screen(void)
{
    if (screen) lv_obj_del(screen);
}

static void button_clicked(lv_event_t *event)
{
    (void)event;
    sw_toast(screen, SW_ICON_CHECK, SW_TONE_GREEN, "Hello!", 1500);
}

void ui_helloapp_start(void)
{
    if (screen) {
        lv_scr_load(screen);
        return;
    }
    sw_init();
    screen = sw_screen();
    lv_obj_t *page = sw_page(screen, "Sugarat");
    lv_obj_t *button = sw_button(page, SW_ICON_PLAY, "Tap me", SW_BUTTON_FILLED, SW_TONE_BLUE);
    lv_obj_add_event_cb(button, button_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screen, screen_deleted, LV_EVENT_DELETE, NULL);
    lv_scr_load(screen);
    sdgoods_app_shell_bind(screen);
    sdgoods_app_shell_set_exit_cb(close_screen);
}

void ui_helloapp_poll(void) {}
