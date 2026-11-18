/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Watch gallery scenes: every component appears in at least one screen. */
#include "sw_gallery.h"
#include "sugarat_watch.h"
#include "sugarat_pixel.h"

static lv_obj_t *face(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *clock = sw_clock(s, "10:09", "周四 11月6日");
    lv_obj_align(clock, LV_ALIGN_CENTER, 0, -42);
    lv_obj_t *row = sw_row(s);
    lv_obj_set_style_pad_column(row, 14, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 82);
    sw_gauge(row, 78, 0, 100, 76, "电量", SW_TONE_GREEN);
    sw_gauge(row, 78, 40, 180, 72, "心率", SW_TONE_RED);
    sw_gauge(row, 78, -10, 40, 23, "℃", SW_TONE_TEAL);
    return s;
}

static lv_obj_t *analog(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *clock = sw_analog_clock(s, 344, 10, 9, 36);
    lv_obj_center(clock);
    lv_obj_t *date = sw_label(s, "周四 6", SW_FONT_CAPTION, SW_TONE_ORANGE);
    lv_obj_align(date, LV_ALIGN_CENTER, 0, 70);
    lv_obj_t *rings = sw_activity_rings(s, 52, 80, 60, 90);
    lv_obj_align(rings, LV_ALIGN_CENTER, 0, -72);
    lv_obj_move_background(rings);
    lv_obj_move_background(date);
    return s;
}

static lv_obj_t *activity(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *rings = sw_activity_rings(s, 176, 82, 64, 100);
    lv_obj_align(rings, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_t *row = sw_row(s);
    lv_obj_set_style_pad_column(row, 18, 0);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 226);
    sw_stat(row, "412", NULL, "活动 千卡", SW_TONE_PINK);
    sw_stat(row, "19", NULL, "锻炼 分钟", SW_TONE_GREEN);
    sw_stat(row, "9", NULL, "站立 小时", SW_TONE_TEAL);
    return s;
}

static lv_obj_t *apps(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *grid = sw_app_grid(s);
    static const struct { sw_icon_t icon; sw_tone_t tone; } items[] = {
        {SW_ICON_ROBOT, SW_TONE_GREEN},    {SW_ICON_HEART, SW_TONE_RED},     {SW_ICON_MUSIC, SW_TONE_PINK},
        {SW_ICON_TIMER, SW_TONE_ORANGE},   {SW_ICON_CLOUD, SW_TONE_BLUE},    {SW_ICON_SETTINGS, SW_TONE_RAISED},
        {SW_ICON_CHAT, SW_TONE_GREEN},     {SW_ICON_MAP, SW_TONE_TEAL},      {SW_ICON_CAMERA, SW_TONE_RAISED},
        {SW_ICON_CALENDAR, SW_TONE_RED},   {SW_ICON_RUN, SW_TONE_GREEN},     {SW_ICON_MAIL, SW_TONE_BLUE},
        {SW_ICON_BED, SW_TONE_PURPLE},     {SW_ICON_COMPASS, SW_TONE_RAISED},{SW_ICON_PHONE, SW_TONE_GREEN},
        {SW_ICON_BULB, SW_TONE_YELLOW},    {SW_ICON_IMAGE, SW_TONE_PINK},    {SW_ICON_GAME, SW_TONE_PURPLE},
        {SW_ICON_WALK, SW_TONE_ORANGE},
    };
    for (unsigned i = 0; i < sizeof(items) / sizeof(items[0]); ++i)
        sw_set_action(sw_app_icon(grid, items[i].icon, items[i].tone, NULL), (int)i);
    return s;
}

static lv_obj_t *settings(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *page = sw_page(s, "设置");
    lv_obj_t *item = sw_list_item(page, SW_ICON_WIFI, SW_TONE_BLUE, "无线网络", "Sugarat-Lab");
    sw_list_item_toggle(item, true);
    item = sw_list_item(page, SW_ICON_BLUETOOTH, SW_TONE_BLUE, "蓝牙", NULL);
    sw_list_item_value(item, "已连接");
    item = sw_list_item(page, SW_ICON_MOON, SW_TONE_PURPLE, "勿扰模式", "22:00 - 07:00");
    sw_list_item_toggle(item, false);
    item = sw_list_item(page, SW_ICON_SUN, SW_TONE_ORANGE, "亮度与显示", NULL);
    sw_list_item_chevron(item);
    sw_section(page, "系统");
    item = sw_list_item(page, SW_ICON_INFO, SW_TONE_RAISED, "关于本机", NULL);
    sw_list_item_chevron(item);
    sw_scroll_indicator(page);
    return s;
}

static lv_obj_t *buttons(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 10, 0);
    lv_obj_center(col);
    lv_obj_set_width(sw_button(col, SW_ICON_PLAY, "开始", SW_BUTTON_FILLED, SW_TONE_GREEN), 230);
    lv_obj_set_width(sw_button(col, SW_ICON_MIC, "语音", SW_BUTTON_TINTED, SW_TONE_BLUE), 230);
    lv_obj_set_width(sw_button(col, SW_ICON_NONE, "稍后提醒", SW_BUTTON_PLAIN, SW_TONE_TEXT), 230);
    lv_obj_t *row = sw_row(col);
    sw_icon_button(row, SW_ICON_PHONE, SW_TONE_GREEN);
    sw_icon_button(row, SW_ICON_CLOSE, SW_TONE_RED);
    sw_icon_button(row, SW_ICON_CHAT, SW_TONE_TEXT);
    return s;
}

static lv_obj_t *selection(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_center(col);
    static const char *const modes[] = {"日", "周", "月"};
    sw_segmented(col, modes, 3, 1);
    lv_obj_t *row = sw_row(col);
    lv_obj_set_style_pad_column(row, 16, 0);
    sw_label(row, "抬腕亮屏", SW_FONT_BODY, SW_TONE_TEXT);
    sw_toggle(row, true);
    sw_checkbox(col, "每天提醒", true);
    sw_checkbox(col, "仅工作日", false);
    return s;
}

static lv_obj_t *values(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_center(col);
    sw_label(col, "目标 千卡", SW_FONT_CAPTION, SW_TONE_PINK);
    sw_stepper(col, 100, 900, 420, 10);
    sw_slider(col, 0, 100, 64, SW_TONE_YELLOW);
    sw_progress(col, 72, SW_TONE_TEAL);
    lv_obj_t *row = sw_row(col);
    lv_obj_set_style_pad_column(row, 22, 0);
    sw_ring(row, 52, 68, SW_TONE_ORANGE);
    sw_spinner(row, 46, SW_TONE_GREEN);
    sw_badge(row, "12", SW_TONE_RED);
    sw_badge(row, NULL, SW_TONE_BLUE);
    return s;
}

static lv_obj_t *picker(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 12, 0);
    lv_obj_center(col);
    sw_label(col, "闹钟", SW_FONT_HEADLINE, SW_TONE_ORANGE);
    sw_roller(col, "06:30\n07:00\n07:30\n08:00\n08:30", 2, 3);
    lv_obj_set_width(sw_button(col, SW_ICON_CHECK, "设定", SW_BUTTON_FILLED, SW_TONE_ORANGE), 180);
    return s;
}

static lv_obj_t *music(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *player = sw_now_playing(s, "晴天", "周杰伦 · 叶惠美", true);
    lv_obj_center(player);
    sw_volume(s, 60);
    lv_obj_t *dots = sw_page_dots(s, 3, 1);
    (void)dots;
    return s;
}

static lv_obj_t *timer(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *ring = sw_timer(s, 336, 25 * 60, 17 * 60 + 42, SW_TONE_ORANGE);
    lv_obj_center(ring);
    lv_obj_align(lv_obj_get_child(ring, 0), LV_ALIGN_CENTER, 0, -30);
    lv_obj_t *label = sw_label(ring, "专注", SW_FONT_BODY, SW_TONE_ORANGE);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -84);
    lv_obj_t *row = sw_row(ring);
    lv_obj_set_style_pad_column(row, 24, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 56);
    sw_icon_button(row, SW_ICON_CLOSE, SW_TONE_TEXT);
    sw_icon_button(row, SW_ICON_PAUSE, SW_TONE_ORANGE);
    return s;
}

static lv_obj_t *workout(void)
{
    lv_obj_t *s = sw_screen();
    sw_status_bar(s, "10:09");
    lv_obj_t *col = sw_column(s);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_align(col, LV_ALIGN_CENTER, -10, 6);
    lv_obj_t *t = sw_label(col, "00:24:16", SW_FONT_NUM_M, SW_TONE_YELLOW);
    (void)t;
    sw_stat(col, "4.21", "公里", "户外跑步", SW_TONE_GREEN);
    lv_obj_t *row = sw_row(col);
    sw_stat(row, "152", "BPM", "心率", SW_TONE_RED);
    static const int16_t hr[] = {118, 124, 131, 129, 138, 144, 141, 149, 152, 150};
    sw_sparkline(row, 90, 44, hr, 10, SW_TONE_RED);
    return s;
}

static lv_obj_t *notification(void)
{
    lv_obj_t *s = sw_screen();
    sw_status_bar(s, "10:09");
    lv_obj_t *card = sw_notification(s, SW_ICON_ROBOT, SW_TONE_GREEN, "SUGARAT · 现在", "固件编译完成",
                                     "esp32s3 · 1.2 MB，已准备好烧录到徽章。");
    lv_obj_center(card);
    return s;
}

static lv_obj_t *alert(void)
{
    lv_obj_t *s = sw_screen();
    sw_alert(s, SW_ICON_BATTERY, SW_TONE_RED, "电量低", "剩余 10%，是否开启省电模式？", "开启", "取消");
    return s;
}

static lv_obj_t *loading(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_center(sw_loading(s, "正在同步…"));
    sw_toast(s, SW_ICON_CHECK, SW_TONE_GREEN, "已连接蓝牙", 0);
    return s;
}

static lv_obj_t *empty(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_center(sw_empty_state(s, SW_ICON_BELL, "没有通知", "新消息会显示在这里"));
    return s;
}

static lv_obj_t *control_center(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 14, 0);
    lv_obj_center(col);
    static const struct { sw_icon_t icon; sw_tone_t tone; bool on; } items[] = {
        {SW_ICON_WIFI, SW_TONE_BLUE, true},     {SW_ICON_BLUETOOTH, SW_TONE_BLUE, true}, {SW_ICON_MOON, SW_TONE_PURPLE, false},
        {SW_ICON_BELL, SW_TONE_RED, false},     {SW_ICON_BULB, SW_TONE_TEXT, true},      {SW_ICON_LOCK, SW_TONE_TEAL, false},
    };
    for (int r = 0; r < 2; ++r) {
        lv_obj_t *row = sw_row(col);
        lv_obj_set_style_pad_column(row, 14, 0);
        for (int c = 0; c < 3; ++c) sw_toggle_button(row, items[r * 3 + c].icon, items[r * 3 + c].tone, items[r * 3 + c].on);
    }
    lv_obj_t *row = sw_row(col);
    sw_icon(row, SW_ICON_SUN, SW_SIZE_S, SW_TONE_MUTED);
    lv_obj_set_width(sw_slider(row, 0, 100, 70, SW_TONE_TEXT), 170);
    sw_icon(row, SW_ICON_SUN, SW_SIZE_M, SW_TONE_TEXT);
    return s;
}

static lv_obj_t *companion(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *frame = sw_avatar_frame(s, 150, SW_TONE_GREEN);
    lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 36);
    sp_avatar(frame, SP_CHAR_CUBEY, 4, SP_MOOD_HAPPY);
    lv_obj_t *wave = sw_waveform(s, 160, 40, SW_TONE_BLUE);
    lv_obj_align(wave, LV_ALIGN_CENTER, 0, 52);
    lv_obj_t *voice = sw_voice_button(s, SW_VOICE_LISTENING);
    lv_obj_align(voice, LV_ALIGN_BOTTOM_MID, 0, -36);
    return s;
}

static lv_obj_t *chat(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *page = sw_page(s, "方糖仔");
    sw_chat_bubble(page, "明天早上会下雨吗？", true);
    sw_chat_bubble(page, "明早有小雨，8 点后转多云，记得带伞哦。", false);
    sw_chat_bubble(page, "提醒我 7:30 出门", true);
    sw_chat_bubble(page, "好的，已设置 7:30 提醒。", false);
    return s;
}

static lv_obj_t *charts(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 10, 0);
    lv_obj_center(col);
    sw_stat(col, "8,264", "步", "今日步数", SW_TONE_GREEN);
    static const int16_t steps[] = {3, 5, 9, 4, 7, 12, 8, 10, 6, 11, 9, 5};
    sw_bar_chart(col, 240, 90, steps, 12, SW_TONE_GREEN);
    sw_label(col, "6:00                 12:00                 18:00", SW_FONT_CAPTION, SW_TONE_MUTED);
    return s;
}

static lv_obj_t *tiles(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 12, 0);
    lv_obj_center(col);
    lv_obj_t *row = sw_row(col);
    lv_obj_set_style_pad_column(row, 12, 0);
    sw_tile(row, SW_ICON_SUN, SW_TONE_YELLOW, "23°", "上海 晴");
    sw_tile(row, SW_ICON_HEART, SW_TONE_RED, "72", "静息心率");
    row = sw_row(col);
    lv_obj_set_style_pad_column(row, 12, 0);
    sw_tile(row, SW_ICON_BED, SW_TONE_PURPLE, "7:42", "睡眠");
    sw_tile(row, SW_ICON_DROPLET, SW_TONE_TEAL, "98%", "血氧");
    return s;
}

static lv_obj_t *voice_states(void)
{
    lv_obj_t *s = sw_screen();
    lv_obj_t *col = sw_column(s);
    lv_obj_set_style_pad_row(col, 8, 0);
    lv_obj_center(col);
    for (int v = SW_VOICE_IDLE; v <= SW_VOICE_OFFLINE; ++v) sw_voice_button(col, (sw_voice_t)v);
    return s;
}

static const struct { const char *id; lv_obj_t *(*create)(void); } scenes[] = {
    {"face", face},          {"analog", analog},       {"activity", activity}, {"apps", apps},
    {"settings", settings},  {"buttons", buttons},     {"selection", selection}, {"values", values},
    {"picker", picker},      {"music", music},         {"timer", timer},       {"workout", workout},
    {"notification", notification}, {"alert", alert}, {"loading", loading},   {"empty", empty},
    {"control", control_center}, {"companion", companion}, {"chat", chat},     {"charts", charts},
    {"tiles", tiles},        {"voice", voice_states},
};

int sw_gallery_count(void)
{
    return (int)(sizeof(scenes) / sizeof(scenes[0]));
}

lv_obj_t *sw_gallery_create(int index)
{
    return index >= 0 && index < sw_gallery_count() ? scenes[index].create() : NULL;
}

const char *sw_gallery_id(int index)
{
    return index >= 0 && index < sw_gallery_count() ? scenes[index].id : NULL;
}
