/* SPDX-License-Identifier: Apache-2.0 */
/** Magic Bottle labels and frame composition on the LVGL thread. */
#include "bottle_view.h"
#include "sugarat_watch.h"
#include <stdio.h>

void bottle_view_create(bottle_view_t *view, uint16_t *pixels)
{
    view->descriptor = (lv_img_dsc_t){.header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = 360, .h = 360},
        .data_size = 360 * 360 * 2, .data = (uint8_t *)pixels};
    view->image = lv_img_create(view->screen);
    lv_img_set_src(view->image, &view->descriptor);
    lv_obj_clear_flag(view->image, LV_OBJ_FLAG_CLICKABLE);
    view->title = sw_label(view->screen, "Magic Bottle", SW_FONT_TITLE, SW_TONE_TEXT);
    lv_obj_align(view->title, LV_ALIGN_TOP_MID, 0, 70);
    view->hint = sw_label(view->screen, "Tilt to flow · Shake to splash\nTap: color · Swipe right: menu", SW_FONT_CAPTION, SW_TONE_MUTED);
    lv_obj_set_style_text_align(view->hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->hint, LV_ALIGN_TOP_MID, 0, 110);
}

/* Rows are 60 px tall on a 68 px pitch; each button's touch area covers half of each 8 px gap. */
#define ROW_H 60
#define ROW_PITCH 68
#define ROW_TOP 104
static lv_obj_t *menu_button(lv_obj_t *panel, const char *text, int action, int x, int y, int width, sw_button_variant_t variant)
{
    sw_tone_t tone=variant==SW_BUTTON_PLAIN?SW_TONE_RAISED:SW_TONE_TEAL;
    lv_obj_t *button=sw_button(panel,SW_ICON_NONE,text,variant,tone);
    sw_set_action(button,action); lv_obj_set_size(button,width,ROW_H); lv_obj_set_pos(button,x,y);
    lv_obj_set_ext_click_area(button,(ROW_PITCH-ROW_H)/2);
    lv_obj_set_style_bg_opa(button,variant==SW_BUTTON_TINTED?LV_OPA_70:LV_OPA_60,LV_STATE_PRESSED);
    return button;
}
static void row_button(lv_obj_t *panel, const char *text, int action, int row, sw_button_variant_t variant)
{
    menu_button(panel,text,action,44,ROW_TOP+row*ROW_PITCH,272,variant);
}
void bottle_view_hide_menu(bottle_view_t *view)
{
    if(view->menu) { lv_obj_del(view->menu); view->menu=NULL; }
}
void bottle_view_menu(bottle_view_t *view,const bottle_link_t *link,bool nearby,bool radio_failed)
{
    bottle_view_hide_menu(view);
    /* Opaque round overlay: it owns every touch while open and the bottle frame is not redrawn. */
    lv_obj_t *panel=lv_obj_create(view->screen); view->menu=panel;
    lv_obj_remove_style_all(panel); lv_obj_set_pos(panel,0,0); lv_obj_set_size(panel,360,360);
    lv_obj_set_style_radius(panel,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(panel,sw_color(SW_TONE_BG),0); lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);
    lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_EVENT_BUBBLE);
    unsigned amount=bottle_liquid_total(&link->liquid[bottle_link_local_index(link)]);
    unsigned percent=(amount*100+BOTTLE_CAPACITY/2)/BOTTLE_CAPACITY;
    const char *title="Magic Bottle",*hint="Tap bottle: color · Hold: reset";
    char status[96];
    if(radio_failed) { title="Radio unavailable"; hint="Return to Solo and retry"; }
    else switch(link->phase) {
        case BOTTLE_SOLO: if(nearby) { title="Two bottles"; hint="One hosts, the other joins"; } break;
        case BOTTLE_HOSTING: title="Waiting for friend"; snprintf(status,sizeof(status),"Room %04lX\nAsk your friend to join",(unsigned long)(link->room&65535)); hint=status; break;
        case BOTTLE_SCANNING: title="Finding rooms"; hint="Keep both devices nearby"; break;
        case BOTTLE_FOUND: title="Room found"; snprintf(status,sizeof(status),"Room %04lX\nCheck your friend's screen",(unsigned long)(link->room&65535)); hint=status; break;
        case BOTTLE_JOINING: title="Waiting for host"; snprintf(status,sizeof(status),"You: %04lX\nAsk your friend to accept",(unsigned long)(link->self&65535)); hint=status; break;
        case BOTTLE_INVITED: title="Join request"; snprintf(status,sizeof(status),"Friend %04lX\nAccept to share this bottle",(unsigned long)(link->peer&65535)); hint=status; break;
        case BOTTLE_LINKED: title="Bottles connected"; snprintf(status,sizeof(status),"Fill %u%% · Top edge is the mouth\nUpright: catch · Mouth down: pour",percent); hint=status; break;
        case BOTTLE_LOST: title="Transfer stopped"; hint=link->revision?"Keep nearby; upright to retry\nSolo starts a new bottle":"Pairing expired\nStart a new solo bottle"; break;
    }
    lv_obj_t *label=sw_label(panel,title,SW_FONT_HEADLINE,SW_TONE_TEXT);
    lv_obj_align(label,LV_ALIGN_TOP_MID,0,32);
    label=sw_label(panel,hint,SW_FONT_CAPTION,SW_TONE_MUTED);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0); lv_obj_align(label,LV_ALIGN_TOP_MID,0,62);
    if(link->phase==BOTTLE_SOLO && !radio_failed) {
        if(nearby) {
            row_button(panel,"Host room",BOTTLE_ACTION_HOST,0,SW_BUTTON_FILLED);
            row_button(panel,"Find friend",BOTTLE_ACTION_SCAN,1,SW_BUTTON_FILLED);
        } else {
            char fill[24]; snprintf(fill,sizeof(fill),"Fill %u%%",percent);
            menu_button(panel,"Color",BOTTLE_ACTION_COLOR,36,ROW_TOP,140,SW_BUTTON_TINTED);
            menu_button(panel,fill,BOTTLE_ACTION_FILL,184,ROW_TOP,140,SW_BUTTON_TINTED);
            row_button(panel,"Reset bottle",BOTTLE_ACTION_RESET,1,SW_BUTTON_TINTED);
            row_button(panel,"Play with friend",BOTTLE_ACTION_NEARBY,2,SW_BUTTON_FILLED);
        }
    } else {
        if(link->phase==BOTTLE_FOUND) {
            row_button(panel,"Join this room",BOTTLE_ACTION_JOIN,0,SW_BUTTON_FILLED);
            row_button(panel,"Next room",BOTTLE_ACTION_SCAN,1,SW_BUTTON_TINTED);
        }
        if(link->phase==BOTTLE_INVITED) row_button(panel,"Accept friend",BOTTLE_ACTION_ACCEPT,0,SW_BUTTON_FILLED);
        row_button(panel,"New solo bottle",BOTTLE_ACTION_SOLO,2,SW_BUTTON_PLAIN);
    }
    /* The bottom edge narrows the circle, so Back is shorter but keeps a wider touch margin. */
    lv_obj_t *back=menu_button(panel,"Back",BOTTLE_ACTION_BACK,116,ROW_TOP+3*ROW_PITCH,128,SW_BUTTON_PLAIN);
    lv_obj_set_height(back,46); lv_obj_set_ext_click_area(back,12);
}
