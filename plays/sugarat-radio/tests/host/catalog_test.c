/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 * Exercise the actual directory parser and HTTP fallback with deterministic responses.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "radio_catalog.h"
#include "radio_region.h"
#include "esp_http_client.h"

struct catalog_http { const char *response; int offset; bool complete; };
static struct catalog_http fixture;
static const char *curated, *browser;
static bool truncate_curated;
static int requests, browser_requests;
static char queried[512];
const char bundled_stations[] __asm__("_binary_stations_us_json_start") = BUNDLED_STATIONS;

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
    assert(!strcmp(config->user_agent,"SugaratRadio/0.1.0"));
    assert(!strstr(config->url,"ip-api") && !strstr(config->url,"ipwho"));
    bool hosted=strstr(config->url,"curated.test") != NULL;
    fixture=(struct catalog_http){.response=hosted?curated:browser,.complete=!hosted||!truncate_curated};
    ++requests;
    if(!hosted) { ++browser_requests;snprintf(queried,sizeof(queried),"%s",config->url);assert(strstr(queried,"countrycode=US")); }
    return &fixture;
}
int esp_http_client_open(esp_http_client_handle_t client,int size) { (void)size;return client->response?0:-1; }
int esp_http_client_fetch_headers(esp_http_client_handle_t client) { return (int)strlen(client->response); }
int esp_http_client_get_status_code(esp_http_client_handle_t client) { (void)client;return 200; }
int esp_http_client_read(esp_http_client_handle_t client,char *out,int capacity) {
    int count=(int)strlen(client->response)-client->offset;
    if(count>capacity)count=capacity;
    memcpy(out,client->response+client->offset,(size_t)count);client->offset+=count;return count;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client) { return client->complete; }
void esp_http_client_close(esp_http_client_handle_t client) { (void)client; }
void esp_http_client_cleanup(esp_http_client_handle_t client) { (void)client; }

int main(void) {
    const char *valid="[{\"name\":\"KEXP Seattle\",\"url_resolved\":\"https://stream.test/live\",\"codec\":\"AAC\",\"countrycode\":\"US\"}]";
    curated=valid;browser=NULL;
    assert(radio_catalog_load("seattle")==1 && browser_requests==0);
    assert(!strcmp(radio_catalog_country(),"US") && !strcmp(radio_catalog_city(),"Seattle, WA"));
    assert(radio_catalog_at(0)->freq_mhz>90.29f && radio_catalog_at(0)->freq_mhz<90.31f);
    curated="[{\"name\":\"Wrong region\",\"url\":\"https://stream.test/a\",\"codec\":\"MP3\",\"countrycode\":\"CN\"},"
            "{\"name\":\"Unsupported\",\"url\":\"https://stream.test/b\",\"codec\":\"OPUS\",\"countrycode\":\"US\"},"
            "{\"name\":\"Playlist\",\"url\":\"https://stream.test/c.m3u8\",\"codec\":\"AAC\",\"countrycode\":\"US\"},"
            "{\"name\":\"HLS flag\",\"url\":\"https://stream.test/d\",\"codec\":\"AAC\",\"countrycode\":\"US\",\"hls\":true},"
            "{\"name\":\"Bad protocol\",\"url\":\"file:///private/file.mp3\",\"codec\":\"MP3\",\"countrycode\":\"US\"}]";
    browser=valid;browser_requests=0;
    assert(radio_catalog_load("seattle")==1 && browser_requests>0);
    assert(!strcmp(radio_catalog_at(0)->name,"KEXP Seattle"));
    assert(!strcmp(radio_catalog_city(),"Seattle, WA"));
    curated=valid;truncate_curated=true;browser=NULL;
    assert(radio_catalog_load("")==3 && !strcmp(radio_catalog_city(),"US presets"));
    assert(!strcmp(radio_catalog_at(0)->name,"KEXP 90.3"));
    curated="invalid JSON";truncate_curated=false;
    assert(radio_catalog_load("retired-city")==3);
    assert(!strcmp(radio_region_find("retired-city")->label,"United States"));
    assert(radio_region_at(radio_region_count())==NULL);
    assert(radio_parse_freq("Jazz 24")==0.0f);
    assert(radio_parse_freq("KEXP")==90.3f && radio_parse_freq("NOTKEXP")==0.0f);
    assert(radio_catalog_at(-1)==NULL && radio_catalog_at(64)==NULL);
    puts("PASS: hosted-feed priority, US-only filtering, MP3/AAC filtering, malformed/truncated response fallback, deduplication, bundled URLs and US station identity.");
    return 0;
}
