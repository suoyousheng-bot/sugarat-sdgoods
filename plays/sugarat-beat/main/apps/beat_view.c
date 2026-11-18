/** Draw the cylindrical platforms and mascot without per-frame object creation. */
#include "beat_view.h"

#include <math.h>

extern const lv_img_dsc_t beat_backdrop;

enum {
    FIELD_WIDTH = 240,
    FIELD_HEIGHT = 160,
    BEAT_SPACING = 36,
    PLAYER_X = 58,
    FLOOR_Y = 123,
    BASE_Y = 155,
};

static void fill(lv_draw_ctx_t *ctx, int x, int y, int w, int h, int radius, uint32_t rgb, lv_opa_t opacity)
{
    const lv_area_t area = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.radius = radius;
    rect.bg_color = lv_color_hex(rgb);
    rect.bg_opa = opacity;
    lv_draw_rect(ctx, &rect, &area);
}

static void oval(lv_draw_ctx_t *ctx, int x, int y, int rx, int ry, uint32_t rgb, lv_opa_t opacity)
{
    for (int row = -ry; row <= ry; ++row) {
        const int span = (int)(rx * sqrtf(1.0f - (float)(row * row) / (float)(ry * ry)));
        fill(ctx, x - span, y + row, span * 2 + 1, 1, 0, rgb, opacity);
    }
}

static void line(lv_draw_ctx_t *ctx, int x1, int y1, int x2, int y2, int width, uint32_t rgb)
{
    lv_draw_line_dsc_t stroke;
    lv_draw_line_dsc_init(&stroke);
    stroke.color = lv_color_hex(rgb);
    stroke.width = width;
    stroke.round_start = 1;
    stroke.round_end = 1;
    const lv_point_t a = {x1, y1};
    const lv_point_t b = {x2, y2};
    lv_draw_line(ctx, &stroke, &a, &b);
}

static int surface(unsigned tile)
{
    static const int relief[] = {9, 17, 2, 21, 11, 6, 23, 13};
    return beat_level_hazard(tile) ? FLOOR_Y + 7 : FLOOR_Y - relief[tile % 8];
}

static void draw_pad(lv_draw_ctx_t *ctx, int x, int y, int bottom, bool hazard, bool finish, lv_opa_t light)
{
    const int height = bottom - y;
    oval(ctx, x, bottom, 16, 4, 0x191919, LV_OPA_50);
    if (hazard) {
        oval(ctx, x, y, 14, 6, 0x151515, LV_OPA_COVER);
        oval(ctx, x, y, 12, 4, 0x666666, LV_OPA_COVER);
        oval(ctx, x, y, 9, 3, 0x343434, LV_OPA_COVER);
        line(ctx, x - 6, y - 3, x + 6, y + 3, 2, 0xd4d4d4);
        line(ctx, x - 6, y + 3, x + 6, y - 3, 2, 0xd4d4d4);
        return;
    }

    if (light > 0) {
        oval(ctx, x, y - 2, 25, 12, 0x70ff40, light / 10);
        oval(ctx, x, y - 2, 22, 10, 0x70ff40, light / 7);
        oval(ctx, x, y, 18, 8, 0x68ff25, light / 4);
    }
    fill(ctx, x - 14, y, 29, height, 4, 0x141414, LV_OPA_COVER);
    static const uint32_t shine[] = {0x242424, 0x393939, 0x4e4e4e, 0x535353,
                                    0x434343, 0x343434, 0x292929, 0x202020};
    for (unsigned i = 0; i < sizeof(shine) / sizeof(shine[0]); ++i) {
        fill(ctx, x - 13 + (int)i * 2, y + 2, 2, height - 5, 0, shine[i], LV_OPA_COVER);
    }
    fill(ctx, x + 7, y + 2, 6, height - 5, 1, 0x090909, LV_OPA_COVER);
    oval(ctx, x, y, 14, 6, 0x171717, LV_OPA_COVER);
    oval(ctx, x, y - 1, 13, 5, 0x999999, LV_OPA_COVER);
    oval(ctx, x, y - 1, 10, 4, 0x252525, LV_OPA_COVER);
    oval(ctx, x, y - 1, 7, 3, 0x696969, LV_OPA_COVER);
    oval(ctx, x, y - 1, 4, 2, finish ? 0xf3f3f3 : 0x202020, LV_OPA_COVER);
    if (light > 0) {
        oval(ctx, x, y - 1, 13, 5, 0x76ff39, light);
        oval(ctx, x, y - 1, 9, 3, 0x182a10, light);
        oval(ctx, x, y - 1, 5, 2, 0xa5ff66, light);
        fill(ctx, x - 10, y + 5, 3, 9, 1, 0x71ff29, light / 2);
    }
}

static void draw_urchin(lv_draw_ctx_t *ctx, int x, int feet, float fraction, float lift, bool chirping)
{
    static const struct { int x, y, length; } spines[] = {
        {1, -16, 10}, {8, -14, 8}, {13, -9, 10}, {16, -3, 7}, {16, 4, 9},
        {13, 10, 7}, {7, 14, 6}, {0, 16, 5}, {-7, 14, 6}, {-13, 10, 9},
        {-16, 4, 7}, {-16, -3, 9}, {-13, -10, 8}, {-7, -14, 11},
    };
    const bool grounded = lift < 4.0f;
    const int stretch = grounded ? -2 : fraction < .3f ? 2 : 0;
    const int rx = 13 - stretch;
    const int ry = 12 + stretch;
    const int y = feet - ry - (grounded ? 3 : 5);

    if (!grounded) {
        fill(ctx, x - 26, y + 7, 5, 5, 1, 0x222222, LV_OPA_70);
        fill(ctx, x - 33, y + 11, 4, 4, 1, 0x111111, LV_OPA_70);
        fill(ctx, x - 40, y + 15, 2, 2, 1, 0x111111, LV_OPA_50);
    }

    lv_draw_rect_dsc_t spike;
    lv_draw_rect_dsc_init(&spike);
    spike.bg_color = lv_color_hex(0x151515);
    spike.bg_opa = LV_OPA_COVER;
    for (unsigned i = 0; i < sizeof(spines) / sizeof(spines[0]); ++i) {
        const int dx = spines[i].x;
        const int dy = spines[i].y;
        const int length = spines[i].length;
        const int root_x = x + dx * (rx - 3) / 16;
        const int root_y = y + dy * (ry - 3) / 16;
        const int half_x = dy * 3 / 16;
        const int half_y = dx * 3 / 16;
        const int vertical_spine = dy > 6 && grounded ? 3 : length;
        const lv_point_t points[] = {
            {root_x - half_x, root_y + half_y},
            {x + dx * (rx + length) / 16, y + dy * (ry + vertical_spine) / 16},
            {root_x + half_x, root_y - half_y},
        };
        lv_draw_triangle(ctx, &spike, points);
    }

    fill(ctx, x - rx, y - ry, 2 * rx + 1, 2 * ry + 1, LV_RADIUS_CIRCLE, 0x101010, LV_OPA_COVER);
    oval(ctx, x - 5, y - 4, 5, 3, 0x3e3e3e, LV_OPA_COVER);
    fill(ctx, x - 7, y + 1, 2, 2, 1, 0x585858, LV_OPA_COVER);
    fill(ctx, x - 4, y + 4, 2, 2, 1, 0x585858, LV_OPA_COVER);
    fill(ctx, x + 1, y - 4, 10, 6, 3, 0xe5e5e5, LV_OPA_COVER);
    fill(ctx, x + 6, y - 3, 4, 4, 2, 0x282828, LV_OPA_COVER);
    fill(ctx, x + 7, y - 3, 2, 2, 1, 0xa1fb63, LV_OPA_COVER);
    fill(ctx, x + 3, y + 4, 5, chirping ? 5 : 2, 2, 0xc0c0c0, LV_OPA_COVER);
}

static lv_opa_t pad_light(const beat_view_t *view, unsigned index)
{
    if (view->preview) {
        return index == 0 || index == 2 ? LV_OPA_90 : LV_OPA_TRANSP;
    }
    if (view->level.result != BEAT_ACTIVE) {
        return view->level.result == BEAT_CLEARED && index == FINISH_TILE ? LV_OPA_COVER : LV_OPA_TRANSP;
    }
    if ((int)index == view->cue_pad || (view->level.tap_queued && index == view->level.to)) {
        return LV_OPA_COVER;
    }
    if (view->ms >= view->level.launch_ms && index == view->level.from) {
        const uint32_t elapsed = view->ms - view->level.launch_ms;
        return elapsed < 210 ? (lv_opa_t)(242 * (210 - elapsed) / 210) : LV_OPA_TRANSP;
    }
    return LV_OPA_TRANSP;
}

static void draw_scene(lv_event_t *event)
{
    const beat_view_t *view = lv_event_get_user_data(event);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t field;
    lv_obj_get_coords(lv_event_get_target(event), &field);
    const lv_area_t *incoming = ctx->clip_area;
    lv_area_t clip;
    if (!_lv_area_intersect(&clip, incoming, &field)) {
        return;
    }
    ctx->clip_area = &clip;

    float tile, lift;
    beat_level_pose(&view->level, view->ms, &tile, &lift);
    const unsigned first = tile >= 2 ? (unsigned)tile - 2 : 0;
    for (unsigned i = first; i <= FINISH_TILE; ++i) {
        const int x = PLAYER_X + (int)lroundf((i - tile) * BEAT_SPACING);
        if (x > FIELD_WIDTH + 18) {
            break;
        }
        if (x >= -18) {
            draw_pad(ctx, field.x1 + x, field.y1 + surface(i), field.y1 + BASE_Y,
                     beat_level_hazard(i), i == FINISH_TILE, pad_light(view, i));
        }
    }
    const unsigned from = view->level.from;
    const unsigned to = view->level.to;
    const float fraction = from != to ? (tile - from) / (to - from) : 0.0f;
    const uint32_t elapsed = view->ms >= view->level.launch_ms ? view->ms - view->level.launch_ms : 0;
    const float settling = from != to && elapsed > TAP_LATE_MS
        ? (float)(elapsed - TAP_LATE_MS) / (float)((to - from) * BEAT_MS - TAP_LATE_MS) : 0.0f;
    const float y = surface(from) + (surface(to) - surface(from)) * settling - .82f * lift;
    const bool chirping = view->level.result == BEAT_ACTIVE && to - from == 2
        && elapsed < 172;
    draw_urchin(ctx, field.x1 + PLAYER_X, field.y1 + (int)lroundf(y), fraction, lift, chirping);
    ctx->clip_area = incoming;
}

static void draw_progress(lv_event_t *event)
{
    const beat_view_t *view = lv_event_get_user_data(event);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event), &area);
    const uint32_t finish_ms = COUNT_IN_MS + FINISH_TILE * BEAT_MS;
    const uint32_t position = LV_MIN(view->ms, finish_ms);
    const int length = (int)(176 * position / finish_ms);
    fill(ctx, area.x1, area.y1, 180, 8, 4, 0x2a2a2a, LV_OPA_COVER);
    fill(ctx, area.x1 + 2, area.y1 + 2, 176, 4, 2, 0x636363, LV_OPA_COVER);
    if (length > 0) {
        fill(ctx, area.x1 + 2, area.y1 + 2, length, 4, 2, 0x8afa3d, LV_OPA_COVER);
    }
}

static lv_obj_t *art(lv_obj_t *screen, int w, int h, int y, lv_event_cb_t draw, beat_view_t *view)
{
    lv_obj_t *obj = lv_obj_create(screen);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, draw, LV_EVENT_DRAW_MAIN, view);
    return obj;
}

void beat_view_create(beat_view_t *view, lv_obj_t *screen)
{
    lv_obj_t *image = lv_img_create(screen);
    lv_img_set_src(image, &beat_backdrop);
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    view->scene = art(screen, FIELD_WIDTH, FIELD_HEIGHT, 88, draw_scene, view);
    view->progress = art(screen, 180, 8, 253, draw_progress, view);
}

void beat_view_draw(beat_view_t *view, const beat_level_t *level, uint32_t ms, bool preview, int cue_pad)
{
    view->level = *level;
    view->ms = ms;
    view->preview = preview;
    view->cue_pad = cue_pad;
    lv_obj_invalidate(view->scene);
    lv_obj_invalidate(view->progress);
}
