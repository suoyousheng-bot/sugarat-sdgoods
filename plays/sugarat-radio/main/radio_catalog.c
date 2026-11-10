/* SPDX-License-Identifier: Apache-2.0
 * Modified for Sugarat Radio by Deotaland, 2026.
 * US directory: configured curated feed, Radio Browser, bundled development stations.
 * Runs only on the catalog worker; the UI reads after its completion handoff.
 */
#include "radio_catalog.h"
#include "radio_station_identity.h"
#include "radio_region.h"
#include "sdkconfig.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "radio_catalog";

#define RB_BASE CONFIG_SUGARAT_RADIO_BROWSER_URL
#define HTTP_JSON_CAP  200000   /* Radio Browser 列表 JSON 上限（PSRAM 暂存） */
#define CITY_LIMIT     40
#define COUNTRY_LIMIT  40


static radio_station_t *s_stations;
static int             s_count;
static char            s_country[RADIO_COUNTRY_MAX] = "";
static char            s_city[24] = "";

/* 大小写不敏感子串查找（strcasestr 不可移植，自写） */
static bool contains_ci(const char *hay, const char *needle)
{
    if (!hay || !needle || !needle[0]) {
        return false;
    }
    size_t nl = strlen(needle);
    for (const char *p = hay; *p; p++) {
        if (strncasecmp(p, needle, nl) == 0) {
            return true;
        }
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * 频率解析（三段置信度）：台名里找数字，优先带小数的 87.0~108.0，
 * 其次 3/4 位整数 /10。含 "fm" 提升一档置信度。返回范围内最佳频率；无则 0。
 * ------------------------------------------------------------------------- */
float radio_parse_freq(const char *name)
{
    if (!name) {
        return 0.0f;
    }
    float known = radio_station_known_frequency(name);
    if (known > 0.0f) return known;
    bool has_fm = contains_ci(name, "fm") || contains_ci(name, "mhz");
    float best = 0.0f;
    int best_conf = 0;

    const char *p = name;
    while (*p) {
        if (!isdigit((unsigned char)*p)) {
            p++;
            continue;
        }
        char tok[16];
        int ti = 0;
        bool dot = false;
        while (*p && ti < 15 && (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1])))) {
            if (*p == '.') {
                dot = true;
            }
            tok[ti++] = *p++;
        }
        tok[ti] = '\0';

        float v = 0.0f;
        int conf = 0;
        if (dot) {
            v = strtof(tok, NULL);
            if (v >= 87.0f && v <= 108.0f) {
                conf = has_fm ? 3 : 2;
            }
        } else if (has_fm && (v=strtof(tok,NULL))>=87.0f && v<=108.0f) {
            conf = 3;  /* FM 89 / FM 101 are valid integer frequencies. */
        } else if (ti == 3 || ti == 4) {
            v = strtof(tok, NULL) / 10.0f;   /* 1011 -> 101.1，927 -> 92.7 */
            if (v >= 87.0f && v <= 108.0f) {
                conf = has_fm ? 2 : 1;
            }
        }
        if (conf > best_conf) {
            best_conf = conf;
            best = v;
        }
    }
    return best>0.0f?best:radio_station_known_frequency(name);
}

/* ---------------------------------------------------------------------------
 * HTTP GET 整个响应到一个 PSRAM 缓冲（调用方 free）。非 2xx / 出错返回 NULL。
 * ------------------------------------------------------------------------- */
static char *http_get_buf(const char *url, int *out_len)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .user_agent = "SugaratRadio/0.1.0",
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        return NULL;
    }
    char *buf = NULL;
    if (esp_http_client_open(c, 0) != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", url);
        esp_http_client_cleanup(c);
        return NULL;
    }
    esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);
    if (status / 100 != 2) {
        ESP_LOGW(TAG, "GET %s -> %d", url, status);
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        return NULL;
    }
    buf = heap_caps_malloc(HTTP_JSON_CAP + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGW(TAG, "json buffer alloc failed (%d)", (int)(HTTP_JSON_CAP + 1));
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        return NULL;
    }
    size_t fill = 0;
    int r;
    while (fill < HTTP_JSON_CAP &&
           (r = esp_http_client_read(c, buf + fill, (int)(HTTP_JSON_CAP - fill))) > 0) {
        fill += (size_t)r;
    }
    bool complete = esp_http_client_is_complete_data_received(c);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    if (!complete || fill == 0 || fill >= HTTP_JSON_CAP) {
        heap_caps_free(buf);
        return NULL;
    }
    buf[fill] = '\0';
    if (out_len) {
        *out_len = (int)fill;
    }
    ESP_LOGI(TAG, "GET %s -> %d, %u bytes", url, status, (unsigned)fill);
    return buf;
}

/* 取字符串字段（不存在返回 def） */
static const char *jstr(const cJSON *obj, const char *key, const char *def)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(it) && it->valuestring) {
        return it->valuestring;
    }
    return def;
}

static int jint(const cJSON *obj, const char *key, int def)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(it)) {
        return it->valueint;
    }
    return def;
}

/* UTF-8 百分号编码（城市名是中文，必须编码后才能进 query） */
static void url_encode(const char *in, char *out, size_t cap)
{
    static const char *hex = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; p && *p && o + 4 < cap; p++) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0F];
        }
    }
    out[o] = '\0';
}

/* 解码器（esp_audio_simple_dec）只吃 MP3/AAC。宁可少列，也不要列出来播不了。
 * 只拦「明确不支持」的：OGG/OPUS/FLAC/HLS 与播放列表地址。
 * codec 字段大量为空或 UNKNOWN，不能反过来白名单，否则会砍掉一半可用台。 */
static bool stream_playable(const char *codec, const char *url)
{
    if (!url || (strncmp(url, "https://", 8) && strncmp(url, "http://", 7))) {
        return false;
    }
    if (contains_ci(url, ".m3u8") || contains_ci(url, ".m3u") || contains_ci(url, ".pls") ||
        contains_ci(url, ".ogg") || contains_ci(url, "opus") || contains_ci(url, ".flac")) {
        return false;
    }
    if (!codec || (strcasecmp(codec, "MP3") && strcasecmp(codec, "AAC") && strcasecmp(codec, "AAC+"))) return false;
    if (contains_ci(codec, "ogg") || contains_ci(codec, "opus") || contains_ci(codec, "flac") ||
        contains_ci(codec, "hls") || contains_ci(codec, "dash")) {
        return false;
    }
    return true;
}

static bool already_have(const char *url)
{
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_stations[i].url, url) == 0) {
            return true;
        }
    }
    return false;
}

/* 把一电台对象追加进 s_stations；返回是否成功加入 */
static bool append_station(const cJSON *item)
{
    if (s_count >= RADIO_CATALOG_MAX) {
        return false;
    }
    const char *url = jstr(item, "url_resolved", "");
    if (!url[0]) url = jstr(item, "url", NULL);
    if (strcmp(jstr(item, "countrycode", ""), "US")) return false;
    const char *name = jstr(item, "name", NULL);
    const char *codec = jstr(item, "codec", "");
    if (!url || url[0] == '\0' || !name || !name[0]) {
        return false;
    }
    if (!stream_playable(codec, url) || (jint(item,"hls",0) || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "hls"))) || already_have(url) || strlen(url)>=RADIO_URL_MAX) {
        return false;
    }
    radio_station_t *st = &s_stations[s_count];
    memset(st, 0, sizeof(*st));
    radio_station_copy_utf8(name,st->name,sizeof(st->name));
    strncpy(st->url, url, RADIO_URL_MAX - 1);
    strncpy(st->codec, codec, RADIO_CODEC_MAX - 1);
    strncpy(st->country, jstr(item, "countrycode", ""), RADIO_COUNTRY_MAX - 1);
    strncpy(st->tag, jstr(item, "tags", ""), RADIO_TAG_MAX - 1);
    st->bitrate = (uint32_t)jint(item, "bitrate", 0);
    st->votes = jint(item, "votes", 0);
    /* Parse before truncating the display buffer: FM may be at the end. */
    st->freq_mhz = radio_parse_freq(name);
    if(st->freq_mhz<=0.0f){
        const char *tags=jstr(item,"tags","");
        if(contains_ci(tags,"fm") || contains_ci(tags,"mhz"))st->freq_mhz=radio_parse_freq(tags);
    }
    s_count++;
    return true;
}

/* 解析 Radio Browser 返回的 JSON 数组，追加到列表 */
static void parse_station_array(const char *json)
{
    cJSON *root = cJSON_ParseWithOpts(json, NULL, true);
    if (!cJSON_IsArray(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return;
    }
    int n = cJSON_GetArraySize(root);
    for (int i = 0; i < n && s_count < RADIO_CATALOG_MAX; i++) {
        append_station(cJSON_GetArrayItem(root, i));
    }
    cJSON_Delete(root);
}

/* 通用搜索：query 由调用方拼好（不含前导 '?'）。返回新增数量。 */
static int fetch_search(const char *query)
{
    char url[512];
    snprintf(url, sizeof(url), "%s/json/stations/search?%s", RB_BASE, query);
    int len = 0;
    char *json = http_get_buf(url, &len);
    if (!json) {
        ESP_LOGW(TAG, "fetch failed: %s", query);
        return 0;
    }
    int before = s_count;
    parse_station_array(json);
    heap_caps_free(json);
    ESP_LOGI(TAG, "fetch %s -> +%d", query, s_count - before);
    return s_count - before;
}

extern const char bundled_stations[] __asm__("_binary_stations_us_json_start");

static void fetch_region(const radio_region_t *region) {
    char city[80], state[80], query[320];
    if (region->city[0]) {
        url_encode(region->city, city, sizeof(city));
        url_encode(region->state, state, sizeof(state));
        snprintf(query, sizeof(query),
                 "countrycode=US&state=%s&stateExact=true&name=%s&order=votes&reverse=true&hidebroken=true&limit=%d",
                 state, city, CITY_LIMIT);
        fetch_search(query);
        if (s_count < 8) {
            snprintf(query, sizeof(query),
                     "countrycode=US&state=%s&stateExact=true&order=votes&reverse=true&hidebroken=true&limit=%d",
                     state, COUNTRY_LIMIT);
            fetch_search(query);
        }
    }
    if (s_count < 8) fetch_search("countrycode=US&order=votes&reverse=true&hidebroken=true&limit=64");
}

int radio_catalog_load(const char *region_id) {
    if (!s_stations) s_stations = heap_caps_calloc(RADIO_CATALOG_MAX, sizeof(*s_stations), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_stations) { ESP_LOGE(TAG, "no PSRAM for station catalog"); return 0; }
    const radio_region_t *region = radio_region_find(region_id);
    s_count = 0;
    snprintf(s_country, sizeof(s_country), "US");
    snprintf(s_city, sizeof(s_city), "%s", region->label);
    if (CONFIG_SUGARAT_RADIO_CURATED_URL[0]) {
        if (strncmp(CONFIG_SUGARAT_RADIO_CURATED_URL, "https://", 8)) {
            ESP_LOGE(TAG, "Curated feed requires HTTPS; using Radio Browser");
        } else {
            char *json = http_get_buf(CONFIG_SUGARAT_RADIO_CURATED_URL, NULL);
            if (json) { parse_station_array(json); heap_caps_free(json); }
        }
    }
    if (!s_count) fetch_region(region);
    if (!s_count) {
        parse_station_array(bundled_stations);
        snprintf(s_city, sizeof(s_city), "US presets");
        ESP_LOGW(TAG, "Directory unavailable; using %d bundled station URLs (Internet still required)", s_count);
    }
    return s_count;
}

int radio_catalog_count(void)
{
    return s_count;
}

const radio_station_t *radio_catalog_at(int idx)
{
    if (idx < 0 || idx >= s_count) {
        return NULL;
    }
    return &s_stations[idx];
}

const radio_station_t *radio_catalog_next(int idx)
{
    if (s_count == 0) {
        return NULL;
    }
    int n = idx + 1;
    if (n >= s_count) {
        n = 0;
    }
    if (n < 0) {
        n = s_count - 1;
    }
    return &s_stations[n];
}

const char *radio_catalog_country(void)
{
    return s_country;
}

const char *radio_catalog_city(void)
{
    return s_city;
}
