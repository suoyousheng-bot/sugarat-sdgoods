/* SPDX-License-Identifier: Apache-2.0 */
#include "game.h"
#include <math.h>
#include <string.h>
/* Marble: a solid ball rolls with 5/7 of the slope acceleration; one g of slope is 1100 px/s^2 on screen. */
#define MARBLE_ROLL_ACCEL (1100.0f*5.0f/7.0f)
#define MARBLE_RADIUS 5.4f
#define MARBLE_MAX_SPEED 380.0f
#define MARBLE_ROLLING_DECEL 18.0f
#define MARBLE_DRAG 0.35f
#define MARBLE_RESTITUTION 0.42f
#define MARBLE_BOUNCE_MIN 30.0f
/* Road: rivals are cars about 1.3 m wide and 2.6 m long. */
#define CAR_WIDTH_M 1.25f
#define CAR_LENGTH_M 2.6f
#define CRUISE_MS 36.0f
#define BOOST_MS 50.0f
#define VERGE_MS 11.5f
#define BOOST_SECONDS 1.8f
/* Rival pace: a clean race without boosts finishes third or fourth, so winning needs the pads. */
#define RIVAL_MS 41.0f
#define RIVAL_LEAD_M 70.0f
static float clamp(float v,float a,float b){return fminf(b,fmaxf(a,v));}
static float circle_cos[16],circle_sin[16];
bool race_game_ball_free(int map,float x,float y,float radius){
    if(!rr_maze_open(map,(int)x,(int)y))return false;
    if(circle_cos[0]==0)for(int i=0;i<16;i++){circle_cos[i]=cosf(i*.3926990817f);circle_sin[i]=sinf(i*.3926990817f);}
    for(int i=0;i<16;i++)
        if(!rr_maze_open(map,(int)floorf(x+circle_cos[i]*radius),(int)floorf(y+circle_sin[i]*radius)))return false;
    return true;
}
void race_game_begin(race_game *g,int mode,int map,bool rivals){
    memset(g,0,sizeof *g);g->phase=RACE_COUNTDOWN;g->countdown=3;
    g->frame.mode=mode;g->frame.map=map;g->frame.live_hud=true;g->frame.players=1;g->frame.rank=1;
    int x,y;rr_maze_point(map,0,&x,&y);g->x=x;g->y=y;g->frame.marble_x=x;g->frame.marble_y=y;
    for(int i=0;i<RR_BOOST_PADS;i++)g->pad_lap[i]= -1;
    if(mode!=0)return;
    g->frame.lap=1;g->frame.laps=RACE_LAPS;
    if(!rivals)return;
    /* The player starts at the back of a staggered grid and has to pass every rival. */
    static const race_rival grid[RR_RIVALS]={{7,-1.6f,0,.955f,-1.6f,0},{14,1.6f,0,.975f,1.6f,2.1f},{21,0,0,.99f,0,4.2f}};
    g->rival_count=RR_RIVALS;memcpy(g->rivals,grid,sizeof grid);
    g->frame.rival_count=RR_RIVALS;g->frame.players=1+RR_RIVALS;g->frame.rank=1+RR_RIVALS;
}
static void marble(race_game *g,float dt,float ix,float iy){
    float ax=ix*MARBLE_ROLL_ACCEL,ay=iy*MARBLE_ROLL_ACCEL;
    g->vx+=ax*dt;g->vy+=ay*dt;
    float speed=hypotf(g->vx,g->vy);
    if(speed>0){
        float drop=(MARBLE_ROLLING_DECEL+MARBLE_DRAG*speed)*dt;
        if(drop>=speed&&hypotf(ax,ay)<MARBLE_ROLLING_DECEL){g->vx=g->vy=0;}
        else{float k=fminf(MARBLE_MAX_SPEED,fmaxf(0,speed-drop))/speed;g->vx*=k;g->vy*=k;}
    }
    int map=g->frame.map;
    float nx=g->x+g->vx*dt;
    if(race_game_ball_free(map,nx,g->y,MARBLE_RADIUS))g->x=nx;
    else{float hit=fabsf(g->vx);if(hit>g->impact)g->impact=hit;if(hit>MARBLE_BOUNCE_MIN)g->events|=RACE_EVENT_WALL;g->vx=hit>MARBLE_BOUNCE_MIN?-g->vx*MARBLE_RESTITUTION:0;g->vy*=.97f;}
    float ny=g->y+g->vy*dt;
    if(race_game_ball_free(map,g->x,ny,MARBLE_RADIUS))g->y=ny;
    else{float hit=fabsf(g->vy);if(hit>g->impact)g->impact=hit;if(hit>MARBLE_BOUNCE_MIN)g->events|=RACE_EVENT_WALL;g->vy=hit>MARBLE_BOUNCE_MIN?-g->vy*MARBLE_RESTITUTION:0;g->vx*=.97f;}
    g->frame.marble_x=(int)lroundf(g->x);g->frame.marble_y=(int)lroundf(g->y);
    for(int i=0;i<3;i++){
        int x,y;rr_maze_point(map,i+2,&x,&y);
        if(!(g->checkpoint_mask&1U<<i)&&hypotf(g->x-x,g->y-y)<9){g->checkpoint_mask|=1U<<i;g->events|=RACE_EVENT_CHECKPOINT;}
    }
    g->frame.checkpoints=(int)((g->checkpoint_mask&1)+((g->checkpoint_mask>>1)&1)+((g->checkpoint_mask>>2)&1));
    g->frame.checkpoint_mask=g->checkpoint_mask;
    int ex,ey;rr_maze_point(map,1,&ex,&ey);
    if(g->checkpoint_mask==7&&hypotf(g->x-ex,g->y-ey)<9)g->phase=RACE_FINISHED;
}
static void rivals(race_game *g,float dt){
    int map=g->frame.map;
    for(int i=0;i<g->rival_count;i++){
        race_rival *r=&g->rivals[i];
        float target=RIVAL_MS*r->skill*(1-clamp(fabsf(rr_track_curvature(map,r->distance+RR_CAR_AHEAD_M))*2.2f,0,.25f));
        /* Rubber band: a rival far ahead eases off and one far behind pushes, so the field stays close. */
        float gap=r->distance-g->frame.distance_m;
        if(gap>RIVAL_LEAD_M)target*=.92f;else if(gap< -30)target*=1.12f;
        r->speed+=clamp(target-r->speed,-14*dt,8*dt);
        float lane=clamp(r->lane+.9f*sinf(r->distance*.021f+r->phase),-2.6f,2.6f),rate=1.8f;
        /* Pull out to pass a player ahead in the same lane instead of running into the back of the car. */
        float ahead=g->frame.distance_m-r->distance;
        if(ahead>0&&ahead<14&&fabsf(g->lateral-lane)<1.6f){lane=clamp(g->lateral>0?g->lateral-1.8f:g->lateral+1.8f,-2.8f,2.8f);rate=3.0f;}
        r->lateral+=clamp(lane-r->lateral,-rate*dt,rate*dt);
        r->distance+=r->speed*dt;
    }
}
static void contacts(race_game *g){
    for(int i=0;i<g->rival_count;i++){
        race_rival *r=&g->rivals[i];
        float dz=r->distance-g->frame.distance_m,dl=r->lateral-g->lateral;
        if(fabsf(dz)>=CAR_LENGTH_M||fabsf(dl)>=CAR_WIDTH_M)continue;
        float closing;
        if(dz>=0){closing=g->speed-r->speed;g->frame.distance_m=r->distance-CAR_LENGTH_M;if(closing>0){g->speed-=closing*.7f;r->speed+=closing*.4f;}}
        else{closing=r->speed-g->speed;r->distance=g->frame.distance_m-CAR_LENGTH_M;if(closing>0){r->speed-=closing*.7f;g->speed+=closing*.4f;}}
        float push=dl>=0?-.12f:.12f;g->lateral=clamp(g->lateral+push,-4.5f,4.5f);r->lateral=clamp(r->lateral-push,-2.8f,2.8f);
        if(g->bump_cooldown<=0){g->events|=RACE_EVENT_BUMP;g->impact=fmaxf(g->impact,fabsf(closing));g->bump_cooldown=.35f;}
    }
    if(g->frame.distance_m<0)g->frame.distance_m=0;
}
static void pads(race_game *g){
    float length=rr_track_length(g->frame.map),car=g->frame.distance_m+RR_CAR_AHEAD_M;
    int lap=(int)(car/length);float here=car-lap*length;
    for(int i=0;i<RR_BOOST_PADS;i++){
        float start,lateral;rr_boost_pad(g->frame.map,i,&start,&lateral);
        if(g->pad_lap[i]==lap||here<start||here>start+RR_PAD_LENGTH_M||fabsf(g->lateral-lateral)>RR_PAD_HALF_WIDTH_M+.3f)continue;
        g->pad_lap[i]=lap;g->boost=BOOST_SECONDS;g->speed=fmaxf(g->speed,CRUISE_MS)+6;g->events|=RACE_EVENT_BOOST;
    }
}
static int road_rank(const race_game *g){
    int rank=1;for(int i=0;i<g->rival_count;i++)if(g->rivals[i].distance>g->frame.distance_m)rank++;
    return rank;
}
static void road(race_game *g,float dt,float ix){
    int map=g->frame.map;float length=rr_track_length(map),total=length*RACE_LAPS;
    bool verge=fabsf(g->lateral)>3.0f;
    if(g->boost>0)g->boost=fmaxf(0,g->boost-dt);
    if(g->bump_cooldown>0)g->bump_cooldown-=dt;
    if(g->lap_flash>0)g->lap_flash=fmaxf(0,g->lap_flash-dt);
    float target=verge?VERGE_MS:g->boost>0?BOOST_MS:CRUISE_MS;
    g->speed+=clamp(target-g->speed,(verge?-18:-6)*dt,(g->boost>0?30:8)*dt);
    g->lateral+=ix*10.5f*dt-rr_track_curvature(map,g->frame.distance_m)*g->speed*g->speed*.065f*dt;
    g->lateral=clamp(g->lateral,-4.5f,4.5f);
    g->frame.distance_m=fminf(total,g->frame.distance_m+g->speed*dt);
    rivals(g,dt);contacts(g);pads(g);
    int lap=(int)(g->frame.distance_m/length)+1;if(lap>RACE_LAPS)lap=RACE_LAPS;
    if(lap>g->frame.lap){g->frame.lap=lap;g->lap_flash=1.5f;g->events|=lap==RACE_LAPS?RACE_EVENT_FINAL_LAP:RACE_EVENT_LAP;}
    g->frame.lateral_m=g->lateral;g->frame.speed_kmh=(int)(g->speed*3.6f);g->frame.boost_s=g->boost;g->frame.lap_flash_ms=(int)(g->lap_flash*1000);
    for(int i=0;i<g->rival_count;i++){g->frame.rival_distance_m[i]=g->rivals[i].distance;g->frame.rival_lateral_m[i]=g->rivals[i].lateral;}
    g->frame.rank=road_rank(g);
    if(g->frame.distance_m>=total){g->phase=RACE_FINISHED;g->frame.lap_flash_ms=0;g->frame.boost_s=0;}
}
void race_game_step(race_game *g,float dt,float ix,float iy){
    if(!isfinite(dt)||!isfinite(ix)||!isfinite(iy)||dt<=0||dt>.25f)return;
    ix=clamp(ix,-.85f,.85f);iy=clamp(iy,-.85f,.85f);
    if(g->phase==RACE_COUNTDOWN){g->countdown-=dt;if(g->countdown>0)return;dt= -g->countdown;g->phase=RACE_RUNNING;}
    if(g->phase!=RACE_RUNNING)return;
    while(dt>0){
        float step=fminf(dt,.0025f);
        g->elapsed+=step;g->frame.elapsed_ms=(int)(g->elapsed*1000);
        if(g->frame.mode==0)road(g,step,ix);else marble(g,step,ix,iy);
        dt-=step;if(g->phase==RACE_FINISHED)break;
    }
}
static float dot3(const float a[3],const float b[3]){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static bool gravity(float x,float y,float z,float n[3]){
    float g=sqrtf(x*x+y*y+z*z);
    if(!isfinite(g)||g<.6f||g>1.4f)return false;
    n[0]=x/g;n[1]=y/g;n[2]=z/g;return true;
}
bool race_tilt_calibrate(race_tilt *t,float x,float y,float z){
    float n[3];if(!gravity(x,y,z,n))return false;
    /* Rotate the face-up screen axes by the shortest rotation that carries face-up gravity (0,0,1) onto n.
       Nearly face down, start from the face-down hold (a half turn about X) instead to stay well conditioned. */
    bool down=n[2]<0;float flip=down?-1:1,c=n[2]*flip;
    float v[3]={-n[1]*flip,n[0]*flip,0},k=1/(1+c);
    const float axes[2][3]={{1,0,0},{0,flip,0}};float *out[2]={t->ex,t->ey};
    for(int a=0;a<2;a++){
        const float *u=axes[a];float vu=dot3(v,u);
        float cross[3]={v[1]*u[2]-v[2]*u[1],v[2]*u[0]-v[0]*u[2],v[0]*u[1]-v[1]*u[0]};
        for(int i=0;i<3;i++)out[a][i]=c*u[i]+cross[i]+v[i]*vu*k;
    }
    return true;
}
bool race_tilt_read(const race_tilt *t,float x,float y,float z,float *ix,float *iy){
    float n[3];if(!gravity(x,y,z,n))return false;
    *ix= -dot3(n,t->ex);*iy=dot3(n,t->ey);return true;
}
