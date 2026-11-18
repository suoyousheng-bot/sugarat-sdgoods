/* SPDX-License-Identifier: Apache-2.0 */
/** Execute firmware redirect, captive-probe and authenticated profile handlers. */
#include "card_host_http.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static httpd_uri_t routes[2];
static unsigned route_count,fail_registration;
static bool fail_start,fail_receive;
static void copy(char *out,size_t size,const char *value){assert(strlen(value)<size);strcpy(out,value);}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *key,char *out,size_t size)
{
    const char *value=!strcmp(key,"Host")?r->host:!strcmp(key,"Authorization")?r->authorization:NULL;
    if(!value||strlen(value)>=size)return ESP_FAIL;copy(out,size,value);return ESP_OK;
}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *value){r->code=atoi(value);return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *value){copy(r->type,sizeof(r->type),value);return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *key,const char *value)
{
    if(!strcmp(key,"Location"))copy(r->location,sizeof(r->location),value);
    if(!strcmp(key,"Cache-Control"))copy(r->cache,sizeof(r->cache),value);
    if(!strcmp(key,"Referrer-Policy"))copy(r->referrer,sizeof(r->referrer),value);
    return ESP_OK;
}
esp_err_t httpd_resp_send(httpd_req_t *r,const char *body,ptrdiff_t size)
{assert(size>=0&&(size_t)size<sizeof(r->response));memcpy(r->response,body,size);r->response[size]=0;return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *body){return httpd_resp_send(r,body,strlen(body));}
int httpd_req_recv(httpd_req_t *r,char *out,size_t size)
{if(fail_receive)return -1;size_t left=r->content_len-r->offset;if(size>left)size=left;memcpy(out,r->body+r->offset,size);r->offset+=size;return (int)size;}
bool httpd_uri_match_wildcard(const char *a,const char *b,size_t size){(void)a;(void)b;(void)size;return true;}
esp_err_t httpd_start(httpd_handle_t *handle,httpd_config_t *config)
{assert(config->uri_match_fn==httpd_uri_match_wildcard&&config->max_uri_handlers==2);if(fail_start)return ESP_FAIL;*handle=routes;return ESP_OK;}
esp_err_t httpd_register_uri_handler(httpd_handle_t handle,const httpd_uri_t *uri)
{assert(handle==routes&&route_count<2);if(route_count+1==fail_registration)return ESP_FAIL;routes[route_count++]=*uri;return ESP_OK;}
static httpd_req_t request(card_network_t *n,httpd_method_t method,const char *uri,const char *host,const char *auth,const char *body)
{
    httpd_req_t r={.user_ctx=n,.uri=uri,.host=host,.authorization=auth,.body=body,.content_len=body?strlen(body):0,.code=200};
    unsigned index=method==HTTP_GET?0:1;assert(routes[index].method==method&&routes[index].user_ctx==n);
    assert(routes[index].handler(&r)==ESP_OK);return r;
}
int main(void)
{
    card_network_t n={.published={.name="Lin"}};
    strcpy(n.token,"0123456789abcdef0123456789abcdef");snprintf(n.url,sizeof(n.url),"http://192.168.4.1/edit#%s",n.token);
    fail_start=true;assert(!card_http_start(&n)&&!n.server);fail_start=false;
    fail_registration=2;assert(!card_http_start(&n)&&n.server);route_count=0;fail_registration=0;
    assert(card_http_start(&n)&&route_count==2);
    const char *probes[]={"/","/generate_204","/gen_204","/hotspot-detect.html","/connecttest.txt","/ncsi.txt","/redirect"};
    for(unsigned i=0;i<sizeof(probes)/sizeof(*probes);i++) {
        httpd_req_t r=request(&n,HTTP_GET,probes[i],"connectivitycheck.example",NULL,NULL);
        assert(r.code==302&&!strcmp(r.location,n.url)&&!strcmp(r.cache,"no-store")&&!strcmp(r.referrer,"no-referrer"));
    }
    httpd_req_t r=request(&n,HTTP_GET,"/captive-portal","192.168.4.1",NULL,NULL);
    assert(r.code==200&&!strcmp(r.type,"application/captive+json")&&strstr(r.response,n.url));
    r=request(&n,HTTP_GET,"/edit","192.168.4.1",NULL,NULL);assert(r.code==200&&strstr(r.response,"<form id=\"form\" novalidate>"));
    r=request(&n,HTTP_GET,"/edit?lang=en","192.168.4.1",NULL,NULL);assert(r.code==200&&strstr(r.response,"Edit your card"));
    const char *auth="Bearer 0123456789abcdef0123456789abcdef";
    r=request(&n,HTTP_GET,"/api/profile","192.168.4.1",NULL,NULL);assert(r.code==401&&strstr(r.response,"unauthorized"));
    r=request(&n,HTTP_GET,"/api/profile","192.168.4.1","Bearer ffffffffffffffffffffffffffffffff",NULL);assert(r.code==401&&strstr(r.response,"unauthorized"));
    r=request(&n,HTTP_GET,"/api/profile","foreign.example",auth,NULL);assert(r.code==401&&strstr(r.response,"unauthorized"));
    r=request(&n,HTTP_GET,"/api/profile","192.168.4.1:80",auth,NULL);assert(r.code==200&&strstr(r.response,"Lin"));
    r=request(&n,HTTP_GET,"/api/missing","192.168.4.1",auth,NULL);assert(r.code==404&&!r.location[0]&&strstr(r.response,"not_found"));
    r=request(&n,HTTP_GET,"/api/status","192.168.4.1",auth,NULL);assert(r.code==200&&strstr(r.response,"idle"));
    const char *body="{\"name\":\"New\",\"company\":\"\",\"role\":\"\",\"contact\":\"\",\"link\":\"\",\"kind\":0}";
    r=request(&n,HTTP_POST,"/api/profile","192.168.4.1",NULL,body);assert(r.code==401&&!n.queued&&strstr(r.response,"unauthorized"));
    r=request(&n,HTTP_POST,"/api/profile","192.168.4.1",auth,"{}");assert(r.code==400&&!n.queued&&strstr(r.response,"invalid"));
    char oversized[CARD_JSON_MAX+2];memset(oversized,'A',sizeof(oversized)-1);oversized[sizeof(oversized)-1]=0;
    r=request(&n,HTTP_POST,"/api/profile","192.168.4.1",auth,oversized);assert(r.code==413&&strstr(r.response,"too_large")&&!n.queued);
    fail_receive=true;r=request(&n,HTTP_POST,"/api/profile","192.168.4.1",auth,body);assert(r.code==408&&strstr(r.response,"timeout")&&!n.queued);fail_receive=false;
    r=request(&n,HTTP_POST,"/api/profile","192.168.4.1",auth,body);assert(r.code==202&&n.queued&&n.request==1&&!strcmp(n.pending.name,"New"));
    r=request(&n,HTTP_GET,"/api/profile","192.168.4.1",auth,NULL);assert(strstr(r.response,"Lin"));
    r=request(&n,HTTP_POST,"/api/profile","192.168.4.1",auth,body);assert(r.code==409&&n.request==1&&strstr(r.response,"busy"));
    r=request(&n,HTTP_GET,"/api/status","192.168.4.1",auth,NULL);assert(strstr(r.response,"queued"));
    n.queued=false;n.finished=true;n.failed=true;
    r=request(&n,HTTP_GET,"/api/status","192.168.4.1",auth,NULL);assert(strstr(r.response,"save_unconfirmed")&&!strstr(r.response,"saved"));
    n.failed=false;n.published=n.pending;
    r=request(&n,HTTP_GET,"/api/status","192.168.4.1",auth,NULL);assert(strstr(r.response,"saved"));
    strcpy(n.token,"ffffffffffffffffffffffffffffffff");snprintf(n.url,sizeof(n.url),"http://192.168.4.1/edit#%s",n.token);
    r=request(&n,HTTP_GET,"/api/profile","192.168.4.1",auth,NULL);assert(r.code==401&&strstr(r.response,"unauthorized"));
    r=request(&n,HTTP_GET,"/","192.168.4.1",NULL,NULL);assert(r.code==302&&!strcmp(r.location,n.url));
    puts("PASS: actual HTTP routes, root/probe redirects, captive JSON, token/Host checks, queued/saved/error states and token rotation");
    return 0;
}
