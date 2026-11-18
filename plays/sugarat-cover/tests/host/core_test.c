/* SPDX-License-Identifier: Apache-2.0 */
/** Execute storage and encrypted transfer sources with an injected, lossy datagram transport. */
#include "cv_store.h"
#include "cv_link.h"
#include "cv_jpeg.h"
#include "cv_gesture.h"
#include "cv_copy.h"
#include "lvgl.h"
#include "../../components/sdgoods_board/src/sdgoods_touch_window.h"
#include "../../components/sugarat_portal_dns/sugarat_wifi_dns_packet.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
typedef struct {bool fail;char current[33];cv_locale_t locale;} settings_t;
static bool settings(void *user,const char *id,cv_locale_t locale)
{settings_t *s=user;if(s->fail)return false;strcpy(s->current,id);s->locale=locale;return true;}
static uint8_t *read_file(const char *path,size_t *size)
{
    FILE *f=fopen(path,"rb");assert(f);assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>0);rewind(f);
    uint8_t *bytes=malloc((size_t)n);assert(bytes);assert(fread(bytes,1,(size_t)n,f)==(size_t)n);assert(fclose(f)==0);*size=(size_t)n;return bytes;
}
typedef struct {uint8_t bytes[CV_PACKET],source[6];size_t size;unsigned destination;} frame_t;
typedef struct wire wire_t;
typedef struct {wire_t *wire;unsigned index;cv_store_t store;settings_t settings;unsigned saves;} endpoint_t;
struct wire {cv_link_t link[2];endpoint_t ends[2];frame_t frames[256];unsigned head,tail,sent;bool lossy,drop_done,fail_save;};
static bool send_packet(void *user,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{
    endpoint_t *end=user;wire_t *w=end->wire;assert(size<=CV_PACKET);w->sent++;
    if(w->drop_done&&bytes[4]==8)return true;
    if(w->lossy&&w->sent%7==0)return true;
    assert(w->tail-w->head<254);
    frame_t *f=&w->frames[w->tail++%256];memcpy(f->bytes,bytes,size);memcpy(f->source,w->link[end->index].own,6);f->size=size;f->destination=1-end->index;
    if(w->lossy&&w->sent%11==0&&bytes[5])f->bytes[size-1]^=1;
    if(w->lossy&&w->sent%5==0){w->frames[w->tail%256]=*f;w->tail++;}
    if(bytes[4]>=4)assert(bytes[5]==1);
    (void)mac;return true;
}
static cv_error_t save(void *user,const uint8_t *data,size_t size,char id[33])
{
    endpoint_t *end=user;end->saves++;if(end->wire->fail_save)return CV_IO;
    return cv_store_add(&end->store,data,size,id);
}
static void relay(wire_t *w)
{
    unsigned budget=10000;
    while(w->head<w->tail&&budget--) {
        frame_t frame=w->frames[w->head++%256];cv_link_receive(&w->link[frame.destination],frame.source,frame.bytes,frame.size);
    }
    assert(budget);
}
static void start(wire_t *w,const char *directory,const uint8_t *seed,size_t size)
{
    memset(w,0,sizeof(*w));
    for(unsigned i=0;i<2;i++) {
        endpoint_t *end=&w->ends[i];end->wire=w;end->index=i;char path[512];snprintf(path,sizeof(path),"%s/side-%u",directory,i);assert(mkdir(path,0700)==0);
        assert(cv_store_open(&end->store,path,"",CV_EN,settings,&end->settings)==CV_OK);
        char id[33];assert(cv_store_add(&end->store,seed,size,id)==CV_OK);assert(cv_store_select(&end->store,id)==CV_OK);
        uint8_t mac[6]={2,0,0,0,0,(uint8_t)(i+1)};assert(cv_link_open(&w->link[i],i==1,mac,send_packet,save,end,0));
    }
}
static void pair(wire_t *w,const uint8_t *photo,size_t size)
{
    cv_link_tick(&w->link[0],700);cv_link_tick(&w->link[1],700);relay(w);
    assert(w->link[0].peer_count==1);assert(cv_link_offer(&w->link[0],0,photo,size));relay(w);
    assert(w->link[0].phase==L_CONFIRM&&w->link[1].phase==L_CONFIRM);assert(w->link[0].code==w->link[1].code);
    assert(!memcmp(w->link[0].key,w->link[1].key,32));assert(w->ends[1].saves==0);
}
static void run(wire_t *w,uint64_t until)
{
    for(uint64_t now=w->link[0].now+20;now<until;now+=20){cv_link_tick(&w->link[0],now);cv_link_tick(&w->link[1],now);relay(w);}
}
static void close_links(wire_t *w){for(unsigned i=0;i<2;i++)cv_link_close(&w->link[i]);}
int main(int argc,char **argv)
{
    uint8_t query[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,5,'p','h','o','n','e',5,'l','o','c','a','l',0,0,1,0,1};
    uint8_t packet[512],ip[]={192,168,4,1};uint32_t address;memcpy(&address,ip,4);
    for(size_t n=0;n<sizeof(query);n++){memcpy(packet,query,n);assert(sugarat_wifi_dns_reply(packet,n,sizeof(packet),address)==0);}
    memcpy(packet,query,sizeof(query));size_t response=sugarat_wifi_dns_reply(packet,sizeof(query),sizeof(packet),address);
    assert(response==sizeof(query)+16&&packet[7]==1&&!memcmp(packet+response-4,ip,4));
    memcpy(packet,query,sizeof(query));packet[26]=28;assert(sugarat_wifi_dns_reply(packet,sizeof(query),sizeof(packet),address)==sizeof(query)&&packet[7]==0);
    memcpy(packet,query,sizeof(query));packet[12]=0xc0;assert(!sugarat_wifi_dns_reply(packet,sizeof(query),sizeof(packet),address));
    assert(argc==3);lv_init();size_t size;uint8_t *photo=read_file(argv[1],&size),*other=malloc(size);assert(other);memcpy(other,photo,size);other[14]^=1;
    assert(cv_jpeg(photo,size,NULL)==CV_OK&&cv_jpeg(other,size,NULL)==CV_OK);assert(cv_jpeg(photo,size-1,NULL)==CV_JPEG);
    uint8_t invalid[4]={0xff,0xd8,0xff,0xd9};assert(cv_jpeg(invalid,4,NULL)==CV_JPEG);
    settings_t preferences={0};cv_store_t store;char directory[512];snprintf(directory,sizeof(directory),"%s/library",argv[2]);assert(mkdir(directory,0700)==0);
    assert(cv_store_open(&store,directory,"",CV_EN,settings,&preferences)==CV_OK);assert(store.library.locale==CV_EN);
    char a[33],b[33];assert(cv_store_add(&store,photo,size,a)==CV_OK);assert(cv_store_add(&store,other,size,b)==CV_OK);assert(strcmp(a,b));
    assert(cv_store_add(&store,photo,size,a)==CV_OK&&store.library.count==2);assert(cv_store_select(&store,a)==CV_OK);
    preferences.fail=true;assert(cv_store_select(&store,b)==CV_SETTINGS&&!strcmp(store.current,a));assert(cv_store_locale(&store,CV_ZH)==CV_SETTINGS&&store.library.locale==CV_EN);
    preferences.fail=false;assert(cv_store_locale(&store,CV_ZH)==CV_OK);cv_store_t reopened;assert(cv_store_open(&reopened,directory,preferences.current,preferences.locale,settings,&preferences)==CV_OK);assert(reopened.library.locale==CV_ZH&&!strcmp(reopened.current,a));
    uint8_t *bytes=NULL;size_t length=0;assert(cv_store_read(&store,"../anything",&bytes,&length)==CV_INVALID);assert(cv_store_delete(&store,a)==CV_BUSY);
    assert(cv_store_delete(&store,b)==CV_OK&&store.library.count==1);char retained[192];strcpy(retained,store.directory);strcat(store.directory,"/missing");assert(cv_store_add(&store,other,size,b)==CV_IO);strcpy(store.directory,retained);
    assert(cv_store_read(&store,a,&bytes,&length)==CV_OK&&length==size&&!memcmp(bytes,photo,size));free(bytes);
    assert(cv_store_load(&store,a,&bytes,&length)==CV_OK&&length==size&&!memcmp(bytes,photo,size));free(bytes);
    {char forged[256];const char *wrong="0123456789abcdef0123456789abcdef";snprintf(forged,sizeof(forged),"%s/%s.jpg",store.directory,wrong);
     FILE *file=fopen(forged,"wb");assert(file&&fwrite(photo,1,size,file)==size&&fclose(file)==0);
     assert(cv_store_load(&store,wrong,&bytes,&length)==CV_CORRUPT&&!bytes);assert(remove(forged)==0);}
    cv_gesture_t gesture;cv_gesture_press(&gesture,180,200,3);cv_gesture_move(&gesture,230,200);cv_gesture_move(&gesture,180,200);assert(!cv_gesture_tap(&gesture,3,true));assert(cv_gesture_swipe(&gesture,80,200)==0);
    sdg_touch_window_t window={0};sdg_touch_window_begin(&window,100,3000);assert(sdg_touch_window_busy(&window,120));sdg_touch_window_end(&window,125,40);assert(sdg_touch_window_busy(&window,164));assert(!sdg_touch_window_busy(&window,165));
    sdg_touch_window_begin(&window,INT32_MAX-10,100);assert(sdg_touch_window_busy(&window,(int32_t)((uint32_t)INT32_MAX+20)));sdg_touch_window_end(&window,(int32_t)((uint32_t)INT32_MAX+20),40);assert(!sdg_touch_window_busy(&window,(int32_t)((uint32_t)INT32_MAX+60)));
    cv_gesture_press(&gesture,240,180,-1);cv_gesture_move(&gesture,120,180);assert(cv_gesture_swipe(&gesture,120,180)==-1);assert(!cv_gesture_tap(&gesture,-1,true));
    cv_gesture_press(&gesture,240,30,-1);assert(cv_gesture_swipe(&gesture,100,30)==0);cv_gesture_press(&gesture,240,180,-1);assert(cv_gesture_swipe(&gesture,220,180)==0);assert(cv_gesture_swipe(&gesture,180,250)==0);
    cv_gesture_press(&gesture,90,180,3);assert(cv_gesture_back(&gesture,240,185));assert(!cv_gesture_back(&gesture,130,185));assert(!cv_gesture_back(&gesture,240,290));assert(!cv_gesture_back(&gesture,20,180));gesture.lost=true;assert(!cv_gesture_back(&gesture,240,180));
    for(int locale=CV_EN;locale<=CV_ZH;locale++)for(int key=0;key<C_COUNT;key++)assert(*cv_text((cv_locale_t)locale,(cv_copy_key_t)key));
    wire_t *wire=calloc(1,sizeof(*wire));assert(wire);
    for(unsigned scenario=0;scenario<4;scenario++) {
        char scenario_dir[512];snprintf(scenario_dir,sizeof(scenario_dir),"%s/transfer-%u",argv[2],scenario);assert(mkdir(scenario_dir,0700)==0);
        start(wire,scenario_dir,other,size);pair(wire,photo,size);wire->lossy=scenario==1;wire->drop_done=scenario==2;wire->fail_save=scenario==3;
        cv_link_confirm(&wire->link[0]);relay(wire);run(wire,3000);assert(wire->ends[1].saves==0);
        cv_link_confirm(&wire->link[1]);relay(wire);run(wire,60000);
        if(scenario==2){assert(wire->link[0].phase==L_FAILED&&wire->link[0].error==CV_UNKNOWN);assert(wire->link[1].phase==L_DONE);}
        else if(scenario==3){assert(wire->link[0].phase==L_FAILED&&wire->link[0].error==CV_IO);}
        else {if(wire->link[0].phase!=L_DONE||wire->link[1].phase!=L_DONE)fprintf(stderr,"scenario=%u phases=%u/%u errors=%u/%u offset=%u/%u saves=%u\n",scenario,wire->link[0].phase,wire->link[1].phase,wire->link[0].error,wire->link[1].error,wire->link[0].offset,wire->link[1].offset,wire->ends[1].saves);assert(wire->link[0].phase==L_DONE&&wire->link[1].phase==L_DONE);assert(wire->ends[1].saves==1);assert(wire->ends[1].store.library.count==2);}
        assert(!strcmp(wire->ends[1].store.current,wire->ends[1].settings.current));close_links(wire);
    }
    free(wire);free(other);free(photo);puts("PASS: JPEG validation, durable library selection, language persistence, preserved originals, gesture isolation, encrypted paired transfer, loss/corruption/duplicates, unknown delivery and save failure");return 0;
}
