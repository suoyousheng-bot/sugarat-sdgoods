/* Native checks never open Wi-Fi or serial hardware. The protocol has a separate two-endpoint test. */
#include "pet_radio.h"
bool pet_radio_open(pet_radio_t *r){r->notice="Native check: radio unavailable";return false;}
void pet_radio_close(pet_radio_t *r){(void)r;}
bool pet_radio_send(pet_radio_t *r,const uint8_t mac[6],const uint8_t *data,size_t size){(void)r;(void)mac;(void)data;(void)size;return false;}
void pet_radio_poll(pet_radio_t *r,pet_social_t *s){(void)r;(void)s;}
