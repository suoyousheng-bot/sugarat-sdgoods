/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "cv_store.h"
#include "cv_crypto.h"
#include "cv_jpeg.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool cv_id_valid(const char *id)
{
    if(!id||strlen(id)!=CV_ID_LENGTH)return false;
    for(unsigned i=0;i<CV_ID_LENGTH;i++)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return false;
    return true;
}
static bool path_for(const cv_store_t *s,const char *id,const char *suffix,char *path,size_t capacity)
{return cv_id_valid(id)&&snprintf(path,capacity,"%s/%s%s",s->directory,id,suffix)>0;}
static int compare(const void *a,const void *b){return strcmp(((const cv_photo_t *)a)->id,((const cv_photo_t *)b)->id);}
static int file_stat(const char *path,struct stat *out)
{
#ifdef ESP_PLATFORM
    /* The platform appdata provider is FAT, which has no symbolic links. */
    return stat(path,out);
#else
    return lstat(path,out);
#endif
}
static cv_error_t refresh(cv_store_t *s)
{
    DIR *dir=opendir(s->directory);if(!dir)return CV_IO;
    cv_library_t next=s->library;next.count=0;struct dirent *entry;
    cv_error_t error=CV_OK;
    while((entry=readdir(dir))) {
        size_t length=strlen(entry->d_name);if(length!=CV_ID_LENGTH+4||strcmp(entry->d_name+CV_ID_LENGTH,".jpg"))continue;
        char id[CV_ID_LENGTH+1],path[256];memcpy(id,entry->d_name,CV_ID_LENGTH);id[CV_ID_LENGTH]=0;
        if(!path_for(s,id,".jpg",path,sizeof(path)))continue;
        struct stat st;
        if(file_stat(path,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||st.st_size>CV_MAX_JPEG){error=CV_CORRUPT;break;}
        if(next.count==CV_MAX_PHOTOS){error=CV_FULL;break;}
        cv_photo_t *photo=&next.photos[next.count++];strcpy(photo->id,id);photo->size=(uint32_t)st.st_size;
    }
    if(closedir(dir)!=0)error=CV_IO;
    if(error!=CV_OK)return error;
    qsort(next.photos,next.count,sizeof(next.photos[0]),compare);next.selected=0;
    for(unsigned i=0;i<next.count;i++)if(!strcmp(s->current,next.photos[i].id))next.selected=i;
    next.revision++;s->library=next;return CV_OK;
}
cv_error_t cv_store_open(cv_store_t *s,const char *directory,const char *current,cv_locale_t locale,cv_settings_write_t save,void *user)
{
    memset(s,0,sizeof(*s));s->save_settings=save;s->user=user;s->library.locale=locale==CV_ZH?CV_ZH:CV_EN;
    if(!directory||strlen(directory)>=sizeof(s->directory))return CV_IO;
    strcpy(s->directory,directory);if(cv_id_valid(current))strcpy(s->current,current);
    cv_error_t error=refresh(s);s->library.writable=error==CV_OK;s->library.error=error;
    if(error==CV_OK&&s->library.count&&!*s->current)strcpy(s->current,s->library.photos[0].id);
    else if(error==CV_OK&&*s->current) {
        bool found=false;for(unsigned i=0;i<s->library.count;i++)if(!strcmp(s->current,s->library.photos[i].id))found=true;
        if(!found){s->library.writable=false;s->library.error=CV_CORRUPT;return CV_CORRUPT;}
    }
    return error;
}
static cv_error_t read_photo(cv_store_t *s,const char *id,uint8_t **bytes,size_t *size,bool decode_check)
{
    *bytes=NULL;*size=0;char path[256];if(!path_for(s,id,".jpg",path,sizeof(path)))return CV_INVALID;
    struct stat st;if(file_stat(path,&st)||!S_ISREG(st.st_mode)||st.st_size<4||st.st_size>CV_MAX_JPEG)return CV_CORRUPT;
    FILE *file=fopen(path,"rb");if(!file)return CV_IO;
    uint8_t *data=malloc((size_t)st.st_size);if(!data){fclose(file);return CV_MEMORY;}
    bool ok=fread(data,1,(size_t)st.st_size,file)==(size_t)st.st_size&&!ferror(file);
    if(fclose(file)!=0)ok=false;
    cv_error_t error=!ok?CV_IO:decode_check?cv_jpeg(data,(size_t)st.st_size,NULL):CV_OK;
    uint8_t digest[32];char actual[CV_ID_LENGTH+1];
    if(error==CV_OK) {
        if(!cv_sha256(data,(size_t)st.st_size,digest))error=CV_CORRUPT;
        else {for(unsigned i=0;i<16;i++)snprintf(actual+i*2,3,"%02x",digest[i]);if(strcmp(actual,id))error=CV_CORRUPT;}
    }
    if(error!=CV_OK){free(data);return error;}
    *bytes=data;*size=(size_t)st.st_size;return CV_OK;
}
cv_error_t cv_store_read(cv_store_t *s,const char *id,uint8_t **bytes,size_t *size)
{return read_photo(s,id,bytes,size,true);}
cv_error_t cv_store_load(cv_store_t *s,const char *id,uint8_t **bytes,size_t *size)
{return read_photo(s,id,bytes,size,false);}
cv_error_t cv_store_add(cv_store_t *s,const uint8_t *bytes,size_t size,char id[CV_ID_LENGTH+1])
{
    if(!s->library.writable)return CV_IO;
    cv_error_t error=cv_jpeg(bytes,size,NULL);if(error!=CV_OK)return error;
    uint8_t hash[32];if(!cv_sha256(bytes,size,hash))return CV_CORRUPT;
    for(unsigned i=0;i<16;i++)snprintf(id+i*2,3,"%02x",hash[i]);
    for(unsigned i=0;i<s->library.count;i++)if(!strcmp(id,s->library.photos[i].id)) {
        uint8_t *old=NULL;size_t length=0;error=cv_store_read(s,id,&old,&length);
        bool same=error==CV_OK&&length==size&&!memcmp(old,bytes,size);free(old);return same?CV_OK:CV_CORRUPT;
    }
    if(s->library.count==CV_MAX_PHOTOS)return CV_FULL;
    char staging[256],final[256];path_for(s,id,".part",staging,sizeof(staging));path_for(s,id,".jpg",final,sizeof(final));
    /* A partial file has never been advertised; reclaim only this application's exact content ID. */
    if(unlink(staging)!=0&&errno!=ENOENT)return CV_IO;
    int fd=open(staging,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return CV_IO;
    size_t offset=0;bool ok=true;
    while(offset<size){ssize_t n=write(fd,bytes+offset,size-offset);if(n<=0){ok=false;break;}offset+=(size_t)n;}
    if(ok&&fsync(fd)!=0)ok=false;if(close(fd)!=0)ok=false;
    if(!ok){unlink(staging);return CV_IO;}
    if(rename(staging,final)!=0){unlink(staging);return CV_IO;}
    error=refresh(s);
    if(error!=CV_OK){s->library.writable=false;s->library.error=CV_UNKNOWN;return CV_UNKNOWN;}
    return CV_OK;
}
cv_error_t cv_store_select(cv_store_t *s,const char *id)
{
    uint8_t *bytes=NULL;size_t size=0;cv_error_t error=cv_store_read(s,id,&bytes,&size);free(bytes);
    if(error!=CV_OK)return error;
    if(!s->save_settings(s->user,id,s->library.locale))return CV_SETTINGS;
    strcpy(s->current,id);return refresh(s);
}
cv_error_t cv_store_delete(cv_store_t *s,const char *id)
{
    if(!s->library.writable)return CV_IO;
    if(!cv_id_valid(id))return CV_INVALID;
    if(!strcmp(id,s->current))return CV_BUSY;
    char path[256];path_for(s,id,".jpg",path,sizeof(path));
    bool present=false;for(unsigned i=0;i<s->library.count;i++)if(!strcmp(id,s->library.photos[i].id))present=true;
    if(!present)return CV_INVALID;
    if(unlink(path)!=0)return CV_IO;
    return refresh(s);
}
cv_error_t cv_store_locale(cv_store_t *s,cv_locale_t locale)
{
    if(locale!=CV_EN&&locale!=CV_ZH)return CV_INVALID;
    if(!s->save_settings(s->user,s->current,locale))return CV_SETTINGS;
    s->library.locale=locale;s->library.revision++;return CV_OK;
}
