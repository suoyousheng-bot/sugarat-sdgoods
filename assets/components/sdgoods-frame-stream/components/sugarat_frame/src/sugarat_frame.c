/* Two-core work queue, double-buffered panel strips and parallel rotation; see sugarat_frame.h. */
#include "sugarat_frame.h"
#include <stdatomic.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdgoods_app_shell.h"
#include "sdgoods_lcd.h"

static const char *TAG="sugarat_frame";

/* ---- work queue: written by the caller before the helper is notified, read-only while it runs ---- */
static TaskHandle_t s_helper;
static SemaphoreHandle_t s_helper_done;
static sugarat_frame_job_fn s_fn;
static void *s_arg;
static int s_count;
static atomic_int s_next;

static void work_run(void)
{
    for(int i;(i=atomic_fetch_add(&s_next,1))<s_count;)s_fn(i,s_arg);
}

static void helper_task(void *arg)
{
    (void)arg;
    for(;;){
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        work_run();
        xSemaphoreGive(s_helper_done);
    }
}

sugarat_frame_workers_config_t sugarat_frame_workers_default_config(void)
{
    return (sugarat_frame_workers_config_t){.core=1,.priority=5,.stack_bytes=4096};
}

esp_err_t sugarat_frame_workers_start(const sugarat_frame_workers_config_t *config)
{
    if(!config)return ESP_ERR_INVALID_ARG;
    if(s_helper)return ESP_ERR_INVALID_STATE;
    if(!s_helper_done)s_helper_done=xSemaphoreCreateBinary();
    if(!s_helper_done)return ESP_ERR_NO_MEM;
    if(xTaskCreatePinnedToCore(helper_task,"frame_helper",config->stack_bytes,NULL,config->priority,&s_helper,config->core)!=pdPASS){
        s_helper=NULL;return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void sugarat_frame_parallel(int count,sugarat_frame_job_fn fn,void *arg)
{
    if(count<=0)return;
    if(!s_helper){for(int i=0;i<count;i++)fn(i,arg);return;}
    s_fn=fn;s_arg=arg;s_count=count;
    atomic_store(&s_next,0);
    xTaskNotifyGive(s_helper);
    work_run();
    xSemaphoreTake(s_helper_done,portMAX_DELAY);
}

/* ---- parallel rotation ---- */
typedef struct {
    const sugarat_rotate_job_t *job;
    sugarat_rotate_row_t rows[SUGARAT_ROTATE_TILE];
    int y0,y1,lo,hi;bool columns;
} rotate_work_t;

static void rotate_unit(int index,void *arg)
{
    rotate_work_t *w=arg;
    if(w->columns){
        int x=w->lo+index*SUGARAT_ROTATE_TILE,end=x+SUGARAT_ROTATE_TILE-1<w->hi?x+SUGARAT_ROTATE_TILE-1:w->hi;
        sugarat_rotate_tile(w->job,w->rows,w->y0,w->y1-w->y0,x,end);
    }else{
        int y=w->y0+index*SUGARAT_ROTATE_TILE,end=y+SUGARAT_ROTATE_TILE<w->y1?y+SUGARAT_ROTATE_TILE:w->y1;
        sugarat_rotate_rows(w->job,y,end,w->lo,w->hi);
    }
}

void sugarat_frame_rotate(const sugarat_rotate_job_t *job,int y0,int y1,int lo,int hi)
{
    if(y1<=y0 || hi<lo)return;
    static rotate_work_t work;   // one caller at a time, as for sugarat_frame_parallel()
    work.job=job;work.y0=y0;work.y1=y1;work.lo=lo;work.hi=hi;work.columns=y1-y0<=SUGARAT_ROTATE_TILE;
    if(work.columns)for(int y=y0;y<y1;y++)work.rows[y-y0]=sugarat_rotate_row(job,y);
    int count=work.columns?(hi-lo+SUGARAT_ROTATE_TILE)/SUGARAT_ROTATE_TILE:(y1-y0+SUGARAT_ROTATE_TILE-1)/SUGARAT_ROTATE_TILE;
    sugarat_frame_parallel(count,rotate_unit,&work);
}

/* ---- strip stream ---- */
typedef struct { uint16_t *pixels;int x1,y1,x2,y2;unsigned index; } strip_t;

struct sugarat_frame_stream {
    sugarat_frame_stream_config_t config;
    uint16_t *strips[2];
    SemaphoreHandle_t free[2];
    QueueHandle_t queue;
    volatile esp_err_t error;
    volatile int64_t send_us;
    bool broken;
};

static void sender_task(void *arg)
{
    sugarat_frame_stream_t *s=arg;
    for(;;){
        strip_t strip;
        xQueueReceive(s->queue,&strip,portMAX_DELAY);
        int64_t t=esp_timer_get_time();
        esp_err_t err=sdgoods_lcd_draw_bitmap_safe(strip.x1,strip.y1,strip.x2,strip.y2,strip.pixels);
        s->send_us+=esp_timer_get_time()-t;
        if(err!=ESP_OK)s->error=err;
        xSemaphoreGive(s->free[strip.index]);
    }
}

sugarat_frame_stream_config_t sugarat_frame_stream_default_config(void)
{
    return (sugarat_frame_stream_config_t){.max_width=360,.rows=12,.sender_core=1,.sender_priority=7,.timeout_ms=3000};
}

esp_err_t sugarat_frame_stream_create(const sugarat_frame_stream_config_t *config,sugarat_frame_stream_t **out)
{
    if(!config || !out || config->max_width<=0 || config->rows<=0)return ESP_ERR_INVALID_ARG;
    *out=NULL;
    sugarat_frame_stream_t *s=calloc(1,sizeof(*s));
    if(!s)return ESP_ERR_NO_MEM;
    s->config=*config;
    size_t bytes=(size_t)config->max_width*config->rows*2;
    for(int i=0;i<2;i++){
        s->strips[i]=heap_caps_malloc(bytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
        s->free[i]=xSemaphoreCreateBinary();
        if(s->free[i])xSemaphoreGive(s->free[i]);
    }
    s->queue=xQueueCreate(2,sizeof(strip_t));
    bool ok=s->strips[0] && s->free[0] && s->free[1] && s->queue &&
            xTaskCreatePinnedToCore(sender_task,"frame_sender",3072,s,config->sender_priority,NULL,config->sender_core)==pdPASS;
    if(!ok){
        for(int i=0;i<2;i++){heap_caps_free(s->strips[i]);if(s->free[i])vSemaphoreDelete(s->free[i]);}
        if(s->queue)vQueueDelete(s->queue);
        free(s);
        return ESP_ERR_NO_MEM;
    }
    if(!s->strips[1])ESP_LOGW(TAG,"One internal DMA strip; rendering and transfer will not overlap");
    *out=s;
    return ESP_OK;
}

int sugarat_frame_stream_buffers(const sugarat_frame_stream_t *stream){return stream->strips[1]?2:1;}

int64_t sugarat_frame_stream_take_send_us(sugarat_frame_stream_t *stream)
{
    int64_t us=stream->send_us;stream->send_us=0;
    return us;
}

esp_err_t sugarat_frame_stream_send(sugarat_frame_stream_t *s,int x1,int y1,int x2,int y2,sugarat_frame_strip_fn fill,void *arg)
{
    if(!s || !fill || x2<=x1 || y2<=y1 || x2-x1>s->config.max_width)return ESP_ERR_INVALID_ARG;
    if(s->broken)return ESP_ERR_INVALID_STATE;
    const unsigned buffers=s->strips[1]?2:1;
    const TickType_t wait=pdMS_TO_TICKS(s->config.timeout_ms);
    s->error=ESP_OK;
    esp_err_t result=ESP_OK;
    for(int y=y1,k=0;y<y2;y+=s->config.rows,k++){
        unsigned index=k%buffers;
        if(xSemaphoreTake(s->free[index],wait)!=pdTRUE){result=ESP_ERR_TIMEOUT;break;}
        if(s->error!=ESP_OK){xSemaphoreGive(s->free[index]);break;}
        int rows=y2-y<s->config.rows?y2-y:s->config.rows;
        fill(s->strips[index],x1,y,x2,rows,arg);
        strip_t strip={s->strips[index],x1,y,x2,y+rows,index};
        xQueueSend(s->queue,&strip,portMAX_DELAY);
    }
    // Each strip's semaphore returns when its transfer ends.
    unsigned taken=0;
    for(unsigned i=0;i<buffers;i++)if(xSemaphoreTake(s->free[i],wait)==pdTRUE)taken|=1u<<i;
    for(unsigned i=0;i<buffers;i++)if(taken&(1u<<i))xSemaphoreGive(s->free[i]);
    if(taken!=(1u<<buffers)-1 || result==ESP_ERR_TIMEOUT){
        // A strip may still be owned by the sender: never reuse the buffers.
        s->broken=true;
        ESP_LOGE(TAG,"Panel strip did not free within %u ms; stream disabled",(unsigned)s->config.timeout_ms);
        return ESP_ERR_TIMEOUT;
    }
    return s->error;
}

bool sugarat_frame_lvgl_layers_clear(void)
{
    return !lv_obj_get_child_cnt(lv_layer_top()) && !lv_obj_get_child_cnt(lv_layer_sys()) && !sdgoods_app_shell_menu_is_open();
}
