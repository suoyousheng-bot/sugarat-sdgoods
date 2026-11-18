/* SPDX-License-Identifier: Apache-2.0 */
/** Serve the firmware's actual HTTP handlers over a bounded pipe owned by the test server. */
#define CV_HOST_HTTP 1
#include "../../main/cover/cv_http.c"
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include "lvgl.h"
host_asset_t cv_host_assets[4];
void cv_device_library_changed(cv_device_t *d){d->status.library=d->store.library;}
int64_t esp_timer_get_time(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
size_t httpd_req_get_hdr_value_len(httpd_req_t *r,const char *key){return !strcmp(key,"X-Cover-Token")&&r->token?strlen(r->token):0;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *key,char *out,size_t n)
{size_t size=httpd_req_get_hdr_value_len(r,key);if(size>=n)return -1;memcpy(out,r->token,size);out[size]=0;return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *type){snprintf(r->type,sizeof(r->type),"%s",type);return 0;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *key,const char *value){if(!strcmp(key,"Location"))snprintf(r->location,sizeof(r->location),"%s",value);return 0;}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *status){r->status=atoi(status);return 0;}
esp_err_t httpd_resp_send(httpd_req_t *r,const char *body,ptrdiff_t n)
{if(n<0)return -1;free(r->response);r->response=malloc((size_t)n+1);assert(r->response);memcpy(r->response,body,(size_t)n);r->response_size=(size_t)n;return 0;}
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *body){return httpd_resp_send(r,body,(ptrdiff_t)strlen(body));}
esp_err_t httpd_resp_send_err(httpd_req_t *r,int status,const char *body){r->status=status;return httpd_resp_sendstr(r,body);}
int httpd_req_recv(httpd_req_t *r,char *out,size_t count)
{size_t left=r->content_len-r->offset;if(count>left)count=left;memcpy(out,r->body+r->offset,count);r->offset+=count;return (int)count;}
bool httpd_uri_match_wildcard(const char *a,const char *b,size_t n){(void)a;(void)b;(void)n;return true;}
esp_err_t httpd_start(httpd_handle_t *handle,httpd_config_t *config){(void)config;*handle=(void *)1;return 0;}
esp_err_t httpd_register_uri_handler(httpd_handle_t handle,const httpd_uri_t *uri){(void)handle;(void)uri;return 0;}
esp_err_t httpd_stop(httpd_handle_t handle){(void)handle;return 0;}
static host_asset_t read_file(const char *path)
{
    FILE *f=fopen(path,"rb");assert(f);assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>=0);rewind(f);host_asset_t a={.size=(size_t)n,.bytes=malloc((size_t)n+1)};assert(a.bytes);assert(fread(a.bytes,1,a.size,f)==a.size);fclose(f);return a;
}
static bool settings_write(void *user,const char *id,cv_locale_t locale)
{
    cv_device_t *d=user;char path[256],next[256];snprintf(path,sizeof(path),"%s/preferences",d->directory);snprintf(next,sizeof(next),"%s/preferences.next",d->directory);
    FILE *f=fopen(next,"wb");if(!f)return false;
    bool ok=fprintf(f,"%u %s\n",(unsigned)locale,id)>0&&fflush(f)==0&&fsync(fileno(f))==0;if(fclose(f)!=0)ok=false;
    return ok&&rename(next,path)==0;
}
int main(int argc,char **argv)
{
    assert(argc==4);lv_init();cv_device_t device={0};assert(strlen(argv[3])<sizeof(device.directory));strcpy(device.directory,argv[3]);
    const char *names[]={"index.html","app.js","style.css","font.woff2"};
    for(unsigned i=0;i<4;i++){char path[512];snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);cv_host_assets[i]=read_file(path);}
    char prefs[256],current[33]={0};unsigned language=0;snprintf(prefs,sizeof(prefs),"%s/preferences",device.directory);FILE *f=fopen(prefs,"rb");if(f){if(fscanf(f,"%u %32s",&language,current)!=2)current[0]=0;fclose(f);}
    assert(cv_store_open(&device.store,device.directory,current,language==1?CV_ZH:CV_EN,settings_write,&device)==CV_OK);
    if(!device.store.library.count){host_asset_t sample=read_file(argv[2]);char id[33];assert(cv_store_add(&device.store,sample.bytes,sample.size,id)==CV_OK);assert(cv_store_select(&device.store,id)==CV_OK);free(sample.bytes);}
    memset(device.status.token,'1',32);strcpy(device.status.ssid,"Sugarat-Test");
    httpd_req_t probe={.user_ctx=&device,.uri="/generate_204",.method=HTTP_GET,.status=200};
    get(&probe);assert(probe.status==302);assert(!strcmp(probe.location,CV_PORTAL_ORIGIN "/gallery#11111111111111111111111111111111"));free(probe.response);
    char *line=NULL;size_t capacity=0;
    while(getline(&line,&capacity,stdin)>0) {
        unsigned method=0;char uri[256],token[64];size_t size=0;
        if(sscanf(line,"%u %255s %63s %zu",&method,uri,token,&size)!=4||method>3||size>CV_MAX_JPEG+1)break;
        uint8_t *body=malloc(size+1);assert(body);if(fread(body,1,size,stdin)!=size){free(body);break;}
        httpd_req_t request={.user_ctx=&device,.uri=uri,.token=strcmp(token,"-")?token:"",.method=(httpd_method_t)method,.body=body,.content_len=size,.status=200};strcpy(request.type,"text/plain");
        if(method==HTTP_GET)get(&request);else mutate(&request);
        for(char *p=request.type;*p;p++)if(*p==' ')*p='~';
        printf("%d %s %zu %s\n",request.status,request.type,request.response_size,*request.location?request.location:"-");fwrite(request.response,1,request.response_size,stdout);fflush(stdout);
        free(request.response);free(body);
    }
    free(line);for(unsigned i=0;i<4;i++)free(cv_host_assets[i].bytes);return 0;
}
