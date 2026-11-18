/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** One LVGL owner, five-field home and finger-tracked bottom action drawer. */
#pragma once
#include "card_exchange.h"
#include "sugarat_watch.h"
#include "sdgoods_tap.h"
typedef enum {CARD_PAGE_HOME,CARD_PAGE_QR,CARD_PAGE_NEARBY,CARD_PAGE_VERIFY,CARD_PAGE_EDIT,CARD_PAGE_CONTACTS,CARD_PAGE_NOTICE} card_page_t;
typedef enum {CARD_ACT_QR,CARD_ACT_EXCHANGE,CARD_ACT_EDIT,CARD_ACT_CONTACTS,CARD_ACT_SELECT,CARD_ACT_CONFIRM,CARD_ACT_CANCEL,CARD_ACT_NEXT,CARD_ACT_EDIT_SECURITY,CARD_ACT_PREV,CARD_ACT_LANGUAGE,CARD_ACT_COUNT} card_action_t;
typedef void (*card_action_fn)(void *context,card_action_t action);
typedef struct card_view card_view_t;
typedef struct {card_view_t *view;card_action_t action;} card_button_context_t;
struct card_view {
    lv_obj_t *screen,*content,*shade,*drawer,*buttons[4],*button_labels[4],*language_label;
    card_page_t page;
    const card_state_t *state;
    const card_exchange_t *exchange;
    card_language_t language;
    card_message_t notice;
    const char *ssid,*password;
    unsigned selected;
    bool edit_connected,paused,open,visible,dragging,animating,render_pending;
    card_action_fn action;void *context;
    sdgoods_tap_ctx_t taps[10],shade_tap;
    card_button_context_t button_contexts[10];unsigned button_count;
    struct {bool active,moved,eligible,was_open;lv_point_t start;int axis,origin_y,max_distance;} gesture;
};
/** Create one screen and persistent drawer; the caller binds the BSP app shell and back gesture. */
void card_view_create(card_view_t *view,const card_state_t *state,const card_exchange_t *exchange,card_action_fn action,void *context);
/** Rebuild content only when no drawer movement is in progress. Caller owns presented strings. */
void card_view_refresh(card_view_t *view);
/** Show or dismiss the bottom drawer with one short position animation. */
void card_view_drawer(card_view_t *view,bool open);
/** Cancel owned animations and pointer state when platform controls take foreground ownership. */
void card_view_pause(card_view_t *view,bool paused);
/** Stop animations before deleting the screen and its callback contexts. */
void card_view_dispose(card_view_t *view);
/** Reject unsupported user glyphs before persisting or acknowledging a received card. */
bool card_view_profile_supported(const card_profile_t *profile);
