/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "cv_jpeg.h"
#include "lvgl.h"
#include "src/extra/libs/sjpg/tjpgd.h"
#include <stdlib.h>
#include <string.h>
typedef struct { const uint8_t *bytes; size_t size, offset; uint16_t *pixels; } decoder_t;
static size_t input(JDEC *decoder, uint8_t *out, size_t count)
{
    decoder_t *d=decoder->device;
    if(count>d->size-d->offset)count=d->size-d->offset;
    if(out)memcpy(out,d->bytes+d->offset,count);
    d->offset+=count;return count;
}
static int output(JDEC *decoder, void *bitmap, JRECT *rect)
{
    decoder_t *d=decoder->device;
    if(rect->right>=CV_SIDE||rect->bottom>=CV_SIDE)return 0;
    const uint8_t *rgb=bitmap;
    for(unsigned y=rect->top;y<=rect->bottom;y++)for(unsigned x=rect->left;x<=rect->right;x++,rgb+=3)
        if(d->pixels)d->pixels[y*CV_SIDE+x]=lv_color_make(rgb[0],rgb[1],rgb[2]).full;
    return 1;
}
cv_error_t cv_jpeg(const uint8_t *bytes, size_t size, uint16_t *pixels)
{
    if(size<4||size>CV_MAX_JPEG||bytes[0]!=0xff||bytes[1]!=0xd8||bytes[size-2]!=0xff||bytes[size-1]!=0xd9)return CV_JPEG;
    void *work=malloc(8192);if(!work)return CV_MEMORY;
    decoder_t source={.bytes=bytes,.size=size,.pixels=pixels};JDEC decoder;
    JRESULT result=jd_prepare(&decoder,input,work,8192,&source);
    bool valid=result==JDR_OK&&decoder.width==CV_SIDE&&decoder.height==CV_SIDE;
    if(valid)valid=jd_decomp(&decoder,output,0)==JDR_OK;
    free(work);return valid?CV_OK:CV_JPEG;
}
