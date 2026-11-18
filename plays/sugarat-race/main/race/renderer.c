#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generated.h"

typedef struct { uint16_t *p; int alpha; } Canvas;
static uint16_t rgb(int r, int g, int b) { return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }
static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }
static uint16_t blend(uint16_t a, uint16_t b, int amount) {
    int r = (((a >> 11) & 31) * (255 - amount) + ((b >> 11) & 31) * amount) / 255;
    int g = (((a >> 5) & 63) * (255 - amount) + ((b >> 5) & 63) * amount) / 255;
    int bl = ((a & 31) * (255 - amount) + (b & 31) * amount) / 255;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}
static void pixel(Canvas *c, int x, int y, uint16_t v) {
    if ((unsigned)x >= 360 || (unsigned)y >= 360) return;
    int dx = x - 180, dy = y - 180;
    if (dx * dx + dy * dy > 179 * 179) return;
    c->p[y * 360 + x] = c->alpha == 255 ? v : blend(c->p[y * 360 + x], v, c->alpha);
}
/* Half width of the visible disc per row: dx*dx + dy*dy <= 179*179, the same test as pixel(). */
static int16_t disc_half[360];
static bool disc_ready;
static void disc_init(void) {
    if (disc_ready) return;
    disc_ready = true;
    for (int y = 0; y < 360; y++) {
        int r2 = 179 * 179 - (y - 180) * (y - 180), h = -1;
        if (r2 >= 0) { h = (int)sqrtf((float)r2); while (h * h > r2) h--; while ((h + 1) * (h + 1) <= r2) h++; }
        disc_half[y] = (int16_t)(h + 1);
    }
}
static void rect(Canvas *c, int x, int y, int w, int h, uint16_t v) {
    for (int yy = imax(y, 0); yy < imin(y + h, 360); yy++) {
        int half = disc_half[yy] - 1;
        int lo = imax(imax(x, 0), 180 - half), hi = imin(imin(x + w, 360), 181 + half);
        uint16_t *row = c->p + yy * 360;
        if (c->alpha == 255) for (int xx = lo; xx < hi; xx++) row[xx] = v;
        else for (int xx = lo; xx < hi; xx++) row[xx] = blend(row[xx], v, c->alpha);
    }
}
static void disk(Canvas *c, int x, int y, int r, uint16_t v) {
    for (int yy = -r; yy <= r; yy++) {
        int r2 = r * r - yy * yy, h = (int)sqrtf((float)r2);
        while (h * h > r2) h--;
        while ((h + 1) * (h + 1) <= r2) h++;
        rect(c, x - h, y + yy, 2 * h + 1, 1, v);
    }
}
static void pill(Canvas *c, int x, int y, int w, int h, uint16_t v) {
    int r = h / 2;
    rect(c, x + r, y, w - 2 * r, h, v);
    disk(c, x + r, y + r, r, v); disk(c, x + w - r - 1, y + r, r, v);
}
static void poly(Canvas *c, const int *p, int n, uint16_t color) {
    int ymin = 360, ymax = 0;
    for (int i = 0; i < n; i++) { ymin = imin(ymin, p[2*i+1]); ymax = imax(ymax, p[2*i+1]); }
    for (int y = imax(ymin, 0); y <= imin(ymax, 359); y++) {
        int xs[24], count = 0;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            int yi = p[2*i+1], yj = p[2*j+1];
            if ((yi > y) != (yj > y)) xs[count++] = p[2*i] + (y - yi) * (p[2*j] - p[2*i]) / (yj - yi);
        }
        for (int i = 1; i < count; i++) { int v = xs[i], j = i; while (j && xs[j-1] > v) { xs[j] = xs[j-1]; j--; } xs[j] = v; }
        for (int i = 0; i + 1 < count; i += 2) rect(c, xs[i], y, xs[i+1] - xs[i] + 1, 1, color);
    }
}
static const uint8_t digits[12][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}, {0,4,4,0,4,4,0}, {0,1,2,4,8,16,0}
};
static void number(Canvas *c, int x, int y, const char *str, int scale, uint16_t color) {
    for (; *str; str++, x += 6 * scale) {
        int k = *str >= '0' && *str <= '9' ? *str - '0' : *str == ':' ? 10 : *str == '/' ? 11 : -1;
        if (k < 0) continue;
        for (int yy = 0; yy < 7; yy++) for (int xx = 0; xx < 5; xx++)
            if (digits[k][yy] & (1 << (4 - xx))) rect(c, x + xx * scale, y + yy * scale, scale, scale, color);
    }
}
static void label(Canvas *c, int index, int center, int y, uint16_t color) {
    const Label *a = &labels[index];
    for (int yy = 0; yy < a->h; yy++) for (int xx = 0; xx < a->w; xx++) {
        int alpha = a->data[yy*a->w+xx]; int px = center - a->w/2 + xx, py = y + yy;
        if (alpha && (unsigned)px < 360 && (unsigned)py < 360) pixel(c, px, py, blend(c->p[py*360+px], color, alpha));
    }
}
static void point_at(int track, float distance, float *x, float *z) {
    float length = track_lengths[track];
    while (distance < 0) distance += length;
    distance = fmodf(distance, length);
    const RoadPoint *p = tracks[track];
    int lo = 0, hi = ROAD_POINTS - 1;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (p[mid].s <= distance) lo = mid; else hi = mid; }
    float t = (distance - p[lo].s) / (p[hi].s - p[lo].s);
    *x = p[lo].x + (p[hi].x - p[lo].x) * t;
    *z = p[lo].z + (p[hi].z - p[lo].z) * t;
}
typedef struct { int track; float distance, x, z, tx, tz, right_x, right_z; } Camera;
static int project(const Camera *cam, float d, float side, int *sx, int *sy, float *scale) {
    float x, z; point_at(cam->track, cam->distance + d, &x, &z);
    float nx, nz, px, pz; point_at(cam->track, cam->distance + d + 0.2f, &nx, &nz);
    point_at(cam->track, cam->distance + d - 0.2f, &px, &pz);
    float len = hypotf(nx-px,nz-pz), rx = (nz-pz)/len, rz = -(nx-px)/len;
    x += rx * side - cam->x; z += rz * side - cam->z;
    float depth = x * cam->tx + z * cam->tz;
    if (depth < 1.5f) return 0;
    *scale = 180.0f / depth;
    *sx = (int)lroundf(180 + (x * cam->right_x + z * cam->right_z) * *scale);
    *sy = (int)lroundf(142 + 3.1f * *scale);
    return *sx > -160 && *sx < 520 && *sy < 400;
}
/* Car paint: 0 player, 1 translucent nearby peer, 2..4 single-player rivals. */
static const uint8_t car_paint[5][4][3] = {
    {{66,13,31},{222,46,57},{255,122,112},{255,177,84}},
    {{5,67,94},{23,173,193},{105,238,241},{179,255,249}},
    {{92,58,6},{240,178,36},{255,226,120},{255,250,200}},
    {{10,70,40},{46,190,110},{140,240,170},{220,255,200}},
    {{52,24,90},{150,82,226},{205,160,255},{255,200,250}},
};
static void car(Canvas *c, int x, int base, float k, int style) {
    Canvas translucent = *c;
    int ghost = style == 1;
    if (ghost) { translucent.alpha = 180; c = &translucent; }
    const uint8_t (*paint)[3] = car_paint[style];
    uint16_t dark = rgb(paint[0][0],paint[0][1],paint[0][2]);
    uint16_t body = rgb(paint[1][0],paint[1][1],paint[1][2]);
    uint16_t bright = rgb(paint[2][0],paint[2][1],paint[2][2]);
    uint16_t lamp = rgb(paint[3][0],paint[3][1],paint[3][2]);
    uint16_t window = rgb(29,52,72), tire = rgb(7,16,27);
    #define BOX(a,b,w,h,col) rect(c,x+(int)lroundf((a)*k),base+(int)lroundf((b)*k),imax(1,(int)lroundf((w)*k)),imax(1,(int)lroundf((h)*k)),col)
    BOX(-34,-3,68,7,rgb(10,19,27));
    BOX(-34,-19,10,22,tire); BOX(24,-19,10,22,tire);
    int pts[] = {x-(int)(31*k),base, x-(int)(32*k),base-(int)(26*k), x-(int)(23*k),base-(int)(32*k),
        x-(int)(17*k),base-(int)(46*k), x+(int)(17*k),base-(int)(46*k), x+(int)(23*k),base-(int)(32*k),
        x+(int)(32*k),base-(int)(26*k), x+(int)(31*k),base};
    poly(c,pts,8,dark);
    BOX(-29,-27,58,21,body); BOX(-16,-43,32,12,bright); BOX(-14,-40,28,10,window);
    BOX(-25,-29,50,5,bright); BOX(-27,-12,54,7,dark);
    BOX(-25,-18,18,5,lamp); BOX(7,-18,18,5,lamp);
    BOX(-6,-11,12,5,rgb(242,219,170)); BOX(-33,-31,66,4,dark); BOX(-33,-32,66,2,bright);
    BOX(-24,-4,10,3,rgb(114,134,141)); BOX(14,-4,10,3,rgb(114,134,141));
    BOX(-27,-23,2,8,bright); BOX(25,-23,2,8,bright);
    #undef BOX
}
static void scenery(Canvas *c, int theme, int x, int base, float scale, int kind) {
    int height = (int)(scale * (theme == 1 ? 2.2f : 4.0f));
    if (height < 3 || height > 180) return;
    int w = imax(1,height/10);
    if (theme == 0 && kind % 4 != 0) {
        rect(c,x-w/2,base-height,w,height,rgb(120,79,53));
        rect(c,x,base-height+2,imax(1,w/2),height-2,rgb(201,143,75));
        int y=base-height, r=height/3;
        int leaves[]={x-r,y+6,x-r/2,y-r/3,x,y,x+r/2,y-r/3,x+r,y+6,x+r/2,y+2,x,y+4,x-r/2,y+2};
        poly(c,leaves,8,rgb(23,106,91));
        int leaf2[]={x-r+2,y+10,x-r/2,y+1,x,y+1,x+r/2,y+1,x+r-2,y+11,x+r/2,y+6,x,y+3,x-r/2,y+6};
        poly(c,leaf2,8,rgb(53,158,107));
    } else if (theme == 1) {
        if (kind%3==0) { int p[]={x-height/3,base,x-height/5,base-height,x+height/8,base-height-4,x+height/3,base}; poly(c,p,4,rgb(147,74,65)); rect(c,x-height/7,base-height,height/8,height,rgb(216,125,80)); }
        else { uint16_t a=rgb(43,101,89), b=rgb(83,140,101); rect(c,x-w,base-height,w*2,height,a); rect(c,x-w,base-height,w,height,b); rect(c,x-height/3,base-height*2/3,height/3,w*2,a); rect(c,x-height/3,base-height*4/5,w*2,height/5,a); rect(c,x+w,base-height/2,height/3,w*2,a); rect(c,x+height/3-w,base-height*3/4,w*2,height/4,a); }
    } else if (theme == 2) {
        uint16_t pole=rgb(37,47,81), light=kind%2?rgb(65,218,239):rgb(240,96,190);
        rect(c,x-w/2,base-height,w,height,pole); rect(c,x-w/2,base-height,height/5,w,pole);
        rect(c,x-w,base-height,w*3,w*2,light); rect(c,x-w,base-height+2,w*3,1,rgb(213,247,247));
        if(kind%3==0) { rect(c,x-height/5,base-height*2/3,height*2/5,height/4,rgb(26,49,85)); rect(c,x-height/5,base-height*2/3,height*2/5,2,light); }
    } else {
        int p[]={x-height/4,base,x-height/6,base-height/2,x+height/7,base-height*2/3,x+height/3,base};
        poly(c,p,4,rgb(137,152,144));
    }
}
/* Fixed pads sit on straight sections; lateral centres alternate so collecting them needs steering. */
static const float pad_fractions[3][RR_BOOST_PADS] = {{.08f,.33f,.66f,.80f},{.05f,.34f,.44f,.76f},{.05f,.24f,.58f,.89f}};
static const float pad_laterals[3][RR_BOOST_PADS] = {{-1.5f,1.5f,0.0f,-1.5f},{1.5f,-1.5f,0.0f,1.5f},{0.0f,-1.5f,1.5f,-1.5f}};
static void boost_pads(Canvas *c, const Camera *cam, const rr_state *s) {
    float length=track_lengths[cam->track], here=fmodf(s->distance_m,length);
    int shift=(s->elapsed_ms/90)%2;
    for(int i=0;i<RR_BOOST_PADS;i++){
        float rel=pad_fractions[cam->track][i]*length-here, lat=pad_laterals[cam->track][i], w=RR_PAD_HALF_WIDTH_M;
        if(rel< -RR_PAD_LENGTH_M)rel+=length;
        if(rel>110)continue;
        for(int j=3;j>=0;j--){
            float a=rel+j*1.75f;if(a<2.0f)continue;
            float sides[6][2]={{a,lat-w},{a+0.9f,lat},{a,lat+w},{a+0.8f,lat+w},{a+1.7f,lat},{a+0.8f,lat-w}};
            int pts[12],ok=1;float k;
            for(int v=0;v<6&&ok;v++)ok=project(cam,sides[v][0],sides[v][1],&pts[2*v],&pts[2*v+1],&k);
            if(ok)poly(c,pts,6,(j+shift)%2?rgb(255,170,40):rgb(255,236,130));
        }
    }
}
static void speed_streaks(Canvas *c, const rr_state *s) {
    Canvas faint=*c;faint.alpha=imin(200,(int)(s->boost_s*160));
    for(int i=0;i<14;i++){
        unsigned h=(unsigned)(i*2654435761u+(unsigned)(s->elapsed_ms/40)*40503u);
        float angle=(float)(h%6283)/1000.0f, inner=118+(float)((h>>13)%30), outer=inner+22+(float)((h>>7)%26);
        float dx=cosf(angle),dy=sinf(angle)*0.85f;
        for(float r=inner;r<outer;r+=1.0f)pixel(&faint,(int)(180+dx*r),(int)(186+dy*r),rgb(226,246,255));
    }
}
void rr_boost_pad(int map,int index,float *distance,float *lateral){
    *distance=pad_fractions[map][index]*track_lengths[map];*lateral=pad_laterals[map][index];
}
static void road(Canvas *c, const rr_state *s) {
    int theme=s->map;
    for(int y=0;y<180;y++){int half=disc_half[y]-1,lo=imax(0,180-half),hi=imin(360,181+half);if(hi>lo)memcpy(c->p+y*360+lo,backgrounds[theme]+y*360+lo,(size_t)(hi-lo)*2);}
    Camera cam={.track=theme,.distance=s->distance_m};
    float x1,z1; point_at(theme,s->distance_m,&cam.x,&cam.z); point_at(theme,s->distance_m+0.2f,&x1,&z1);
    float len=hypotf(x1-cam.x,z1-cam.z); cam.tx=(x1-cam.x)/len;cam.tz=(z1-cam.z)/len;
    cam.right_x=cam.tz;cam.right_z=-cam.tx; cam.x+=s->lateral_m*cam.right_x;cam.z+=s->lateral_m*cam.right_z;
    const uint16_t asphalt[3]={0x294a,0x414b,0x18e6};
    for(int y=143;y<360;y++) {
        float d=558.0f/(y-142), scale; int center, ignore;
        int stripe=((int)(s->distance_m+d)/5)%2;
        uint16_t ground=theme==0?(stripe?rgb(53,137,111):rgb(49,128,103)):theme==1?(stripe?rgb(192,120,81):rgb(183,110,75)):(stripe?rgb(17,27,54):rgb(19,29,60));
        rect(c,0,y,360,1,ground);
        if(theme==0) { rect(c,0,y,90+(y-142)/8,1,stripe?rgb(20,116,157):rgb(25,126,164)); }
        if(!project(&cam,d,0,&center,&ignore,&scale)) continue;
        int half=(int)(3.5f*scale), verge=imax(2,half/9), curb=imax(1,half/17);
        rect(c,center-half-verge,y,half*2+verge*2,1,theme==1?rgb(228,166,106):theme==2?rgb(35,50,76):rgb(190,184,139));
        uint16_t edge=theme==2?(stripe?rgb(52,202,221):rgb(161,54,130)):(stripe?rgb(236,224,197):rgb(201,66,63));
        rect(c,center-half-curb,y,half*2+curb*2,1,edge);
        uint16_t surf=blend(asphalt[theme],rgb(87,100,115),y%3==0?16:0);
        rect(c,center-half,y,half*2,1,surf);
        rect(c,center-half+imax(1,half/35),y,imax(1,half/40),1,rgb(220,218,196));
        rect(c,center+half-imax(2,half/20),y,imax(1,half/40),1,rgb(220,218,196));
        if(((int)(s->distance_m+d)/3)%2==0) for(int lane=-1;lane<=1;lane+=2) rect(c,center+lane*half/3,y,imax(1,half/35),1,theme==2?rgb(71,146,164):rgb(190,193,181));
        if(y>190) for(int k=0;k<3;k++) { unsigned hash=(unsigned)(y*313+k*113+(int)s->distance_m*11); int px=center-half+(int)(hash%((unsigned)imax(1,half*2))); pixel(c,px,y,blend(surf,rgb(151,158,151),26)); }
    }
    for(int n=14;n>=0;n--) {
        float d=5.0f+n*5.5f-fmodf(s->distance_m,5.5f); if(d<3)continue;
        for(int side=-1;side<=1;side+=2) { float k;int x,y;int id=((int)(s->distance_m/5.5f)+n)*7+side;
            if(project(&cam,d,side*(4.9f+(id%3)*0.7f),&x,&y,&k)) scenery(c,theme,x,y,k,abs(id)); }
    }
    if(s->live_hud) boost_pads(c,&cam,s);
    float k;int x,y;
    int order[RR_RIVALS],count=0;
    for(int i=0;i<s->rival_count&&i<RR_RIVALS;i++){
        int j=count++;float rel=s->rival_distance_m[i];
        while(j&&s->rival_distance_m[order[j-1]]<rel){order[j]=order[j-1];j--;}
        order[j]=i;
    }
    for(int pass=0;pass<2;pass++){
        for(int n=0;n<count;n++){
            int i=order[n];float rel=s->rival_distance_m[i]-s->distance_m+RR_CAR_AHEAD_M;
            if((rel>RR_CAR_AHEAD_M)!=(pass==0)||rel>140)continue;
            if(project(&cam,rel,s->rival_lateral_m[i],&x,&y,&k)) car(c,x,y,fminf(1.1f,k*1.35f/64.0f),2+i%3);
        }
        if(pass==0){
            if(s->opponent_visible && project(&cam,s->opponent_distance_m-s->distance_m,0.65f,&x,&y,&k)) car(c,x,y,fminf(0.8f,k*1.35f/64.0f),1);
            car(c,180,311,1.04f,0);
        }
    }
    if(s->live_hud&&s->boost_s>0) speed_streaks(c,s);
    if(s->hide_hud)return;
    pill(c,99,29,162,53,rgb(7,22,40));
    label(c,theme,180,34,rgb(227,239,230));
    char clock[16];snprintf(clock,sizeof clock,"%02d:%02d",s->elapsed_ms/60000,s->elapsed_ms/1000%60);
    number(c,137,57,clock,3,rgb(255,219,111));
    char place[8];snprintf(place,sizeof place,"%d/%d",s->live_hud?s->rank:1,s->live_hud?s->players:2);
    pill(c,237,94,62,24,rgb(8,32,48));number(c,250,100,place,2,rgb(197,242,238));
    pill(c,115,318,130,27,rgb(6,23,38));
    char speed[8];snprintf(speed,sizeof speed,"%03d",s->speed_kmh);number(c,131,322,speed,3,rgb(255,214,92));
    label(c,6,219,327,rgb(156,204,210));
    int progress=s->live_hud?(int)(80*s->distance_m/(track_lengths[theme]*imax(1,s->laps))):34;
    rect(c,140,350,80,3,rgb(37,66,79));rect(c,140,350,imin(80,imax(0,progress)),3,rgb(68,216,211));
    if(s->live_hud&&s->laps>0){
        char lap[24];snprintf(lap,sizeof lap,"%d/%d",s->lap,s->laps);
        bool last=s->lap==s->laps;
        pill(c,61,94,62,24,s->lap_flash_ms>0?rgb(255,170,40):rgb(8,32,48));
        number(c,74,100,lap,2,s->lap_flash_ms>0?rgb(20,24,32):last?rgb(255,140,90):rgb(255,214,92));
        if(s->lap_flash_ms>0){
            pill(c,112,128,136,58,rgb(7,22,40));
            number(c,180-(int)strlen(lap)*15+3,140,lap,5,last?rgb(255,140,90):rgb(255,219,111));
        }
    }
}
static int open_at(int map,int gx,int gy) { return gx>=0&&gy>=0&&gx<17&&gy<17&&mazes[map][gy*17+gx]; }
static void marble(Canvas *c,int x,int y,int ghost) {
    disk(c,x+1,y+2,6,rgb(7,15,25));
    disk(c,x,y,6,ghost?rgb(16,128,153):rgb(176,175,157));
    disk(c,x-1,y-1,5,ghost?rgb(42,202,219):rgb(224,219,198));
    disk(c,x-2,y-2,3,ghost?rgb(121,246,240):rgb(255,250,229));
    pixel(c,x-3,y-3,rgb(255,255,247));
}
static uint16_t *maze_cache;
static int maze_cached= -1;
void rr_set_maze_cache(uint16_t *buffer){maze_cache=buffer;maze_cached= -1;}
static void maze_static(Canvas *c,int m) {
    uint16_t floor=maze_palettes[m][0], wall=maze_palettes[m][1], top=maze_palettes[m][2], face=maze_palettes[m][3];
    rect(c,0,0,360,360,rgb(8,21,37));disk(c,180,199,140,rgb(36,60,69));disk(c,180,199,137,wall);
    for(int gy=0;gy<17;gy++)for(int gx=0;gx<17;gx++) {
        int x=44+gx*16,y=63+gy*16;
        for(int yy=y;yy<y+16;yy++)for(int xx=x;xx<x+16;xx++){
            int dx=xx-180,dy=yy-199;if(dx*dx+dy*dy>135*135)continue;
            if(open_at(m,gx,gy)) { pixel(c,xx,yy,floor);if((xx*13+yy*7)%53==0)pixel(c,xx,yy,blend(floor,rgb(160,179,180),18)); }
            else {uint16_t color=wall; if(yy%16<2)color=top; if(xx%16==0)color=blend(wall,face,80);if((xx*7+yy*13)%67==0)color=blend(wall,top,40);pixel(c,xx,yy,color);}
        }
    }
    for(int gy=0;gy<17;gy++)for(int gx=0;gx<17;gx++) {
        int x=44+gx*16,y=63+gy*16;
        if(!open_at(m,gx,gy)&&open_at(m,gx,gy+1))for(int yy=0;yy<3;yy++)for(int xx=0;xx<16;xx++) {
            int px=x+xx,py=y+16+yy,dx=px-180,dy=py-199;
            if(dx*dx+dy*dy<135*135)pixel(c,px,py,yy==0?top:face);
        }
    }
}
static void maze(Canvas *c,const rr_state *s) {
    int m=s->map;
    if(maze_cache&&maze_cached==m)memcpy(c->p,maze_cache,360*360*sizeof *c->p);
    else{maze_static(c,m);if(maze_cache){memcpy(maze_cache,c->p,360*360*sizeof *c->p);maze_cached=m;}}
    for(int k=0;k<3;k++) {
        int gx=maze_checks[m][k][0],gy=maze_checks[m][k][1],x=52+16*gx,y=71+16*gy;
        bool collected=s->live_hud?(s->checkpoint_mask&(1U<<k))!=0:k<s->checkpoints;
        uint16_t col=collected?rgb(75,213,179):rgb(248,191,66);
        disk(c,x,y,6,rgb(111,81,38));disk(c,x,y,5,col);disk(c,x-1,y-1,3,rgb(255,230,135));
    }
    int ex=52+16*maze_ends[m][2],ey=71+16*maze_ends[m][3];
    rect(c,ex-7,ey-7,15,15,rgb(252,194,78));
    for(int j=0;j<4;j++)for(int i=0;i<4;i++)rect(c,ex-6+i*3,ey-6+j*3,3,3,(i+j)%2?rgb(236,228,203):rgb(24,35,44));
    if(s->opponent_visible)marble(c,s->opponent_x,s->opponent_y,1);
    marble(c,s->marble_x,s->marble_y,0);
    if(s->hide_hud)return;
    pill(c,88,25,184,48,rgb(5,19,35));label(c,3+m,180,30,rgb(220,238,230));
    char clock[16];snprintf(clock,sizeof clock,"00:%02d",s->elapsed_ms/1000%60);number(c,115,52,clock,2,rgb(255,219,110));
    char score[8];snprintf(score,sizeof score,"%d/3",s->checkpoints);number(c,212,52,score,2,rgb(88,226,209));
    pill(c,133,341,94,13,rgb(6,20,36));label(c,7,180,341,rgb(136,178,188));
}
rr_state rr_reference_state(int mode,int map) {
    rr_state s={0};s.mode=mode;s.map=map;s.elapsed_ms=42000;s.speed_kmh=126;s.opponent_visible=true;
    s.distance_m=track_lengths[map]*preview_fractions[map];s.opponent_distance_m=s.distance_m+14;
    if(mode==1){s.elapsed_ms=36000;s.checkpoints=1;s.marble_x=maze_ball_positions[map][0];s.marble_y=maze_ball_positions[map][1];s.opponent_x=maze_ball_positions[map][2];s.opponent_y=maze_ball_positions[map][3];}
    return s;
}
void rr_render(uint16_t *frame,const rr_state *s) {
    if(!frame||!s||s->map<0||s->map>=3||(s->mode!=0&&s->mode!=1))return;
    disc_init();Canvas c={frame,255};
    /* A cached maze overwrites every pixel; other scenes rely on a cleared frame outside the disc. */
    if(!(s->mode==1&&maze_cache&&maze_cached==s->map))memset(frame,0,360*360*sizeof *frame);
    if(s->mode==0)road(&c,s);else maze(&c,s);
}
float rr_track_length(int map){return track_lengths[map];}
float rr_track_curvature(int map,float distance){
    float ax,az,bx,bz,cx,cz;
    point_at(map,distance-1,&ax,&az);point_at(map,distance,&bx,&bz);point_at(map,distance+1,&cx,&cz);
    float u=atan2f(bx-ax,bz-az),v=atan2f(cx-bx,cz-bz),delta=v-u;
    while(delta>3.14159265f)delta-=6.2831853f;
    while(delta< -3.14159265f)delta+=6.2831853f;
    return delta;
}
bool rr_maze_open(int map,int x,int y){
    if(x<44||y<63||x>=316||y>=335)return false;
    int dx=x-180,dy=y-199;if(dx*dx+dy*dy>135*135)return false;
    return open_at(map,(x-44)/16,(y-63)/16);
}
void rr_maze_point(int map,int index,int *x,int *y){
    int gx,gy;
    if(index<2){gx=maze_ends[map][2*index];gy=maze_ends[map][2*index+1];}
    else{gx=maze_checks[map][index-2][0];gy=maze_checks[map][index-2][1];}
    *x=52+16*gx;*y=71+16*gy;
}
