/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Device text is indexed by semantic keys; first use defaults to English. */
#pragma once
#include "cv_types.h"
typedef enum {
    C_TITLE, C_SWIPE, C_PHONE, C_EXCHANGE, C_SEND, C_RECEIVE, C_CONNECT,
    C_WIFI_STEP, C_PAGE_STEP, C_UPLOAD, C_CHOOSE, C_SEARCHING, C_WAIT,
    C_MY_BADGE, C_ASK_PEER, C_INVITE, C_FROM, C_LATER, C_ACCEPT, C_PAIR,
    C_COMPARE, C_MATCH, C_SENDING, C_RECEIVING, C_KEEP_NEAR, C_SAVED,
    C_SHOW, C_SENT, C_DONE, C_BACK, C_LANGUAGE, C_RETRY, C_NO_STORAGE,
    C_CLOSING, C_PHOTO, C_NO_PEERS, C_PREPARING, C_CANCEL, C_BADGE,
    C_NO_PASSWORD, C_PASSWORD, C_KEEP_WIFI, C_ENABLE_PASSWORD, C_DISABLE_PASSWORD, C_BACK_SWIPE, C_COUNT
} cv_copy_key_t;
/** @returns The selected language's immutable UI text. */
const char *cv_text(cv_locale_t locale, cv_copy_key_t key);
