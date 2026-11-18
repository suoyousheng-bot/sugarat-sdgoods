/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Local entry redirects issue the current session token; profile APIs require it. */
#ifdef CARD_HOST_HTTP
#include "card_host_http.h"
#else
#include "card_network.h"
#endif
#include <stdio.h>
#include <string.h>

static esp_err_t reply(httpd_req_t *r,const char *status,const char *body)
{
    httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    return httpd_resp_sendstr(r,body);
}
static bool authorized(httpd_req_t *r,card_network_t *n)
{
    char auth[48],host[40];
    if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK||
       (strcmp(host,"192.168.4.1")&&strcmp(host,"192.168.4.1:80")))return false;
    if(httpd_req_get_hdr_value_str(r,"Authorization",auth,sizeof(auth))!=ESP_OK||strlen(auth)!=39||memcmp(auth,"Bearer ",7))return false;
    return card_secure_equal((uint8_t *)auth+7,(uint8_t *)n->token,32);
}
static const char editor_html[]={
#include "editor.inc"
};
static esp_err_t editor(httpd_req_t *r)
{
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'");
    httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");return httpd_resp_send(r,editor_html,sizeof(editor_html)-1);
}
static esp_err_t get_profile(httpd_req_t *r)
{
    card_network_t *n=r->user_ctx;if(!authorized(r,n))return reply(r,"401 Unauthorized","{\"error\":\"unauthorized\"}");
    char bytes[CARD_JSON_MAX];xSemaphoreTake(n->mutex,portMAX_DELAY);
    size_t size=card_profile_json(&n->published,bytes,sizeof(bytes));xSemaphoreGive(n->mutex);
    if(!size)return reply(r,"500 Internal Server Error","{\"error\":\"read_failed\"}");
    return reply(r,"200 OK",bytes);
}
static esp_err_t save_profile(httpd_req_t *r)
{
    card_network_t *n=r->user_ctx;if(!authorized(r,n))return reply(r,"401 Unauthorized","{\"error\":\"unauthorized\"}");
    if(r->content_len<=0||r->content_len>CARD_JSON_MAX)return reply(r,"413 Payload Too Large","{\"error\":\"too_large\"}");
    char bytes[CARD_JSON_MAX+1];size_t used=0;
    while(used<(size_t)r->content_len) {
        int got=httpd_req_recv(r,bytes+used,r->content_len-used);
        if(got<=0)return reply(r,"408 Request Timeout","{\"error\":\"timeout\"}");
        used+=(unsigned)got;
    }
    card_profile_t profile;
    if(card_profile_parse(bytes,used,&profile)!=CARD_OK)return reply(r,"400 Bad Request","{\"error\":\"invalid\"}");
    xSemaphoreTake(n->mutex,portMAX_DELAY);
    if(n->queued||(!n->finished&&n->request)){xSemaphoreGive(n->mutex);return reply(r,"409 Conflict","{\"error\":\"busy\"}");}
    n->pending=profile;n->queued=true;n->finished=n->failed=false;++n->request;unsigned id=n->request;xSemaphoreGive(n->mutex);
    char response[64];snprintf(response,sizeof(response),"{\"id\":%u,\"state\":\"queued\"}",id);return reply(r,"202 Accepted",response);
}
static esp_err_t status(httpd_req_t *r)
{
    card_network_t *n=r->user_ctx;if(!authorized(r,n))return reply(r,"401 Unauthorized","{\"error\":\"unauthorized\"}");
    xSemaphoreTake(n->mutex,portMAX_DELAY);unsigned id=n->request;bool finished=n->finished,failed=n->failed;xSemaphoreGive(n->mutex);
    char response[192];snprintf(response,sizeof(response),"{\"id\":%u,\"state\":\"%s\",\"error\":\"%s\"}",id,
        !id?"idle":!finished?"queued":failed?"error":"saved",failed?"save_unconfirmed":"");return reply(r,"200 OK",response);
}
static esp_err_t get(httpd_req_t *r)
{
    if(!strcmp(r->uri,"/edit")||!strncmp(r->uri,"/edit?",6))return editor(r);
    if(!strcmp(r->uri,"/api/profile"))return get_profile(r);
    if(!strcmp(r->uri,"/api/status"))return status(r);
    if(!strncmp(r->uri,"/api/",5))return reply(r,"404 Not Found","{\"error\":\"not_found\"}");
    card_network_t *n=r->user_ctx;
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"Referrer-Policy","no-referrer");
    if(!strcmp(r->uri,"/captive-portal")) {
        char body[224];snprintf(body,sizeof(body),"{\"captive\":true,\"user-portal-url\":\"%s\"}",n->url);
        httpd_resp_set_type(r,"application/captive+json");return httpd_resp_sendstr(r,body);
    }
    httpd_resp_set_status(r,"302 Found");httpd_resp_set_hdr(r,"Location",n->url);
    return httpd_resp_sendstr(r,"Sugarat Social Card");
}
bool card_http_start(card_network_t *n)
{
    httpd_config_t server=HTTPD_DEFAULT_CONFIG();server.stack_size=12288;server.recv_wait_timeout=3;server.send_wait_timeout=3;
    server.max_uri_handlers=2;server.lru_purge_enable=true;server.uri_match_fn=httpd_uri_match_wildcard;
    if(httpd_start(&n->server,&server)!=ESP_OK)return false;
    httpd_uri_t routes[]={
        {.uri="/*",.method=HTTP_GET,.handler=get,.user_ctx=n},
        {.uri="/api/profile",.method=HTTP_POST,.handler=save_profile,.user_ctx=n},
    };
    for(unsigned i=0;i<2;++i)if(httpd_register_uri_handler(n->server,&routes[i])!=ESP_OK)return false;
    return true;
}
