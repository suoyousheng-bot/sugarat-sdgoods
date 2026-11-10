/* Rotating circular texture renderer for byte-swapped RGB565 (LV_COLOR_16_SWAP) screens. Pure C:
 * no ESP-IDF dependency, usable in host tests. The texture is a square of native-order RGB565; the
 * backdrop and the output hold byte-swapped pixels. Bilinear sampling with 1/32 weights, an
 * anti-aliased outer edge and spindle hole, and an optional darkened rim band. */
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Width of the darkened band inside the outer edge when rim_shade is set. */
#define SUGARAT_ROTATE_RIM 2.5f

static inline uint16_t sugarat_rotate_swap(uint16_t c){return (uint16_t)((c<<8)|(c>>8));}
/* RGB565 channels are kept apart in 0x07e0f81f lanes so one multiply scales all three. */
static inline uint32_t sugarat_rotate_expand(uint16_t c){return (c|((uint32_t)c<<16))&0x07e0f81f;}
static inline uint16_t sugarat_rotate_pack(uint32_t lanes){return (uint16_t)(lanes|(lanes>>16));}
/** Interpolates expanded lanes; w is 0..32 toward b. */
static inline uint32_t sugarat_rotate_lerp(uint32_t a,uint32_t b,unsigned w){return ((a*(32-w)+b*w)>>5)&0x07e0f81f;}

/** What to draw. Screen pixel (x,y) has its centre at (x+0.5,y+0.5). */
typedef struct {
    const uint16_t *texture;   /**< texture_size x texture_size native RGB565, row stride texture_size. */
    int texture_size;          /**< At least 2; the texture's inscribed circle maps onto the disc. */
    const uint16_t *backdrop;  /**< Byte-swapped screen pixels at backdrop[y*backdrop_stride+x]; NULL uses backdrop_color. */
    int backdrop_stride;
    uint16_t backdrop_color;   /**< Native RGB565 behind the disc when backdrop is NULL. */
    float center_x,center_y;   /**< Disc centre in screen coordinates. */
    float radius;              /**< Visible disc radius in screen pixels. */
    float hole;                /**< Spindle hole radius; 0 for none. The hole shows the backdrop. */
    bool rim_shade;            /**< Darken the outer SUGARAT_ROTATE_RIM pixels to 26/32. */
} sugarat_rotate_params_t;

/** One frame. Output pixel (x,y) is out[(y-out_y)*stride+(x-out_x)]. dim 0..32 scales every output
 *  pixel, backdrop included. Disjoint pixel ranges of one job may be rendered concurrently. */
typedef struct {
    sugarat_rotate_params_t p;
    uint16_t *out;int out_x,out_y,stride;
    unsigned dim;
    int32_t c,s;               /* 16.16 texture step per screen pixel, rotation and scale combined */
    float core;                /* pixels nearer than this need no edge, rim or clamping work */
} sugarat_rotate_job_t;

/** Builds a job for a clockwise rotation by angle radians, writing a full-screen-stride out buffer
 *  (out_x=out_y=0, stride=backdrop_stride when set, else ceil(2*center_x)). Adjust the out fields
 *  for a strip. */
static inline sugarat_rotate_job_t sugarat_rotate_job(const sugarat_rotate_params_t *p,float angle,uint16_t *out,unsigned dim)
{
    float scale=(p->texture_size*0.5f)/p->radius;
    sugarat_rotate_job_t j={*p,out,0,0,p->backdrop?p->backdrop_stride:(int)ceilf(2*p->center_x),dim,
                            (int32_t)lroundf(cosf(angle)*65536*scale),(int32_t)lroundf(sinf(angle)*65536*scale),0};
    // Interior samples stay at least one texel inside the texture, so they need no clamping.
    float inside=p->radius-2*p->radius/p->texture_size,edge=p->radius-(p->rim_shade?SUGARAT_ROTATE_RIM:1.0f);
    j.core=inside<edge?inside:edge;
    return j;
}

static inline uint32_t sugarat_rotate_finish_lanes(uint32_t lanes,unsigned dim)
{
    if(dim<32)lanes=((lanes*dim)>>5)&0x07e0f81f;
    return lanes;
}
static inline uint16_t sugarat_rotate_finish(uint32_t lanes,unsigned dim)
{ return sugarat_rotate_swap(sugarat_rotate_pack(sugarat_rotate_finish_lanes(lanes,dim))); }

/** Bilinear sample at 16.16 texture coordinates, clamped to the texture. */
static inline uint32_t sugarat_rotate_sample(const uint16_t *src,int size,int32_t fx,int32_t fy)
{
    const int32_t limit=(size-1)<<16;
    if(fx<0)fx=0;
    if(fy<0)fy=0;
    if(fx>limit)fx=limit;
    if(fy>limit)fy=limit;
    int x=fx>>16,y=fy>>16;
    const uint16_t *q=src+y*size+x;
    int right=x<size-1,down=y<size-1?size:0;
    unsigned wx=(fx>>11)&31,wy=(fy>>11)&31;
    uint32_t top=sugarat_rotate_lerp(sugarat_rotate_expand(q[0]),sugarat_rotate_expand(q[right]),wx);
    uint32_t bottom=sugarat_rotate_lerp(sugarat_rotate_expand(q[down]),sugarat_rotate_expand(q[down+right]),wx);
    return sugarat_rotate_lerp(top,bottom,wy);
}

/** Equal to sugarat_rotate_sample where both coordinates lie in [0,size-2]; each lerp uses
 *  32a+(b-a)w, which is exact in 32 bits for expanded lanes. */
static inline uint32_t sugarat_rotate_sample_core(const uint16_t *src,int size,int32_t u,int32_t v)
{
    const uint16_t *q=src+(v>>16)*size+(u>>16);
    unsigned wx=(u>>11)&31,wy=(v>>11)&31;
    uint32_t a=sugarat_rotate_expand(q[0]),c=sugarat_rotate_expand(q[size]);
    uint32_t top=(((a<<5)+(sugarat_rotate_expand(q[1])-a)*wx)>>5)&0x07e0f81f;
    uint32_t bottom=(((c<<5)+(sugarat_rotate_expand(q[size+1])-c)*wx)>>5)&0x07e0f81f;
    return (((top<<5)+(bottom-top)*wy)>>5)&0x07e0f81f;
}

/** Renders n interior pixels from d onward; (u,v) is the first pixel's texture position. Kept out of
 *  line so the loop state stays in registers. */
static __attribute__((noinline,unused)) void sugarat_rotate_core_run(const sugarat_rotate_job_t *j,uint16_t *d,int n,int32_t u,int32_t v)
{
    const uint16_t *src=j->p.texture;
    const int size=j->p.texture_size;
    const int32_t du=j->c,dv=-j->s;
    const unsigned dim=j->dim;
    if(dim>=32)for(;n>0;n--,d++,u+=du,v+=dv){uint32_t l=sugarat_rotate_sample_core(src,size,u,v);*d=sugarat_rotate_swap(sugarat_rotate_pack(l));}
    else for(;n>0;n--,d++,u+=du,v+=dv)*d=sugarat_rotate_finish(sugarat_rotate_sample_core(src,size,u,v),dim);
}

/** Geometry shared by every column range of one row. Columns ca..cb outside ha..hb are interior. */
typedef struct { int32_t u0,v0;float dy2;int xa,xb,ca,cb,ha,hb;bool disc; } sugarat_rotate_row_t;

static inline sugarat_rotate_row_t sugarat_rotate_row(const sugarat_rotate_job_t *j,int y)
{
    const float R=j->p.radius,H=j->p.hole,core=j->core,hole_edge=H>0?H+1.0f:0,cx=j->p.center_x;
    const int32_t half=(int32_t)((0.5f-cx)*65536),origin=(int32_t)((j->p.texture_size*0.5f-0.5f)*65536);
    sugarat_rotate_row_t g={0};
    float dy=y+0.5f-j->p.center_y;g.dy2=dy*dy;
    g.disc=g.dy2<(R+0.5f)*(R+0.5f);
    if(!g.disc)return g;
    // Texture coordinate of the x=0 pixel centre; each step right adds (c,-s).
    int32_t fdy=(int32_t)(dy*65536);
    g.u0=origin+(int32_t)(((int64_t)j->c*half+(int64_t)j->s*fdy)>>16);
    g.v0=origin+(int32_t)((-(int64_t)j->s*half+(int64_t)j->c*fdy)>>16);
    float outer=sqrtf((R+0.5f)*(R+0.5f)-g.dy2);
    g.xa=(int)floorf(cx-outer-0.5f);g.xb=(int)ceilf(cx+outer-0.5f);
    g.ca=g.xb+1;g.cb=g.xb;g.ha=g.xb+1;g.hb=g.xb;
    if(core>0 && g.dy2<core*core){float k=sqrtf(core*core-g.dy2);g.ca=(int)ceilf(cx-k-0.5f);g.cb=(int)floorf(cx+k-0.5f);}
    if(hole_edge>0 && g.dy2<hole_edge*hole_edge){float k=sqrtf(hole_edge*hole_edge-g.dy2);g.ha=(int)floorf(cx-k-0.5f);g.hb=(int)ceilf(cx+k-0.5f);}
    return g;
}

/** Renders pixels lo..hi (inclusive) of row y from its geometry; pixels off the disc get the backdrop. */
static inline void sugarat_rotate_span(const sugarat_rotate_job_t *j,const sugarat_rotate_row_t *g,int y,int lo,int hi)
{
    const float R=j->p.radius,H=j->p.hole;
    uint16_t *row=j->out+(y-j->out_y)*j->stride-j->out_x;
    const uint16_t *back=j->p.backdrop?j->p.backdrop+y*j->p.backdrop_stride:NULL;
    const unsigned dim=j->dim;
    const uint32_t fill=sugarat_rotate_expand(j->p.backdrop_color);
    if(!back){uint16_t c=sugarat_rotate_finish(fill,dim);for(int x=lo;x<=hi;x++)row[x]=c;}
    else if(dim>=32)memcpy(row+lo,back+lo,(hi-lo+1)*2);
    else for(int x=lo;x<=hi;x++)row[x]=sugarat_rotate_finish(sugarat_rotate_expand(sugarat_rotate_swap(back[x])),dim);
    if(!g->disc)return;
    const uint16_t *src=j->p.texture;
    const int size=j->p.texture_size;
    const int32_t du=j->c,dv=-j->s;
    const int ca=g->ca,cb=g->cb,ha=g->ha,hb=g->hb;
    int xa=g->xa<lo?lo:g->xa,xb=g->xb>hi?hi:g->xb;
    for(int x=xa;x<=xb;){
        bool fast=x>=ca && x<=cb && (x<ha || x>hb);
        int end;
        if(fast)end=x<ha?(cb<ha-1?cb:ha-1):cb;
        else if(x<ca)end=ca-1<xb?ca-1:xb;
        else if(x>=ha && x<=hb)end=hb<xb?hb:xb;
        else end=xb;
        if(end>xb)end=xb;
        if(fast){
            sugarat_rotate_core_run(j,row+x,end-x+1,g->u0+du*x,g->v0+dv*x);
            x=end+1;
            continue;
        }
        for(;x<=end;x++){
            float dx=x+0.5f-j->p.center_x,r=sqrtf(dx*dx+g->dy2);
            float a=R+0.5f-r;
            if(H>0){float b=r-(H-0.5f);if(b<a)a=b;}
            if(a<=0)continue;
            uint32_t c=sugarat_rotate_sample(src,size,g->u0+du*x,g->v0+dv*x);
            if(j->p.rim_shade && r>R-SUGARAT_ROTATE_RIM)c=((c*26)>>5)&0x07e0f81f;
            if(a<1){
                uint32_t m=back?sugarat_rotate_expand(sugarat_rotate_swap(back[x])):fill;
                c=sugarat_rotate_lerp(m,c,(unsigned)(a*32));
            }
            row[x]=sugarat_rotate_finish(c,dim);
        }
    }
}

/** Renders columns lo..hi of rows y0..y0+n-1 from their geometry rows[0..n-1]. */
static inline void sugarat_rotate_tile(const sugarat_rotate_job_t *j,const sugarat_rotate_row_t *rows,int y0,int n,int lo,int hi)
{ for(int i=0;i<n;i++)sugarat_rotate_span(j,&rows[i],y0+i,lo,hi); }

/** Tile edge used by sugarat_rotate_rows() and by the parallel renderer. */
#define SUGARAT_ROTATE_TILE 40

/** Renders rows y0..y1-1 and columns lo..hi (inclusive) on the calling core. */
static inline void sugarat_rotate_rows(const sugarat_rotate_job_t *j,int y0,int y1,int lo,int hi)
{
    sugarat_rotate_row_t rows[SUGARAT_ROTATE_TILE];
    for(int ty=y0;ty<y1;ty+=SUGARAT_ROTATE_TILE){
        int n=y1-ty<SUGARAT_ROTATE_TILE?y1-ty:SUGARAT_ROTATE_TILE;
        for(int i=0;i<n;i++)rows[i]=sugarat_rotate_row(j,ty+i);
        for(int tx=lo;tx<=hi;tx+=SUGARAT_ROTATE_TILE)
            sugarat_rotate_tile(j,rows,ty,n,tx,tx+SUGARAT_ROTATE_TILE-1<hi?tx+SUGARAT_ROTATE_TILE-1:hi);
    }
}

/** Inclusive square of screen pixels that change when the disc rotates, clipped to [0,limit-1]. */
static inline void sugarat_rotate_bounds(const sugarat_rotate_params_t *p,int limit,int *x0,int *y0,int *x1,int *y1)
{
    float r=p->radius+1.5f;
    *x0=(int)floorf(p->center_x-r);*x1=(int)ceilf(p->center_x+r)-1;
    *y0=(int)floorf(p->center_y-r);*y1=(int)ceilf(p->center_y+r)-1;
    if(*x0<0)*x0=0;
    if(*y0<0)*y0=0;
    if(*x1>limit-1)*x1=limit-1;
    if(*y1>limit-1)*y1=limit-1;
}

#ifdef __cplusplus
}
#endif
