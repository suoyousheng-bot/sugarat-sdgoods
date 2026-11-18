/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** UI queues language persistence and radio work; image reads and cover selection remain synchronous. */
#pragma once
#include "cv_link.h"
#define CV_PORTAL_ORIGIN "http://192.168.4.1"
typedef enum { CV_IDLE, CV_PHONE_MODE, CV_SEND_MODE, CV_RECEIVE_MODE } cv_mode_t;
typedef struct {
    cv_library_t library; cv_mode_t mode; cv_phase_t phase; cv_error_t error;
    unsigned peer_count,stations,progress; uint8_t own[6],peer[6],peers[CV_PEERS][6];
    uint32_t code; char ssid[32],password[20],token[33],saved[CV_ID_LENGTH+1];
    bool ready,changing;
    bool locale_pending;uint32_t locale_result;cv_error_t locale_error;
} cv_status_t;
typedef struct {
    void *user;
    bool (*status)(void *user,cv_status_t *status);
    cv_error_t (*pixels)(void *user,const char *id,uint16_t *pixels);
    cv_error_t (*select)(void *user,const char *id);
    /** Accept one save request; locale_pending and locale_result report completion, including failure. */
    bool (*request_locale)(void *user,cv_locale_t locale);
    bool (*mode)(void *user,cv_mode_t mode);
    bool (*phone_security)(void *user,bool password_required);
    bool (*offer)(void *user,const uint8_t peer[6],const char *id);
    bool (*confirm)(void *user);
} cv_backend_t;
