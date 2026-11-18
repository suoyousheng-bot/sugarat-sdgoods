/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Game gallery scenes: shared components in pixel style plus RPG-only components. */
#include "sg_gallery.h"
#include "sugarat_game.h"
#include "sugarat_pixel.h"

static lv_obj_t *centered_column(lv_obj_t *s, lv_coord_t gap)
{
    lv_obj_t *col = sg_column(s);
    lv_obj_set_style_pad_row(col, gap, 0);
    lv_obj_center(col);
    return col;
}

static lv_obj_t *title(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 6);
    sp_avatar(col, SP_CHAR_DRAGON, 3, SP_MOOD_HAPPY);
    sg_title_screen(col, "糖糖冒险", "SUGARAT QUEST", "PRESS START");
    return s;
}

static lv_obj_t *face(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *clock = sg_clock(s, "10:09", "周四 11月6日");
    lv_obj_align(clock, LV_ALIGN_CENTER, 0, -46);
    lv_obj_t *row = sg_row(s);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 78);
    sg_gauge(row, 80, 0, 100, 76, "电量", SG_TONE_GREEN);
    sg_gauge(row, 80, 40, 180, 72, "心率", SG_TONE_RED);
    sg_gauge(row, 80, -10, 40, 23, "℃", SG_TONE_TEAL);
    return s;
}

static lv_obj_t *analog(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_center(sg_analog_clock(s, 340, 10, 9, 36));
    lv_obj_t *date = sg_label(s, "THU 06", SG_FONT_CAPTION, SG_TONE_ACCENT);
    lv_obj_align(date, LV_ALIGN_CENTER, 0, 64);
    lv_obj_move_background(date);
    return s;
}

static lv_obj_t *hud(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 12);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    sg_hearts(col, 5, 3);
    sg_hp_bar(col, SG_ICON_HEART, 72, 100, SG_TONE_GREEN);
    sg_hp_bar(col, SG_ICON_BOLT, 18, 80, SG_TONE_BLUE);
    sg_level(col, 12, 64);
    lv_obj_t *row = sg_row(col);
    lv_obj_set_style_pad_column(row, 18, 0);
    sg_counter(row, SG_ICON_COIN, 1280, SG_TONE_YELLOW);
    sg_counter(row, SG_ICON_GEM, 36, SG_TONE_TEAL);
    return s;
}

static lv_obj_t *dialog(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *hero = sp_avatar(s, SP_CHAR_WIZARD, 4, SP_MOOD_IDLE);
    lv_obj_align(hero, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_t *box = sg_dialog(s, "小法师", "固件已编译完成，现在烧录吗？");
    lv_obj_align(box, LV_ALIGN_BOTTOM_MID, 0, -56);
    sg_dialog_skip(box);
    return s;
}

static lv_obj_t *portrait(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *box = sg_dialog(s, "比特猫", "电量只剩 10% 了，喵～");
    lv_obj_center(box);
    sp_avatar(sg_dialog_portrait(box), SP_CHAR_CAT, 2, SP_MOOD_SAD);
    sg_dialog_skip(box);
    return s;
}

static lv_obj_t *menu(void)
{
    lv_obj_t *s = sg_screen();
    static const char *const options[] = {"继续游戏", "新的冒险", "道具背包", "设置"};
    lv_obj_center(sg_menu(s, options, 4, 1));
    return s;
}

static lv_obj_t *inventory(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 12);
    sg_label(col, "背包", SG_FONT_BODY, SG_TONE_ACCENT);
    lv_obj_t *inv = sg_inventory(col, 4, 3);
    static const struct { sg_icon_t icon; sg_tone_t tone; uint16_t count; } items[] = {
        {SG_ICON_POTION, SG_TONE_RED, 3},  {SG_ICON_KEY, SG_TONE_YELLOW, 1},  {SG_ICON_GEM, SG_TONE_TEAL, 12},
        {SG_ICON_SHIELD, SG_TONE_BLUE, 0}, {SG_ICON_COIN, SG_TONE_YELLOW, 99}, {SG_ICON_HEART, SG_TONE_PINK, 2},
        {SG_ICON_MAP, SG_TONE_ORANGE, 0},  {SG_ICON_BOLT, SG_TONE_YELLOW, 5},
    };
    for (uint8_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) sg_inventory_set(inv, i, items[i].icon, items[i].tone, items[i].count);
    sg_inventory_select(inv, 2);
    return s;
}

static lv_obj_t *achievement(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 8);
    lv_obj_set_style_translate_y(col, 30, 0);
    sp_avatar(col, SP_CHAR_CHICK, 4, SP_MOOD_HAPPY);
    sg_label(col, "第一次烧录成功", SG_FONT_CAPTION, SG_TONE_MUTED);
    sg_achievement(s, SG_ICON_TROPHY, "固件大师", 0);
    return s;
}

static lv_obj_t *apps(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *grid = sg_app_grid(s);
    static const struct { sg_icon_t icon; sg_tone_t tone; const char *label; } items[] = {
        {SG_ICON_ROBOT, SG_TONE_GREEN, "助手"},  {SG_ICON_MUSIC, SG_TONE_PINK, "音乐"},   {SG_ICON_TIMER, SG_TONE_ORANGE, "计时"},
        {SG_ICON_HEART, SG_TONE_RED, "健康"},    {SG_ICON_GAME, SG_TONE_PURPLE, "游戏"},  {SG_ICON_CLOUD, SG_TONE_BLUE, "天气"},
        {SG_ICON_CHAT, SG_TONE_TEAL, "消息"},    {SG_ICON_IMAGE, SG_TONE_YELLOW, "相册"}, {SG_ICON_SETTINGS, SG_TONE_RAISED, "设置"},
    };
    for (unsigned i = 0; i < sizeof(items) / sizeof(items[0]); ++i)
        sg_set_action(sg_app_icon(grid, items[i].icon, items[i].tone, items[i].label), (int)i);
    return s;
}

static lv_obj_t *settings(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *page = sg_page(s, "设置");
    lv_obj_t *item = sg_list_item(page, SG_ICON_WIFI, SG_TONE_BLUE, "无线网络", "Sugarat-Lab");
    sg_list_item_toggle(item, true);
    item = sg_list_item(page, SG_ICON_BLUETOOTH, SG_TONE_BLUE, "蓝牙", NULL);
    sg_list_item_value(item, "已连接");
    item = sg_list_item(page, SG_ICON_MOON, SG_TONE_PURPLE, "勿扰模式", "22:00-07:00");
    sg_list_item_toggle(item, false);
    item = sg_list_item(page, SG_ICON_SUN, SG_TONE_ORANGE, "亮度", NULL);
    sg_list_item_chevron(item);
    sg_section(page, "系统");
    item = sg_list_item(page, SG_ICON_INFO, SG_TONE_TEXT, "关于本机", NULL);
    sg_list_item_chevron(item);
    sg_scroll_indicator(page);
    return s;
}

static lv_obj_t *buttons(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 10);
    lv_obj_set_width(sg_button(col, SG_ICON_PLAY, "开始", SG_BUTTON_FILLED, SG_TONE_GREEN), 230);
    lv_obj_set_width(sg_button(col, SG_ICON_MIC, "语音", SG_BUTTON_TINTED, SG_TONE_BLUE), 230);
    lv_obj_set_width(sg_button(col, SG_ICON_NONE, "稍后提醒", SG_BUTTON_PLAIN, SG_TONE_TEXT), 230);
    lv_obj_t *row = sg_row(col);
    sg_icon_button(row, SG_ICON_PHONE, SG_TONE_GREEN);
    sg_icon_button(row, SG_ICON_CLOSE, SG_TONE_RED);
    sg_icon_button(row, SG_ICON_CHAT, SG_TONE_TEXT);
    return s;
}

static lv_obj_t *selection(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 16);
    static const char *const modes[] = {"日", "周", "月"};
    sg_segmented(col, modes, 3, 1);
    lv_obj_t *row = sg_row(col);
    lv_obj_set_style_pad_column(row, 16, 0);
    sg_label(row, "抬腕亮屏", SG_FONT_BODY, SG_TONE_TEXT);
    sg_toggle(row, true);
    sg_checkbox(col, "每天提醒", true);
    sg_checkbox(col, "仅工作日", false);
    return s;
}

static lv_obj_t *values(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 14);
    sg_label(col, "目标", SG_FONT_CAPTION, SG_TONE_PINK);
    sg_stepper(col, 100, 900, 420, 10);
    sg_slider(col, 0, 100, 64, SG_TONE_YELLOW);
    sg_progress(col, 70, SG_TONE_TEAL);
    lv_obj_t *row = sg_row(col);
    lv_obj_set_style_pad_column(row, 20, 0);
    sg_ring(row, 56, 68, SG_TONE_ORANGE);
    sg_spinner(row, 50, SG_TONE_GREEN);
    sg_badge(row, "12", SG_TONE_RED);
    sg_badge(row, NULL, SG_TONE_BLUE);
    return s;
}

static lv_obj_t *picker(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 12);
    sg_label(col, "闹钟", SG_FONT_BODY, SG_TONE_ORANGE);
    sg_roller(col, "06:30\n07:00\n07:30\n08:00\n08:30", 2, 3);
    lv_obj_set_width(sg_button(col, SG_ICON_CHECK, "设定", SG_BUTTON_FILLED, SG_TONE_ORANGE), 180);
    return s;
}

static lv_obj_t *music(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_center(sg_now_playing(s, "晴天", "周杰伦 · 叶惠美", true));
    sg_volume(s, 60);
    sg_page_dots(s, 3, 1);
    return s;
}

static lv_obj_t *timer(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *ring = sg_timer(s, 336, 25 * 60, 17 * 60 + 42, SG_TONE_ORANGE);
    lv_obj_center(ring);
    lv_obj_align(lv_obj_get_child(ring, 1), LV_ALIGN_CENTER, 0, -26);
    lv_obj_t *label = sg_label(ring, "专注", SG_FONT_BODY, SG_TONE_ORANGE);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -84);
    lv_obj_t *row = sg_row(ring);
    lv_obj_set_style_pad_column(row, 22, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 56);
    sg_icon_button(row, SG_ICON_CLOSE, SG_TONE_TEXT);
    sg_icon_button(row, SG_ICON_PAUSE, SG_TONE_ORANGE);
    return s;
}

static lv_obj_t *notification(void)
{
    lv_obj_t *s = sg_screen();
    sg_status_bar(s, "10:09");
    lv_obj_center(sg_notification(s, SG_ICON_ROBOT, SG_TONE_GREEN, "SUGARAT", "固件编译完成", "esp32s3 · 1.2MB，已准备好烧录到徽章。"));
    return s;
}

static lv_obj_t *alert(void)
{
    lv_obj_t *s = sg_screen();
    sg_alert(s, SG_ICON_BATTERY, SG_TONE_RED, "电量低", "剩余 10%，开启省电模式？", "开启", "取消");
    return s;
}

static lv_obj_t *loading(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_center(sg_loading(s, "LOADING..."));
    sg_toast(s, SG_ICON_CHECK, SG_TONE_GREEN, "已连接蓝牙", 0);
    return s;
}

static lv_obj_t *empty(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_center(sg_empty_state(s, SG_ICON_BELL, "没有通知", "新消息会显示在这里"));
    return s;
}

static lv_obj_t *control_center(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 14);
    static const struct { sg_icon_t icon; sg_tone_t tone; bool on; } items[] = {
        {SG_ICON_WIFI, SG_TONE_BLUE, true}, {SG_ICON_BLUETOOTH, SG_TONE_BLUE, true}, {SG_ICON_MOON, SG_TONE_PURPLE, false},
        {SG_ICON_BELL, SG_TONE_RED, false}, {SG_ICON_BULB, SG_TONE_YELLOW, true},    {SG_ICON_LOCK, SG_TONE_TEAL, false},
    };
    for (int r = 0; r < 2; ++r) {
        lv_obj_t *row = sg_row(col);
        lv_obj_set_style_pad_column(row, 14, 0);
        for (int c = 0; c < 3; ++c) sg_toggle_button(row, items[r * 3 + c].icon, items[r * 3 + c].tone, items[r * 3 + c].on);
    }
    lv_obj_t *row = sg_row(col);
    sg_icon(row, SG_ICON_SUN, SG_SIZE_S, SG_TONE_MUTED);
    lv_obj_set_width(sg_slider(row, 0, 100, 70, SG_TONE_YELLOW), 160);
    sg_icon(row, SG_ICON_SUN, SG_SIZE_M, SG_TONE_TEXT);
    return s;
}

static lv_obj_t *companion(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *frame = sg_avatar_frame(s, 150, SG_TONE_ACCENT);
    lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 34);
    sp_avatar(frame, SP_CHAR_CUBEY, 4, SP_MOOD_HAPPY);
    lv_obj_t *wave = sg_waveform(s, 184, 56, SG_TONE_BLUE);
    lv_obj_align(wave, LV_ALIGN_CENTER, 0, 52);
    lv_obj_t *voice = sg_voice_button(s, SG_VOICE_LISTENING);
    lv_obj_align(voice, LV_ALIGN_BOTTOM_MID, 0, -34);
    return s;
}

static lv_obj_t *chat(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *page = sg_page(s, "方糖仔");
    sg_chat_bubble(page, "明天早上会下雨吗？", true);
    sg_chat_bubble(page, "明早有小雨，8点后转多云，记得带伞哦。", false);
    sg_chat_bubble(page, "提醒我 7:30 出门", true);
    return s;
}

static lv_obj_t *charts(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 10);
    sg_stat(col, "8264", "步", "今日步数", SG_TONE_GREEN);
    static const int16_t steps[] = {3, 5, 9, 4, 7, 12, 8, 10, 6, 11, 9, 5};
    sg_bar_chart(col, 240, 90, steps, 12, SG_TONE_GREEN);
    static const int16_t hr[] = {62, 64, 70, 76, 72, 80, 88, 84, 78, 74, 70, 68};
    sg_sparkline(col, 240, 40, hr, 12, SG_TONE_RED);
    return s;
}

static lv_obj_t *tiles(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 12);
    lv_obj_t *row = sg_row(col);
    lv_obj_set_style_pad_column(row, 12, 0);
    sg_tile(row, SG_ICON_SUN, SG_TONE_YELLOW, "23°", "上海 晴");
    sg_tile(row, SG_ICON_HEART, SG_TONE_RED, "72", "静息心率");
    row = sg_row(col);
    lv_obj_set_style_pad_column(row, 12, 0);
    sg_tile(row, SG_ICON_BED, SG_TONE_PURPLE, "7:42", "睡眠");
    sg_tile(row, SG_ICON_DROPLET, SG_TONE_TEAL, "98%", "血氧");
    return s;
}

static lv_obj_t *voice_states(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *col = centered_column(s, 6);
    for (int v = SG_VOICE_IDLE; v <= SG_VOICE_OFFLINE; ++v) sg_voice_button(col, (sg_voice_t)v);
    return s;
}

static lv_obj_t *icons(void)
{
    lv_obj_t *s = sg_screen();
    lv_obj_t *grid = sg_row(s);
    lv_obj_set_width(grid, 300);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_center(grid);
    static const sg_tone_t tones[] = {SG_TONE_ACCENT, SG_TONE_TEXT, SG_TONE_TEAL, SG_TONE_PINK, SG_TONE_GREEN};
    for (int i = 1; i < SG_ICON_COUNT; ++i) sg_icon(grid, (sg_icon_t)i, SG_SIZE_M, tones[i % 5]);
    return s;
}

static const struct { const char *id; lv_obj_t *(*create)(void); } scenes[] = {
    {"title", title},       {"hud", hud},             {"dialog", dialog},       {"portrait", portrait},
    {"menu", menu},         {"inventory", inventory}, {"achievement", achievement}, {"face", face},
    {"analog", analog},     {"apps", apps},           {"settings", settings},   {"buttons", buttons},
    {"selection", selection}, {"values", values},     {"picker", picker},       {"music", music},
    {"timer", timer},       {"notification", notification}, {"alert", alert}, {"loading", loading},
    {"empty", empty},       {"control", control_center}, {"companion", companion}, {"chat", chat},
    {"charts", charts},     {"tiles", tiles},         {"voice", voice_states},  {"icons", icons},
};

int sg_gallery_count(void)
{
    return (int)(sizeof(scenes) / sizeof(scenes[0]));
}

lv_obj_t *sg_gallery_create(int index)
{
    return index >= 0 && index < sg_gallery_count() ? scenes[index].create() : NULL;
}

const char *sg_gallery_id(int index)
{
    return index >= 0 && index < sg_gallery_count() ? scenes[index].id : NULL;
}
