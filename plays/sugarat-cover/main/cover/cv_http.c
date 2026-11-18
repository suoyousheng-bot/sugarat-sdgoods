/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Local hotspot portal issues a session token; every photo and mutation requires that token. */
#ifdef CV_HOST_HTTP
#include "cv_host_http.h"
#else
#include "cv_device.h"
#include "sdgoods_input.h"
#include "esp_timer.h"
#include "cJSON.h"
#endif
#include "cv_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CV_HOST_HTTP
extern const uint8_t html_start[] asm("_binary_index_html_start"),html_end[] asm("_binary_index_html_end");
extern const uint8_t js_start[] asm("_binary_app_js_start"),js_end[] asm("_binary_app_js_end");
extern const uint8_t css_start[] asm("_binary_style_css_start"),css_end[] asm("_binary_style_css_end");
extern const uint8_t font_start[] asm("_binary_font_woff2_start"),font_end[] asm("_binary_font_woff2_end");
#endif
bool cv_http_authorized(cv_device_t *d,const char *token)
{
    if(!token||strlen(token)!=32)return false;
    xSemaphoreTake(d->mutex,portMAX_DELAY);
    bool ok=!d->faulted&&*d->status.token&&cv_secure_equal((const uint8_t *)token,(const uint8_t *)d->status.token,32);
    xSemaphoreGive(d->mutex);return ok;
}
static esp_err_t result(httpd_req_t *req,cv_error_t error,const char *id)
{
    char body[160];snprintf(body,sizeof(body),"{\"error\":%u,\"id\":\"%s\"}",(unsigned)error,id?id:"");
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    if(error!=CV_OK)httpd_resp_set_status(req,error==CV_AUTH?"401 Unauthorized":error==CV_BUSY?"409 Conflict":"400 Bad Request");
    return httpd_resp_sendstr(req,body);
}
static bool authorized(httpd_req_t *req)
{
    char token[34];size_t size=httpd_req_get_hdr_value_len(req,"X-Cover-Token");
    return size==32&&httpd_req_get_hdr_value_str(req,"X-Cover-Token",token,sizeof(token))==ESP_OK&&cv_http_authorized(req->user_ctx,token);
}
static esp_err_t state(httpd_req_t *req)
{
    cv_device_t *d=req->user_ctx;cJSON *json=cJSON_CreateObject();if(!json)return result(req,CV_MEMORY,NULL);
    xSemaphoreTake(d->library_mutex,portMAX_DELAY);cv_library_t library=d->store.library;
    xSemaphoreGive(d->library_mutex);char ssid[32];xSemaphoreTake(d->mutex,portMAX_DELAY);strcpy(ssid,d->status.ssid);xSemaphoreGive(d->mutex);
    cJSON_AddStringToObject(json,"locale",library.locale==CV_ZH?"zh":"en");cJSON_AddStringToObject(json,"badge",ssid);
    cJSON_AddNumberToObject(json,"revision",library.revision);cJSON_AddNumberToObject(json,"error",library.error);
    cJSON_AddBoolToObject(json,"writable",library.writable);cJSON_AddNumberToObject(json,"limit",CV_MAX_PHOTOS);
    cJSON_AddNumberToObject(json,"maxBytes",CV_MAX_JPEG);
    cJSON_AddStringToObject(json,"current",library.count?library.photos[library.selected].id:"");
    cJSON *photos=cJSON_AddArrayToObject(json,"photos");
    for(unsigned i=0;i<library.count;i++) {
        cJSON *photo=cJSON_CreateObject();if(!photo)continue;
        cJSON_AddStringToObject(photo,"id",library.photos[i].id);cJSON_AddNumberToObject(photo,"size",library.photos[i].size);cJSON_AddItemToArray(photos,photo);
    }
    char *encoded=cJSON_PrintUnformatted(json);cJSON_Delete(json);if(!encoded)return result(req,CV_MEMORY,NULL);
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t sent=httpd_resp_sendstr(req,encoded);free(encoded);return sent;
}
static esp_err_t get(httpd_req_t *req)
{
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'self'; img-src 'self' blob:; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");
    const uint8_t *start=NULL,*end=NULL;const char *type=NULL;
    if(!strcmp(req->uri,"/gallery")){start=html_start;end=html_end;type="text/html; charset=utf-8";}
    else if(!strcmp(req->uri,"/app.js")){start=js_start;end=js_end;type="text/javascript; charset=utf-8";}
    else if(!strcmp(req->uri,"/style.css")){start=css_start;end=css_end;type="text/css; charset=utf-8";}
    else if(!strcmp(req->uri,"/font.woff2")){start=font_start;end=font_end;type="font/woff2";}
    if(start){httpd_resp_set_type(req,type);return httpd_resp_send(req,(const char *)start,end-start);}
    if(strncmp(req->uri,"/api/",5)) {
        cv_device_t *d=req->user_ctx;char token[33];
        xSemaphoreTake(d->mutex,portMAX_DELAY);memcpy(token,d->status.token,sizeof(token));bool active=!d->faulted&&*token;xSemaphoreGive(d->mutex);
        if(!active)return result(req,CV_BUSY,NULL);
        char destination[128];snprintf(destination,sizeof(destination),CV_PORTAL_ORIGIN "/gallery#%s",token);
        httpd_resp_set_hdr(req,"Cache-Control","no-store");httpd_resp_set_hdr(req,"Referrer-Policy","no-referrer");
        if(!strcmp(req->uri,"/captive-portal")) {
            char body[224];snprintf(body,sizeof(body),"{\"captive\":true,\"user-portal-url\":\"%s\"}",destination);
            httpd_resp_set_type(req,"application/captive+json");return httpd_resp_sendstr(req,body);
        }
        httpd_resp_set_status(req,"302 Found");httpd_resp_set_hdr(req,"Location",destination);
        return httpd_resp_sendstr(req,"Sugarat Cover");
    }
    if(!authorized(req))return result(req,CV_AUTH,NULL);
    if(!strcmp(req->uri,"/api/state"))return state(req);
    if(!strncmp(req->uri,"/api/image/",11)) {
        cv_device_t *d=req->user_ctx;uint8_t *bytes=NULL;size_t size=0;
        xSemaphoreTake(d->library_mutex,portMAX_DELAY);cv_error_t error=cv_store_read(&d->store,req->uri+11,&bytes,&size);xSemaphoreGive(d->library_mutex);
        if(error!=CV_OK)return result(req,error,NULL);
        httpd_resp_set_type(req,"image/jpeg");httpd_resp_set_hdr(req,"Cache-Control","no-store");
        esp_err_t sent=httpd_resp_send(req,(const char *)bytes,size);free(bytes);return sent;
    }
    return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Not found");
}
static esp_err_t mutate(httpd_req_t *req)
{
    if(!authorized(req))return result(req,CV_AUTH,NULL);
    cv_device_t *d=req->user_ctx;cv_error_t error=CV_INVALID;char id[CV_ID_LENGTH+1]={0};
    if(req->method==HTTP_POST&&!strcmp(req->uri,"/api/photos")) {
        if(req->content_len<4||req->content_len>CV_MAX_JPEG)return result(req,CV_JPEG,NULL);
        uint8_t *body=malloc(req->content_len);if(!body)return result(req,CV_MEMORY,NULL);
        size_t offset=0;int64_t deadline=esp_timer_get_time()+10000000;
        while(offset<req->content_len) {
            if(esp_timer_get_time()>deadline){free(body);return result(req,CV_TIMEOUT,NULL);}
            int count=httpd_req_recv(req,(char *)body+offset,req->content_len-offset);
            if(count<=0){free(body);return result(req,CV_IO,NULL);}offset+=(size_t)count;
        }
        if(!authorized(req)){free(body);return result(req,CV_AUTH,NULL);}
        xSemaphoreTake(d->library_mutex,portMAX_DELAY);sdgoods_touch_flash_window(3000);
        error=cv_store_add(&d->store,body,offset,id);sdgoods_touch_flash_window_end();cv_device_library_changed(d);xSemaphoreGive(d->library_mutex);free(body);
    } else if(req->method==HTTP_PUT&&!strncmp(req->uri,"/api/current/",13)&&req->content_len==0) {
        xSemaphoreTake(d->library_mutex,portMAX_DELAY);sdgoods_touch_flash_window(1000);error=cv_store_select(&d->store,req->uri+13);
        sdgoods_touch_flash_window_end();cv_device_library_changed(d);xSemaphoreGive(d->library_mutex);
    } else if(req->method==HTTP_PUT&&!strncmp(req->uri,"/api/language/",14)&&req->content_len==0) {
        const char *language=req->uri+14;
        if(!strcmp(language,"en")||!strcmp(language,"zh")) {
            xSemaphoreTake(d->library_mutex,portMAX_DELAY);sdgoods_touch_flash_window(1000);
            error=cv_store_locale(&d->store,!strcmp(language,"zh")?CV_ZH:CV_EN);sdgoods_touch_flash_window_end();cv_device_library_changed(d);xSemaphoreGive(d->library_mutex);
        }
    } else if(req->method==HTTP_DELETE&&!strncmp(req->uri,"/api/image/",11)&&req->content_len==0) {
        xSemaphoreTake(d->library_mutex,portMAX_DELAY);sdgoods_touch_flash_window(1000);error=cv_store_delete(&d->store,req->uri+11);
        sdgoods_touch_flash_window_end();cv_device_library_changed(d);xSemaphoreGive(d->library_mutex);
    }
    return result(req,error,*id?id:NULL);
}
bool cv_http_start(cv_device_t *d)
{
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.uri_match_fn=httpd_uri_match_wildcard;
    config.stack_size=8192;config.max_open_sockets=4;config.recv_wait_timeout=3;config.send_wait_timeout=3;config.lru_purge_enable=true;
    if(httpd_start(&d->http,&config)!=ESP_OK)return false;
    const httpd_method_t methods[]={HTTP_GET,HTTP_POST,HTTP_PUT,HTTP_DELETE};
    for(unsigned i=0;i<4;i++) {
        httpd_uri_t uri={.uri="/*",.method=methods[i],.handler=i==0?get:mutate,.user_ctx=d};
        if(httpd_register_uri_handler(d->http,&uri)!=ESP_OK){cv_http_stop(d);return false;}
    }
    return true;
}
bool cv_http_stop(cv_device_t *d)
{
    if(d->http){if(httpd_stop(d->http)!=ESP_OK)return false;d->http=NULL;}
    return true;
}
