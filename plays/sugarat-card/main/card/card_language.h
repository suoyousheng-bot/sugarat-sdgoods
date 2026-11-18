/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** App-owned UI language and bilingual notices, independent of profile persistence. */
#pragma once
#include <stdbool.h>
typedef enum {CARD_ZH,CARD_EN} card_language_t;
typedef struct {const char *zh,*en;} card_message_t;
/** Read sugarat_card/lang; absent settings use English. Failure leaves the output untouched. */
bool card_language_load(card_language_t *language);
/** Commit and read back the preference. Failure leaves the saved outcome unconfirmed. */
bool card_language_save(card_language_t language);
