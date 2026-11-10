/*
 * Deotaland Agent Link platform client. Request and response fields follow the Deotaland device gateway as
 * used by the Agent_link SDK (github.com/Deotaland/Agent_link, MIT); this is an independent
 * implementation.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sugarat_agent_cloud.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "agent_protocol.h"
#include "nvs.h"
#include "sugarat_https.h"

static const char *TAG = "agent_cloud";

#define NVS_NAMESPACE      "agent_link"
#define NET_UP_BIT         BIT0
#define BIND_POLL_MS       3000
#define CODE_RENEW_LEAD_S  30
#define ONLINE_PERIOD_MS   60000
#define NO_AGENT_RETRY_MS  10000
#define FAULT_MIN_MS       3000
#define FAULT_MAX_MS       60000

/* Business codes from body.code. */
#define CODE_ALREADY_BOUND 1002
#define CODE_KEY_REVOKED   1002103
#define CODE_NO_AGENT      1002104

typedef struct {
    int http;
    int code;
    char message[96];
} api_result_t;

static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_events;
static TaskHandle_t s_task;
static sugarat_agent_cloud_status_t s_status;
static char s_sn[40];
static char s_mac[18];
static char s_key[72];          /* device auth key; guarded by s_lock */
static volatile bool s_forget;

/* ---- status ------------------------------------------------------------------------------- */

static void publish(sugarat_agent_cloud_phase_t phase, int error_code)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_status.phase != phase || s_status.error_code != error_code) {
        s_status.phase = phase;
        s_status.error_code = error_code;
        s_status.revision++;
    }
    xSemaphoreGive(s_lock);
}

void sugarat_agent_cloud_status(sugarat_agent_cloud_status_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_status;
    xSemaphoreGive(s_lock);
}

const char *sugarat_agent_cloud_device_sn(void)
{
    return s_sn;
}

void sugarat_agent_cloud_set_network(bool up)
{
    if (!s_events) {
        return;
    }
    if (up) {
        xEventGroupSetBits(s_events, NET_UP_BIT);
        if (s_task) {
            xTaskNotifyGive(s_task);
        }
    } else {
        xEventGroupClearBits(s_events, NET_UP_BIT);
    }
}

void sugarat_agent_cloud_forget(void)
{
    s_forget = true;
    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

static bool net_up(void)
{
    return (xEventGroupGetBits(s_events) & NET_UP_BIT) != 0;
}

/* Sleep up to ms; returns early when notified (network change or forget). */
static void nap(uint32_t ms)
{
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));
}

/* ---- NVS ------------------------------------------------------------------------------------ */

static bool nvs_get(const char *key, char *out, size_t cap)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t len = cap;
    const esp_err_t err = nvs_get_str(h, key, out, &len);
    nvs_close(h);
    return err == ESP_OK && out[0];
}

static esp_err_t nvs_put(const char *key, const char *value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = value ? nvs_set_str(h, key, value) : nvs_erase_key(h, key);
    if (err == ESP_ERR_NVS_NOT_FOUND && !value) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static void ensure_identity(void)
{
    if (!s_mac[0]) {
        uint8_t m[6] = { 0 };
        esp_read_mac(m, ESP_MAC_WIFI_STA);
        snprintf(s_mac, sizeof(s_mac), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    }
    if (s_sn[0] || nvs_get("sn", s_sn, sizeof(s_sn))) {
        return;
    }
    /* UUIDv4; generated after Wi-Fi is running, when the hardware RNG is fully random. */
    uint8_t b[16];
    esp_fill_random(b, sizeof(b));
    b[6] = (b[6] & 0x0f) | 0x40;
    b[8] = (b[8] & 0x3f) | 0x80;
    char sn[40];
    snprintf(sn, sizeof(sn), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    if (nvs_put("sn", sn) != ESP_OK) {
        ESP_LOGE(TAG, "device serial could not be saved; activation would not survive a restart");
    }
    memcpy(s_sn, sn, sizeof(s_sn));
}

static bool have_key(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool have = s_key[0] != 0;
    xSemaphoreGive(s_lock);
    return have;
}

static void copy_key(char *out, size_t cap)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(out, cap, "%s", s_key);
    xSemaphoreGive(s_lock);
}

static void set_key(const char *key)
{
    if (nvs_put("auth_key", key) != ESP_OK) {
        ESP_LOGE(TAG, "device key could not be saved; activation would not survive a restart");
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(s_key, sizeof(s_key), "%s", key ? key : "");
    if (!key) {
        s_status.agent_id = 0;
        s_status.agent_name[0] = 0;
        s_status.voice_supported = false;
        s_status.agent_epoch++;
        s_status.revision++;
    }
    xSemaphoreGive(s_lock);
}

/* ---- HTTPS ----------------------------------------------------------------------------------- */

static void redact(char *text, const char *secret)
{
    const size_t n = secret ? strlen(secret) : 0;
    char *p;
    while (n >= 8 && (p = strstr(text, secret)) != NULL) {
        memset(p, '*', n);
    }
}

static const char *method_name(esp_http_client_method_t method)
{
    return method == HTTP_METHOD_GET ? "GET" : method == HTTP_METHOD_DELETE ? "DELETE" : "POST";
}

/*
 * One request. Success needs a 2xx status, success != false and code == 0 (when present).
 * On success *root holds the parsed body (caller deletes); DELETE may return an empty body.
 * Logs carry the path, status and business code; never keys, tokens or bodies.
 */
static esp_err_t api(esp_http_client_method_t method, const char *path, const char *body, bool device_key,
                     cJSON **root, api_result_t *res)
{
    *root = NULL;
    memset(res, 0, sizeof(*res));
    char url[256];
    snprintf(url, sizeof(url), "%s%s", CONFIG_SUGARAT_AGENT_LINK_BASE_URL, path);
    char key[72] = { 0 };
    const sugarat_https_header_t headers[2] = { { "Accept", "application/json" }, { "X-Device-Auth-Key", key } };
    if (device_key) {
        copy_key(key, sizeof(key));
        if (!key[0]) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    sugarat_https_request_t req = sugarat_https_request_default(url);
    req.method = method;
    req.headers = headers;
    req.header_count = device_key ? 2 : 1;
    if (body) {
        req.body = body;
        req.body_length = strlen(body);
        req.content_type = "application/json";
    }
    req.max_response_bytes = 32 * 1024;
    sugarat_https_response_t resp;
    const esp_err_t err = sugarat_https_perform(&req, &resp);
    res->http = resp.status;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s %s: %s", method_name(method), path, esp_err_to_name(err));
        memset(key, 0, sizeof(key));
        sugarat_https_response_free(&resp);
        return err;
    }
    cJSON *json = resp.body ? cJSON_ParseWithLength((const char *)resp.body, resp.length) : NULL;
    sugarat_https_response_free(&resp);
    const bool http_ok = res->http >= 200 && res->http < 300;
    if (!json) {
        memset(key, 0, sizeof(key));
        if (http_ok && method == HTTP_METHOD_DELETE) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "%s %s: HTTP %d, body is not JSON", method_name(method), path, res->http);
        return ESP_ERR_INVALID_RESPONSE;
    }
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(json, "success");
    const cJSON *msg = cJSON_GetObjectItemCaseSensitive(json, "message");
    res->code = cJSON_IsNumber(code) ? code->valueint : 0;
    if (cJSON_IsString(msg)) {
        snprintf(res->message, sizeof(res->message), "%s", msg->valuestring);
        redact(res->message, key);
    }
    memset(key, 0, sizeof(key));
    if (!http_ok || res->code != 0 || (cJSON_IsBool(ok) && !cJSON_IsTrue(ok))) {
        ESP_LOGW(TAG, "%s %s: HTTP %d, code=%d, message=%s", method_name(method), path, res->http, res->code,
                 res->message);
        cJSON_Delete(json);
        return ESP_ERR_INVALID_RESPONSE;
    }
    *root = json;
    return ESP_OK;
}

static const cJSON *field(const cJSON *obj, const char *key)
{
    return cJSON_IsObject(obj) ? cJSON_GetObjectItemCaseSensitive(obj, key) : NULL;
}

static const char *string(const cJSON *obj, const char *key)
{
    const cJSON *v = field(obj, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static const cJSON *data_of(const cJSON *root)
{
    const cJSON *d = field(root, "data");
    return d ? d : root;
}

static bool safe_id(const char *id)
{
    if (!id || !*id || strlen(id) > 79) {
        return false;
    }
    for (const char *p = id; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            return false;
        }
    }
    return true;
}

static const char *fw_version(void)
{
    return esp_app_get_description()->version;
}

/* ---- activation and online loop --------------------------------------------------------------- */

static uint32_t backoff_ms(unsigned failures)
{
    const uint32_t ms = (uint32_t)FAULT_MIN_MS << (failures > 5 ? 5 : failures);
    return ms > FAULT_MAX_MS ? FAULT_MAX_MS : ms;
}

/* Request a code and poll until it is claimed. Returns true once a device key is stored. */
static bool claim(unsigned *failures)
{
    publish(SUGARAT_AGENT_CLOUD_CLAIMING, 0);
    char body[320];
    snprintf(body, sizeof(body),
             "{\"product_id\":%d,\"device_sn\":\"%s\",\"mac_address\":\"%s\",\"chip_type\":\"%s\",\"firmware_version\":\"%s\"}",
             CONFIG_SUGARAT_AGENT_LINK_PRODUCT_ID, s_sn, s_mac, CONFIG_SUGARAT_AGENT_LINK_CHIP_TYPE, fw_version());
    cJSON *root = NULL;
    api_result_t res;
    if (api(HTTP_METHOD_POST, "/api/device-gateway/claim-code", body, false, &root, &res) != ESP_OK) {
        /* Code 1002 is a general refusal; the message says which one. */
        if (res.code == CODE_ALREADY_BOUND && strstr(res.message, "chipTypeMismatch")) {
            publish(SUGARAT_AGENT_CLOUD_REJECTED, res.code);
            nap(60000);
        } else if (res.code == CODE_ALREADY_BOUND &&
                   (strstr(res.message, "bound") || strstr(res.message, "Bound") || strstr(res.message, "绑定"))) {
            publish(SUGARAT_AGENT_CLOUD_ALREADY_BOUND, res.code);
            nap(30000);
        } else {
            publish(SUGARAT_AGENT_CLOUD_FAULT, res.code ? res.code : res.http);
            nap(backoff_ms((*failures)++));
        }
        return false;
    }
    *failures = 0;
    const cJSON *data = data_of(root);
    const char *code = string(data, "code");
    const cJSON *exp = field(data, "expires_in");
    const uint32_t left = cJSON_IsNumber(exp) && exp->valuedouble > 0 ? (uint32_t)exp->valuedouble : 600;
    if (!code || !code[0] || strlen(code) >= sizeof(s_status.code)) {
        ESP_LOGE(TAG, "claim-code: no code in a successful response");
        cJSON_Delete(root);
        publish(SUGARAT_AGENT_CLOUD_FAULT, -1);
        nap(backoff_ms((*failures)++));
        return false;
    }
    char code_copy[8];
    snprintf(code_copy, sizeof(code_copy), "%s", code);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "activation code issued, valid %lu s", (unsigned long)left);

    int64_t expires = esp_timer_get_time() + (int64_t)left * 1000000;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(s_status.code, sizeof(s_status.code), "%s", code_copy);
    s_status.code_expires_us = expires;
    s_status.phase = SUGARAT_AGENT_CLOUD_CODE;
    s_status.error_code = 0;
    s_status.revision++;
    xSemaphoreGive(s_lock);

    snprintf(body, sizeof(body), "{\"device_sn\":\"%s\",\"code\":\"%s\"}", s_sn, code_copy);
    while (net_up() && !s_forget && esp_timer_get_time() + CODE_RENEW_LEAD_S * 1000000LL < expires) {
        nap(BIND_POLL_MS);
        if (!net_up() || s_forget) {
            break;
        }
        if (api(HTTP_METHOD_POST, "/api/device-gateway/bind-status", body, false, &root, &res) != ESP_OK) {
            continue; /* keep showing the code; a transient failure is retried on the next poll */
        }
        data = data_of(root);
        const char *key = string(data, "auth_key");
        const char *status = string(data, "status");
        if (key && key[0] && strlen(key) < sizeof(s_key)) {
            set_key(key);
            cJSON_Delete(root);
            ESP_LOGI(TAG, "device claimed");
            return true;
        }
        const bool expired = status && strcmp(status, "expired") == 0;
        const cJSON *ttl = field(data, "remaining_ttl");
        if (cJSON_IsNumber(ttl) && ttl->valuedouble > 0) {
            expires = esp_timer_get_time() + (int64_t)ttl->valuedouble * 1000000;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_status.code_expires_us = expires;
            xSemaphoreGive(s_lock);
        }
        cJSON_Delete(root);
        if (expired) {
            break;
        }
    }
    return false; /* the code expired or the network was lost: request a new one */
}

typedef enum { AUTH_OK, AUTH_REVOKED, AUTH_NO_AGENT, AUTH_FAILED } auth_result_t;

static auth_result_t authenticate(void)
{
    char key[72];
    copy_key(key, sizeof(key));
    char body[256];
    snprintf(body, sizeof(body), "{\"auth_key\":\"%s\",\"mac_address\":\"%s\",\"firmware_version\":\"%s\"}", key, s_mac,
             fw_version());
    memset(key, 0, sizeof(key));
    cJSON *root = NULL;
    api_result_t res;
    const esp_err_t err = api(HTTP_METHOD_POST, "/api/device-gateway/auth", body, false, &root, &res);
    memset(body, 0, sizeof(body));
    if (err != ESP_OK) {
        if (res.code == CODE_KEY_REVOKED) {
            return AUTH_REVOKED;
        }
        if (res.code == CODE_NO_AGENT) {
            return AUTH_NO_AGENT;
        }
        publish(SUGARAT_AGENT_CLOUD_FAULT, res.code ? res.code : res.http);
        return AUTH_FAILED;
    }
    const cJSON *agent = field(data_of(root), "agent");
    const cJSON *id = field(agent, "id");
    const int agent_id = cJSON_IsNumber(id) && id->valueint > 0 ? id->valueint : 0;
    const char *name = string(agent, "name");
    const bool voice = cJSON_IsObject(field(field(agent, "capabilities"), "realtime_voice"));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_status.agent_id != agent_id) {
        s_status.agent_epoch++;
    }
    s_status.agent_id = agent_id;
    snprintf(s_status.agent_name, sizeof(s_status.agent_name), "%s", name ? name : "");
    s_status.voice_supported = voice;
    s_status.revision++;
    xSemaphoreGive(s_lock);
    cJSON_Delete(root);
    return agent_id ? AUTH_OK : AUTH_NO_AGENT;
}

static void heartbeat(void)
{
    char key[72];
    copy_key(key, sizeof(key));
    char body[128];
    snprintf(body, sizeof(body), "{\"auth_key\":\"%s\"}", key);
    memset(key, 0, sizeof(key));
    cJSON *root = NULL;
    api_result_t res;
    if (api(HTTP_METHOD_POST, "/api/device-gateway/heartbeat", body, false, &root, &res) == ESP_OK) {
        cJSON_Delete(root);
    }
    memset(body, 0, sizeof(body));
}

static void cloud_task(void *arg)
{
    (void)arg;
    unsigned failures = 0;
    for (;;) {
        if (!net_up()) {
            publish(SUGARAT_AGENT_CLOUD_OFFLINE, 0);
            xEventGroupWaitBits(s_events, NET_UP_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        }
        ensure_identity();
        if (s_forget) {
            s_forget = false;
            set_key(NULL);
            ESP_LOGI(TAG, "device key forgotten; requesting a new activation code");
        }
        if (!have_key()) {
            (void)claim(&failures);
            continue;
        }
        sugarat_agent_cloud_status_t now;
        sugarat_agent_cloud_status(&now);
        if (now.phase != SUGARAT_AGENT_CLOUD_ONLINE) {
            publish(SUGARAT_AGENT_CLOUD_SIGNING_IN, 0);
        }
        switch (authenticate()) {
        case AUTH_REVOKED:
            ESP_LOGW(TAG, "the platform revoked the device key; requesting a new activation code");
            set_key(NULL);
            continue;
        case AUTH_NO_AGENT:
            publish(SUGARAT_AGENT_CLOUD_NO_AGENT, CODE_NO_AGENT);
            nap(NO_AGENT_RETRY_MS);
            continue;
        case AUTH_FAILED:
            nap(backoff_ms(failures++));
            continue;
        case AUTH_OK:
            break;
        }
        failures = 0;
        if (now.phase != SUGARAT_AGENT_CLOUD_ONLINE) {
            ESP_LOGI(TAG, "online");
        }
        publish(SUGARAT_AGENT_CLOUD_ONLINE, 0);
        /* Online: heartbeat, then authenticate again on the next pass to follow console changes. */
        const int64_t until = esp_timer_get_time() + ONLINE_PERIOD_MS * 1000LL;
        while (net_up() && !s_forget && esp_timer_get_time() < until) {
            nap((uint32_t)((until - esp_timer_get_time()) / 1000) + 1);
        }
        if (net_up() && !s_forget) {
            heartbeat();
        }
    }
}

esp_err_t sugarat_agent_cloud_start(void)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }
    s_lock = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    if (!s_lock || !s_events) {
        return ESP_ERR_NO_MEM;
    }
    (void)nvs_get("sn", s_sn, sizeof(s_sn));
    (void)nvs_get("auth_key", s_key, sizeof(s_key));
    ESP_LOGI(TAG, "device %s, %s", s_sn[0] ? "identified" : "new", s_key[0] ? "activated" : "not activated");
    /* NVS writes need an internal stack: the cache is disabled while flash is written. */
    if (xTaskCreate(cloud_task, "agent_cloud", 6144, NULL, 4, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/* ---- conversations and voice grants ------------------------------------------------------------ */

static bool bound_to(int agent_id)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool ok = agent_id > 0 && s_status.phase == SUGARAT_AGENT_CLOUD_ONLINE && s_status.agent_id == agent_id;
    xSemaphoreGive(s_lock);
    return ok;
}

static const cJSON *rows_of(const cJSON *data)
{
    if (cJSON_IsArray(data)) {
        return data;
    }
    static const char *const keys[] = { "items", "list", "records", "conversations" };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const cJSON *rows = field(data, keys[i]);
        if (cJSON_IsArray(rows)) {
            return rows;
        }
    }
    return NULL;
}

static const char *conversation_id_of(const cJSON *obj)
{
    const char *id = string(obj, "conversation_id");
    if (!id) {
        id = string(obj, "session_id");
    }
    if (!id) {
        id = string(field(obj, "conversation"), "conversation_id");
    }
    return id;
}

esp_err_t sugarat_agent_cloud_new_conversation(int agent_id, char *id, size_t id_size, bool *reused)
{
    *reused = false;
    id[0] = 0;
    if (!bound_to(agent_id)) {
        return ESP_ERR_INVALID_STATE;
    }
    char path[160];
    snprintf(path, sizeof(path), "/api/agents/%d/actions/realtime_voice/conversations", agent_id);
    cJSON *root = NULL;
    api_result_t res;
    esp_err_t err = api(HTTP_METHOD_POST, path, NULL, true, &root, &res);
    if (err == ESP_OK) {
        const char *created = conversation_id_of(data_of(root));
        if (safe_id(created) && strlen(created) < id_size) {
            snprintf(id, id_size, "%s", created);
        }
        cJSON_Delete(root);
        if (id[0]) {
            ESP_LOGI(TAG, "new conversation created");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "create conversation: no id in a successful response");
    } else if (res.http == 0) {
        return err; /* transport failure: the caller retries */
    }
    /* The platform refused to create one (for example code 1002, agent_template_unavailable):
       continue in the newest existing conversation. */
    snprintf(path, sizeof(path), "/api/agents/%d/actions/realtime_voice/conversations?limit=1", agent_id);
    err = api(HTTP_METHOD_GET, path, NULL, true, &root, &res);
    if (err != ESP_OK) {
        return res.http == 0 ? err : ESP_ERR_NOT_FOUND;
    }
    const cJSON *rows = rows_of(data_of(root));
    const char *latest = rows ? conversation_id_of(cJSON_GetArrayItem(rows, 0)) : NULL;
    if (safe_id(latest) && strlen(latest) < id_size) {
        snprintf(id, id_size, "%s", latest);
    }
    cJSON_Delete(root);
    if (!id[0]) {
        return ESP_ERR_NOT_FOUND;
    }
    *reused = true;
    ESP_LOGW(TAG, "could not create a conversation; continuing the newest existing one");
    return ESP_OK;
}

/* Audio settings may sit under audio_config/audio at the top level or below data/grant. */
static const cJSON *audio_settings(const cJSON *obj, int depth)
{
    if (!cJSON_IsObject(obj) || depth > 3) {
        return NULL;
    }
    static const char *const direct[] = { "audio_config", "audio" };
    for (size_t i = 0; i < 2; i++) {
        const cJSON *a = field(obj, direct[i]);
        if (cJSON_IsObject(a)) {
            return a;
        }
    }
    static const char *const nested[] = { "realtimeConnection", "grant", "data" };
    for (size_t i = 0; i < 3; i++) {
        const cJSON *a = audio_settings(field(obj, nested[i]), depth + 1);
        if (a) {
            return a;
        }
    }
    return NULL;
}

/* Parse one direction; false for formats this firmware cannot handle. */
static bool audio_direction(const cJSON *dir, int rate, bool *opus)
{
    const char *encoding = string(dir, "encoding");
    const cJSON *sr = field(dir, "sample_rate");
    if (!sr) {
        sr = field(dir, "sampleRate");
    }
    const cJSON *ch = field(dir, "channels");
    if (!encoding || !cJSON_IsNumber(sr) || sr->valuedouble != rate || (ch && (!cJSON_IsNumber(ch) || ch->valuedouble != 1))) {
        return false;
    }
    if (strcmp(encoding, "pcm16") == 0) {
        *opus = false;
        return true;
    }
    if (strcmp(encoding, "opus") != 0) {
        return false;
    }
    const cJSON *dur = field(dir, "packet_duration_ms");
    if (!dur) {
        dur = field(dir, "packetDurationMs");
    }
    if (dur && (!cJSON_IsNumber(dur) || dur->valuedouble != 20)) {
        return false;
    }
    *opus = true;
    return true;
}

bool sugarat_agent_cloud_parse_audio(const void *json, bool *input_opus, bool *output_opus)
{
    const cJSON *a = audio_settings((const cJSON *)json, 0);
    if (!a) {
        return true; /* no settings: keep the current formats */
    }
    const cJSON *in = field(a, "input");
    const cJSON *out = field(a, "output");
    bool i = *input_opus, o = *output_opus;
    if (in && !audio_direction(in, 16000, &i)) {
        return false;
    }
    if (out && !audio_direction(out, 24000, &o)) {
        return false;
    }
    *input_opus = i;
    *output_opus = o;
    return true;
}

static const char *grant_string(const cJSON *d, const char *a, const char *b, const char *c)
{
    const char *v = string(d, a);
    if (!v) {
        v = string(d, b);
    }
    if (!v) {
        v = string(d, c);
    }
    return v;
}

esp_err_t sugarat_agent_cloud_grant(int agent_id, const char *conversation_id, sugarat_agent_cloud_grant_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!bound_to(agent_id) || !safe_id(conversation_id)) {
        return ESP_ERR_INVALID_STATE;
    }
    char path[192];
    char body[512];
    snprintf(body, sizeof(body), "{\"session_id\":\"%s\",\"audio_config\":%s}", conversation_id, AGENT_VOICE_AUDIO_CONFIG);
    cJSON *root = NULL;
    api_result_t res;
    snprintf(path, sizeof(path), "/api/agents/%d/actions/realtime_voice/realtime-grant", agent_id);
    esp_err_t err = api(HTTP_METHOD_POST, path, body, true, &root, &res);
    if (err != ESP_OK && res.http == 409) {
        snprintf(path, sizeof(path), "/api/agents/%d/actions/realtime_voice/realtime-refresh", agent_id);
        err = api(HTTP_METHOD_POST, path, body, true, &root, &res);
    }
    if (err != ESP_OK) {
        return err;
    }
    const cJSON *d = data_of(root);
    const cJSON *g = field(d, "grant");
    if (cJSON_IsObject(g)) {
        d = g;
    }
    const char *ws = grant_string(d, "websocket_url", "websocketUrl", "ws_url");
    const char *token = grant_string(d, "connect_token", "connectToken", "token");
    out->input_opus = true;
    out->output_opus = true;
    if (!ws || !token || strncmp(ws, "wss://", 6) != 0 || strlen(ws) >= sizeof(out->url) ||
        strlen(token) >= sizeof(out->token)) {
        err = ESP_ERR_INVALID_RESPONSE;
        ESP_LOGW(TAG, "voice grant: missing or oversized WebSocket address or token");
    } else if (!sugarat_agent_cloud_parse_audio(root, &out->input_opus, &out->output_opus)) {
        err = ESP_ERR_NOT_SUPPORTED;
        ESP_LOGW(TAG, "voice grant: unsupported audio format");
    } else {
        snprintf(out->url, sizeof(out->url), "%s", ws);
        snprintf(out->token, sizeof(out->token), "%s", token);
    }
    cJSON_Delete(root);
    return err;
}

void sugarat_agent_cloud_end_grant(int agent_id, const char *conversation_id)
{
    if (agent_id <= 0 || !safe_id(conversation_id)) {
        return;
    }
    char path[192];
    snprintf(path, sizeof(path), "/api/agents/%d/actions/realtime_voice/realtime-grant/%s", agent_id, conversation_id);
    cJSON *root = NULL;
    api_result_t res;
    if (api(HTTP_METHOD_DELETE, path, NULL, true, &root, &res) == ESP_OK) {
        cJSON_Delete(root);
    }
}
