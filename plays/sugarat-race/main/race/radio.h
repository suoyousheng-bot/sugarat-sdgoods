/* SPDX-License-Identifier: Apache-2.0 */
/** Worker-owned ESP-NOW transport; Wi-Fi callbacks enqueue validated-size datagrams only. */
#pragma once
#include "link.h"
#include "esp_err.h"
typedef struct{void *queue;bool wifi_owned,event_owned,now_owned;}race_radio;
/** On failure all acquired resources are released; single-player remains available. */
esp_err_t race_radio_open(race_radio *radio);
/** Detach callback borrowing before releasing the queue and owned Wi-Fi resources. */
void race_radio_close(race_radio *radio);
/** Send adapter; session traffic repeats state and start acknowledgements. */
void race_radio_send(void *owner,const uint8_t mac[6],const uint8_t *data,size_t size);
/** Drain at most twelve queued datagrams into the worker-owned session. */
void race_radio_poll(race_radio *radio,race_link *link,uint32_t now);
