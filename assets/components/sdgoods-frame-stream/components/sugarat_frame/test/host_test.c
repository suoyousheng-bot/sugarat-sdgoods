/* Host test for the pure-C headers: cc -std=c11 -fsanitize=address,undefined -Iinclude test/host_test.c -lm */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "sugarat_frame_pacing.h"
#include "sugarat_rotate.h"

enum { SIZE=360 };
static uint16_t px(const uint16_t *f,int x,int y){return sugarat_rotate_swap(f[y*SIZE+x]);}

int main(void)
{
    // A 24 ms frame on a 16.667 ms grid starts every second slot; a 5 ms frame every slot.
    sugarat_frame_pacer_t pacer;sugarat_frame_pacer_init(&pacer,SUGARAT_FRAME_SLOT_60HZ_US);
    int64_t t=1000000,starts[6];int n=0;
    for(int64_t now=t;n<6;now+=1000)if(sugarat_frame_pacer_due(&pacer,now)){starts[n++]=now;sugarat_frame_pacer_finish(&pacer,now+24000,true);}
    for(int i=1;i<6;i++)assert(starts[i]-starts[i-1]>=33000 && starts[i]-starts[i-1]<=34000);
    sugarat_frame_pacer_reset(&pacer);n=0;
    for(int64_t now=t;n<6;now+=1000)if(sugarat_frame_pacer_due(&pacer,now)){starts[n++]=now;sugarat_frame_pacer_finish(&pacer,now+5000,true);}
    for(int i=1;i<6;i++)assert(starts[i]-starts[i-1]>=16000 && starts[i]-starts[i-1]<=17000);
    sugarat_frame_stats_t stats={0};sugarat_frame_summary_t sum;
    for(int i=0;i<30;i++)sugarat_frame_stats_record(&stats,t+i*33333,t+i*33333+24000,true,true);
    assert(sugarat_frame_stats_window(&stats,t+1000000,1000000,&sum));
    assert(sum.frames==30 && sum.direct_frames==30 && sum.avg_us==24000 && sum.gap_max_us==33333 && sum.fps>29.9f && sum.fps<30.1f);

    uint16_t *tex=malloc(SIZE*SIZE*2),*out=malloc(SIZE*SIZE*2+64),*first=malloc(SIZE*SIZE*2),*noise=malloc(SIZE*SIZE*2);
    assert(tex&&out&&first&&noise);
    // Asymmetric quadrants make the direction and quarter-turn observable.
    for(int y=0;y<SIZE;y++)for(int x=0;x<SIZE;x++)tex[y*SIZE+x]=x<180?(y<180?0xf800:0x001f):(y<180?0x07e0:0xffff);
    uint32_t seed=7;
    for(int i=0;i<SIZE*SIZE;i++){seed=seed*1664525u+1013904223u;noise[i]=(uint16_t)(seed>>16);}
    // The unclamped interior sampler equals the clamped one wherever interior pixels can sample.
    for(int i=0;i<200000;i++){
        seed=seed*1664525u+1013904223u;int32_t u=(int32_t)(seed%(358u<<16));
        seed=seed*1664525u+1013904223u;int32_t v=(int32_t)(seed%(358u<<16));
        assert(sugarat_rotate_sample_core(noise,SIZE,u,v)==sugarat_rotate_sample(noise,SIZE,u,v));
    }
    sugarat_rotate_params_t p={tex,SIZE,NULL,0,0,180,180,180,0,true};
    memset(out+SIZE*SIZE,0xa5,64);
    sugarat_rotate_job_t j=sugarat_rotate_job(&p,0,out,32);
    assert(j.stride==SIZE);
    sugarat_rotate_rows(&j,0,SIZE,0,SIZE-1);memcpy(first,out,SIZE*SIZE*2);
    assert(px(out,218,143)==0x07e0 && px(out,0,0)==0);
    j=sugarat_rotate_job(&p,1.5707963f,out,32);sugarat_rotate_rows(&j,0,SIZE,0,SIZE-1);
    assert(px(out,218,143)==0xf800);   // clockwise: top-left artwork reaches the top-right
    for(int y=0;y<SIZE;y++)for(int x=0;x<SIZE;x++){float dx=x+0.5f-180,dy=y+0.5f-180;if(dx*dx+dy*dy>181*181)assert(px(out,x,y)==0);}
    for(int i=0;i<64;i++)assert(((uint8_t *)(out+SIZE*SIZE))[i]==0xa5);
    // A hole shows the backdrop colour; a smaller texture is upscaled without leaving its bounds.
    uint16_t small[64*64];for(int i=0;i<64*64;i++)small[i]=noise[i];
    sugarat_rotate_params_t q={small,64,NULL,0,0x001f,180,180,150,12,false};
    for(float a=0;a<6.3f;a+=0.37f){
        sugarat_rotate_job_t k=sugarat_rotate_job(&q,a,out,32);sugarat_rotate_rows(&k,0,SIZE,0,SIZE-1);
        assert(px(out,180,180)==0x001f && px(out,5,5)==0x001f && px(out,180+40,180)!=0x001f);
        for(int i=0;i<64;i++)assert(((uint8_t *)(out+SIZE*SIZE))[i]==0xa5);
    }
    // Strips filled tile by tile in any order equal the full frame.
    uint16_t *back=malloc(SIZE*SIZE*2);assert(back);
    for(int i=0;i<SIZE*SIZE;i++)back[i]=sugarat_rotate_swap((uint16_t)(i*37));
    sugarat_rotate_params_t r={noise,SIZE,back,SIZE,0,180,180,95,8,true};
    sugarat_rotate_job_t full=sugarat_rotate_job(&r,2.1f,out,20);sugarat_rotate_rows(&full,0,SIZE,0,SIZE-1);
    int x0,y0,x1,y1;sugarat_rotate_bounds(&r,SIZE,&x0,&y0,&x1,&y1);
    int w=x1-x0+1;uint16_t *strip=malloc((size_t)w*12*2);assert(strip);
    for(int y=y0;y<=y1;y+=12){
        int rows=y1-y+1<12?y1-y+1:12;
        sugarat_rotate_job_t part=full;part.out=strip;part.out_x=x0;part.out_y=y;part.stride=w;
        sugarat_rotate_row_t g[12];for(int k=0;k<rows;k++)g[k]=sugarat_rotate_row(&part,y+k);
        for(int x=x0+(x1-x0)/SUGARAT_ROTATE_TILE*SUGARAT_ROTATE_TILE;x>=x0;x-=SUGARAT_ROTATE_TILE)
            sugarat_rotate_tile(&part,g,y,rows,x,x+SUGARAT_ROTATE_TILE-1<x1?x+SUGARAT_ROTATE_TILE-1:x1);
        for(int k=0;k<rows;k++)assert(!memcmp(strip+k*w,out+(y+k)*SIZE+x0,(size_t)w*2));
    }
    // Outside the bounds the frame is the dimmed backdrop.
    for(int y=0;y<SIZE;y++)for(int x=0;x<SIZE;x++)if(x<x0||x>x1||y<y0||y>y1)
        assert(out[y*SIZE+x]==sugarat_rotate_finish(sugarat_rotate_expand(sugarat_rotate_swap(back[y*SIZE+x])),20));
    free(strip);free(back);free(tex);free(out);free(first);free(noise);
    puts("PASS frame pacing, statistics, interior sampler, rotation direction, mask, hole, upscaled texture, strip tiles and backdrop");
    return 0;
}
