/* SPDX-License-Identifier: Apache-2.0 */
/** Exercise shipped physics and peer messages with a deterministic simulated clock and loss. */
#include "game.h"
#include "link.h"
#include "synth.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {race_link *other;uint8_t mac[6],last[64];uint32_t now;unsigned count;bool drop;}wire;
static void send_packet(void *owner,const uint8_t mac[6],const uint8_t *p,size_t n){
    wire *w=owner;(void)mac;assert(n==64);memcpy(w->last,p,64);w->count++;
    if(!w->drop&&w->count%5!=0)race_link_receive(w->other,w->mac,p,n,w->now);
}
static float steer_to(const race_game *g,float lateral){
    return rr_track_curvature(g->frame.map,g->frame.distance_m)*g->speed*g->speed*.065f/10.5f-(g->lateral-lateral)*2;
}
static void racing(void){
    for(int map=0;map<3;map++){
        race_game g;race_game_begin(&g,0,map,false);for(int i=0;i<299;i++)race_game_step(&g,.01f,0,0);assert(g.phase==RACE_COUNTDOWN&&g.frame.distance_m==0);
        for(int i=0;i<20000&&g.phase!=RACE_FINISHED;i++)race_game_step(&g,.01f,steer_to(&g,0),0);
        float total=rr_track_length(map)*RACE_LAPS;
        assert(g.phase==RACE_FINISHED);assert(fabsf(g.frame.distance_m-total)<.001f);assert(g.frame.lap==RACE_LAPS);assert(g.elapsed>45&&g.elapsed<100);
        race_game_begin(&g,0,map,false);for(int i=0;i<2000;i++)race_game_step(&g,.01f,1,0);
        assert(fabsf(g.lateral)>3&&g.speed<15);
    }
}
/* A driver that aims for upcoming boost pads and otherwise passes rivals on the emptier side. */
static void rivals(void){
    for(int map=0;map<3;map++){
        race_game g;race_game_begin(&g,0,map,true);assert(g.rival_count==RR_RIVALS&&g.frame.rank==RR_RIVALS+1&&g.frame.players==RR_RIVALS+1);
        unsigned seen=0;int best=RR_RIVALS+1;float length=rr_track_length(map);
        for(int i=0;i<30000&&g.phase!=RACE_FINISHED;i++){
            float here=fmodf(g.frame.distance_m+RR_CAR_AHEAD_M,length),target=0;
            for(int k=0;k<RR_BOOST_PADS;k++){float d,l;rr_boost_pad(map,k,&d,&l);float rel=d-here;if(rel< -RR_PAD_LENGTH_M)rel+=length;if(rel<60){target=l;break;}}
            for(int k=0;k<g.rival_count;k++){float dz=g.rivals[k].distance-g.frame.distance_m;if(dz>0&&dz<14&&fabsf(g.rivals[k].lateral-target)<1.4f)target=g.rivals[k].lateral>0?g.rivals[k].lateral-1.6f:g.rivals[k].lateral+1.6f;}
            race_game_step(&g,.01f,steer_to(&g,target),0);seen|=g.events;g.events=0;
            if(g.frame.rank<best)best=g.frame.rank;
            for(int k=0;k<g.rival_count;k++)assert(isfinite(g.rivals[k].distance)&&fabsf(g.rivals[k].lateral)<=2.8f);
        }
        assert(g.phase==RACE_FINISHED);assert(seen&RACE_EVENT_BOOST);assert(seen&RACE_EVENT_LAP);assert(seen&RACE_EVENT_FINAL_LAP);
        printf("map %d: rival race %.1f s, finished %d/%d, best rank %d, events 0x%x\n",map,g.elapsed,g.frame.rank,g.frame.players,best,seen);
    }
}
static void tilt(void){
    race_tilt t;float x,y,s=sinf(.3f),c=cosf(.3f);
    assert(!race_tilt_calibrate(&t,0,0,.2f));
    assert(race_tilt_calibrate(&t,0,0,1));
    assert(race_tilt_read(&t,0,-s,c,&x,&y)&&fabsf(x)<1e-5f&&fabsf(y+s)<1e-5f);
    assert(race_tilt_read(&t,s,0,c,&x,&y)&&fabsf(x+s)<1e-5f&&fabsf(y)<1e-5f);
    /* Upright hold: the same rotations about the device axes produce the same inputs as when flat. */
    assert(race_tilt_calibrate(&t,0,1,0));
    assert(race_tilt_read(&t,0,1,0,&x,&y)&&fabsf(x)<1e-5f&&fabsf(y)<1e-5f);
    assert(race_tilt_read(&t,0,c,s,&x,&y)&&fabsf(x)<1e-5f&&fabsf(y+s)<1e-5f);
    assert(race_tilt_read(&t,s,c,0,&x,&y)&&fabsf(x+s)<1e-5f&&fabsf(y)<1e-5f);
    assert(race_tilt_calibrate(&t,.3f,.2f,-.93f));assert(race_tilt_read(&t,.3f,.2f,-.93f,&x,&y)&&fabsf(x)<.05f&&fabsf(y)<.05f);
}
static void marble_speed(void){
    race_game g;race_game_begin(&g,1,0,false);for(int i=0;i<301;i++)race_game_step(&g,.01f,0,0);
    float sx=g.x,sy=g.y,top=0;
    for(int i=0;i<100;i++){race_game_step(&g,.005f,0,.26f);top=fmaxf(top,hypotf(g.vx,g.vy));}
    printf("marble: 15 degree tilt, 0.5 s: moved %.1f px, top speed %.0f px/s\n",hypotf(g.x-sx,g.y-sy),top);
    assert(hypotf(g.x-sx,g.y-sy)>15&&top>70);
    race_game_begin(&g,1,0,false);for(int i=0;i<301;i++)race_game_step(&g,.01f,0,0);
    for(int i=0;i<200;i++)race_game_step(&g,.005f,.01f,0);assert(g.vx==0&&g.vy==0);
}
static void maze_cache(void){
    uint16_t *plain=calloc(360*360,2),*cached=calloc(360*360,2),*cache=calloc(360*360,2);assert(plain&&cached&&cache);
    for(int map=0;map<3;map++){
        rr_state st=rr_reference_state(1,map);rr_set_maze_cache(NULL);rr_render(plain,&st);
        rr_set_maze_cache(cache);rr_render(cached,&st);assert(!memcmp(plain,cached,360*360*2));
        st.marble_x+=16;rr_render(cached,&st);rr_set_maze_cache(NULL);rr_render(plain,&st);assert(!memcmp(plain,cached,360*360*2));
    }
    free(plain);free(cached);free(cache);
}
static void sound(void){
    race_synth s;race_synth_init(&s,16000);int16_t out[1600];int peak=0;
    race_synth_params p={.engine=true,.engine_speed=1.4f,.boost=true,.offroad=true};
    for(int e=RACE_SFX_TICK;e<=RACE_SFX_LOSE;e++)race_synth_trigger(&s,(race_sfx)e,1);
    for(int b=0;b<20;b++){race_synth_render(&s,&p,out,1600);for(int i=0;i<1600;i++)if(abs(out[i])>peak)peak=abs(out[i]);}
    assert(peak>2000&&peak<=RACE_SYNTH_PEAK);
    race_synth_params quiet={0};race_synth_init(&s,16000);race_synth_render(&s,&quiet,out,1600);for(int i=0;i<1600;i++)assert(out[i]==0);
}
static void move_to(race_game *g,int tx,int ty){
    for(int tick=0;tick<4000;tick++){
        if(g->phase==RACE_FINISHED)return;
        if(hypotf(g->x-tx,g->y-ty)<1.4f)return;
        race_game_step(g,.005f,(tx-g->x)*.09f-g->vx*.07f,(ty-g->y)*.09f-g->vy*.07f);
        assert(race_game_ball_free(g->frame.map,g->x,g->y,5.4f));
    }
    fprintf(stderr,"Could not reach (%d,%d) from (%.2f,%.2f)\n",tx,ty,g->x,g->y);assert(0);
}
static void path_to(race_game *g,int tx,int ty){
    int parent[289];for(int i=0;i<289;i++)parent[i]= -2;
    int sx=((int)g->x-44)/16,sy=((int)g->y-63)/16,start=sy*17+sx,target=((ty-63)/16)*17+(tx-44)/16;
    int queue[289],head=0,tail=0;queue[tail++]=start;parent[start]= -1;
    while(head<tail){int p=queue[head++],x=p%17,y=p/17;int dx[4]={1,0,-1,0},dy[4]={0,1,0,-1};for(int k=0;k<4;k++){
        int nx=x+dx[k],ny=y+dy[k],n=ny*17+nx;
        if(nx<0||ny<0||nx>=17||ny>=17||parent[n]!= -2||!rr_maze_open(g->frame.map,52+16*nx,71+16*ny))continue;
        parent[n]=p;queue[tail++]=n;
    }}
    assert(parent[target]!= -2);int reverse[289],count=0;
    for(int n=target;n!= -1;n=parent[n])reverse[count++]=n;
    for(int i=count-1;i>=0;i--){int n=reverse[i];move_to(g,52+16*(n%17),71+16*(n/17));}
}
static void marbles(void){
    for(int map=0;map<3;map++){
        race_game g;race_game_begin(&g,1,map,false);for(int i=0;i<301;i++)race_game_step(&g,.01f,0,0);assert(g.phase==RACE_RUNNING);
        for(int point=2;point<5;point++){int x,y;rr_maze_point(map,point,&x,&y);path_to(&g,x,y);}
        assert(g.checkpoint_mask==7);int x,y;rr_maze_point(map,1,&x,&y);path_to(&g,x,y);assert(g.phase==RACE_FINISHED);
        race_game_begin(&g,1,map,false);for(int i=0;i<12000;i++){race_game_step(&g,.005f,sinf(i*.031f),cosf(i*.019f));assert(race_game_ball_free(map,g.x,g.y,5.4f));}
    }
}
static void networking(void){
    race_link a,b;race_game ga,gb;race_game_begin(&ga,0,1,false);race_game_begin(&gb,0,1,false);
    wire ab={.other=&b,.mac={2,0,0,0,0,1}},ba={.other=&a,.mac={2,0,0,0,0,2}};
    race_link_begin(&a,0,1,ab.mac,12345,send_packet,&ab);race_link_begin(&b,0,1,ba.mac,45678,send_packet,&ba);
    a.ready=b.ready=true;
    for(uint32_t now=100;now<6000;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(a.paired&&b.paired&&a.host&&!b.host&&a.committed&&b.committed);assert(a.host_deadline==b.host_deadline);
    assert(abs((int32_t)(a.local_deadline-b.local_deadline))<70);
    assert(!race_link_receive(&b,ab.mac,ab.last,64,6000));
    uint8_t corrupt[64];memcpy(corrupt,ab.last,64);corrupt[32]^=128;assert(!race_link_receive(&b,ab.mac,corrupt,64,6000));
    assert(!race_link_receive(&b,ab.mac,ab.last,63,6000));
    ab.drop=ba.drop=true;
    for(uint32_t now=6000;now<8000;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(a.lost&&b.lost);ab.drop=ba.drop=false;a.paused=true;
    for(uint32_t now=8000;now<8300;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(!a.lost&&!b.lost&&b.peer_paused);
    /* A crosses the line first: B concedes, and A never reads the concession as a faster finish. */
    a.paused=false;ga.phase=gb.phase=RACE_RUNNING;ga.frame.distance_m=900;gb.frame.distance_m=950;
    for(uint32_t now=8300;now<8500;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(race_link_rank(&a,&ga)==2&&race_link_rank(&b,&gb)==1);
    ga.phase=RACE_FINISHED;ga.frame.elapsed_ms=60000;ga.frame.distance_m=rr_track_length(1)*RACE_LAPS;
    for(uint32_t now=8500;now<8700;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(race_link_peer_finished(&b)&&!race_link_peer_finished(&a));assert(race_link_rank(&b,&gb)==2&&race_link_rank(&a,&ga)==1);
    gb.phase=RACE_FINISHED;gb.frame.elapsed_ms=59000;b.conceded=true;
    for(uint32_t now=8700;now<8900;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(a.peer_conceded&&!race_link_peer_finished(&a));assert(race_link_rank(&a,&ga)==1&&race_link_rank(&b,&gb)==2);
    /* Both really finished: the lower time wins, and an exact tie goes to the host. */
    b.conceded=false;gb.frame.elapsed_ms=59000;
    for(uint32_t now=8900;now<9100;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(race_link_rank(&a,&ga)==2&&race_link_rank(&b,&gb)==1);
    gb.frame.elapsed_ms=60000;
    for(uint32_t now=9100;now<9300;now+=20){ab.now=ba.now=now;race_link_tick(&a,&ga,now);race_link_tick(&b,&gb,now);}
    assert(race_link_rank(&a,&ga)==1&&race_link_rank(&b,&gb)==2);
}
int main(void){racing();rivals();tilt();marble_speed();maze_cache();sound();marbles();networking();puts("PASS: three-lap road finishes, rival races with boosts, flat and upright tilt calibration, marble rolling, cached maze frames, bounded sound, off-road slowdown, three maze finishes, wall containment, lossy peer start, replay rejection, disconnect and pause, first finisher wins and the other badge concedes");return 0;}
