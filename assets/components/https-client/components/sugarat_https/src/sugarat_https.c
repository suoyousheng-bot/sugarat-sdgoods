/* Bounded HTTPS requests; see sugarat_https.h. */
#include "sugarat_https.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG="sugarat_https";
enum { INITIAL_CAPACITY=4096 };

static StaticSemaphore_t s_lock_buffer;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_init=portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    uint8_t *data;size_t used,capacity,limit;
    uint32_t caps;bool overflow,no_memory;int retry_after_s;
} body_t;

/* Grows the body without leaving a copy of earlier bytes in freed memory. */
static bool body_reserve(body_t *b,size_t need)
{
    if(need<=b->capacity)return true;
    size_t capacity=b->capacity?b->capacity:INITIAL_CAPACITY;
    while(capacity<need)capacity*=2;
    if(capacity>b->limit+1)capacity=b->limit+1;
    uint8_t *next=heap_caps_malloc(capacity,b->caps);
    if(!next && b->caps!=MALLOC_CAP_DEFAULT)next=heap_caps_malloc(capacity,MALLOC_CAP_DEFAULT);
    if(!next)return false;
    if(b->data){memcpy(next,b->data,b->used);memset(b->data,0,b->capacity);heap_caps_free(b->data);}
    b->data=next;b->capacity=capacity;
    return true;
}

static esp_err_t on_event(esp_http_client_event_t *event)
{
    body_t *b=event->user_data;
    if(event->event_id==HTTP_EVENT_ON_DATA && event->data_len>0 && !b->overflow && !b->no_memory){
        size_t length=(size_t)event->data_len;
        if(b->used+length>b->limit){b->overflow=true;return ESP_OK;}
        if(!body_reserve(b,b->used+length+1)){b->no_memory=true;return ESP_OK;}
        memcpy(b->data+b->used,event->data,length);b->used+=length;b->data[b->used]=0;
    }else if(event->event_id==HTTP_EVENT_ON_HEADER && !strcasecmp(event->header_key,"Retry-After")){
        const char *v=event->header_value;
        b->retry_after_s=isdigit((unsigned char)v[0])?atoi(v):60;
    }
    return ESP_OK;
}

static const char *method_name(esp_http_client_method_t method)
{
    switch(method){
    case HTTP_METHOD_GET:return "GET";
    case HTTP_METHOD_POST:return "POST";
    case HTTP_METHOD_PUT:return "PUT";
    case HTTP_METHOD_PATCH:return "PATCH";
    case HTTP_METHOD_DELETE:return "DELETE";
    case HTTP_METHOD_HEAD:return "HEAD";
    default:return "HTTP";   // other methods are logged without a name
    }
}

/* Copies the URL host for logging; paths and queries may carry credentials. */
static void url_host(const char *url,char *out,size_t size)
{
    const char *start=url+8;   // after "https://"
    size_t n=strcspn(start,"/:?#");
    if(n>=size)n=size-1;
    memcpy(out,start,n);out[n]=0;
}

sugarat_https_request_t sugarat_https_request_default(const char *url)
{
    return (sugarat_https_request_t){.url=url,.method=HTTP_METHOD_GET,.max_response_bytes=64*1024,.timeout_ms=12000,.prefer_psram=true};
}

void sugarat_https_response_free(sugarat_https_response_t *response)
{
    if(!response)return;
    if(response->body){memset(response->body,0,response->length);heap_caps_free(response->body);}
    *response=(sugarat_https_response_t){0};
}

esp_err_t sugarat_https_perform(const sugarat_https_request_t *request,sugarat_https_response_t *response)
{
    if(!response)return ESP_ERR_INVALID_ARG;
    *response=(sugarat_https_response_t){0};
    if(!request || !request->url || strncmp(request->url,"https://",8) || !request->max_response_bytes ||
       (request->body_length && (!request->body || !request->content_type)) || request->timeout_ms<=0)return ESP_ERR_INVALID_ARG;
    taskENTER_CRITICAL(&s_lock_init);
    if(!s_lock)s_lock=xSemaphoreCreateMutexStatic(&s_lock_buffer);
    taskEXIT_CRITICAL(&s_lock_init);
    xSemaphoreTake(s_lock,portMAX_DELAY);

    body_t body={.limit=request->max_response_bytes,.caps=request->prefer_psram?MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT:MALLOC_CAP_DEFAULT};
    esp_http_client_config_t config={.url=request->url,.method=request->method,.timeout_ms=request->timeout_ms,
        .crt_bundle_attach=esp_crt_bundle_attach,.disable_auto_redirect=true,
        .buffer_size=4096,.buffer_size_tx=4096,.event_handler=on_event,.user_data=&body};
    esp_http_client_handle_t client=esp_http_client_init(&config);
    esp_err_t err=client?ESP_OK:ESP_ERR_NO_MEM;
    for(size_t i=0;err==ESP_OK && i<request->header_count;i++)
        err=esp_http_client_set_header(client,request->headers[i].name,request->headers[i].value);
    if(err==ESP_OK && request->body_length){
        err=esp_http_client_set_header(client,"Content-Type",request->content_type);
        if(err==ESP_OK)err=esp_http_client_set_post_field(client,(const char *)request->body,(int)request->body_length);
    }
    if(err==ESP_OK)err=esp_http_client_perform(client);
    if(err==ESP_OK)response->status=esp_http_client_get_status_code(client);
    if(client)esp_http_client_cleanup(client);
    xSemaphoreGive(s_lock);

    response->retry_after_s=body.retry_after_s;
    if(err==ESP_OK && body.overflow)err=ESP_ERR_INVALID_SIZE;
    if(err==ESP_OK && body.no_memory)err=ESP_ERR_NO_MEM;
    if(err==ESP_OK && body.used){response->body=body.data;response->length=body.used;body.data=NULL;}
    if(body.data){memset(body.data,0,body.capacity);heap_caps_free(body.data);}
    char host[64];url_host(request->url,host,sizeof(host));
    if(response->status)ESP_LOGI(TAG,"%s %s -> %d (%u bytes)%s",method_name(request->method),host,response->status,
                                 (unsigned)response->length,err==ESP_ERR_INVALID_SIZE?" body over limit":"");
    else ESP_LOGW(TAG,"%s %s failed: %s",method_name(request->method),host,esp_err_to_name(err));
    return err;
}
