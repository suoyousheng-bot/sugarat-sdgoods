/* SPDX-License-Identifier: Apache-2.0 */
#include "synth.h"
#include <math.h>
#include <string.h>
enum { WAVE_SINE, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW };
typedef struct {
    const uint16_t (*notes)[2];
    int note_count;
    uint16_t sweep_ms, f0, f1;
    float tone, tone_per_strength, noise, noise_per_strength, decay, lowpass;
    uint8_t wave;
} effect;
static const uint16_t count_notes[][2]={{660,160}};
static const uint16_t go_notes[][2]={{990,70},{1320,330}};
static const uint16_t overtake_notes[][2]={{880,70},{1175,70},{1568,130}};
static const uint16_t lap_notes[][2]={{784,90},{988,90},{1175,180}};
static const uint16_t final_notes[][2]={{988,80},{1175,80},{1319,80},{1568,260}};
static const uint16_t checkpoint_notes[][2]={{1319,80},{1760,80},{2093,240}};
static const uint16_t win_notes[][2]={{523,130},{659,130},{784,130},{1047,460}};
static const uint16_t win_low_notes[][2]={{262,130},{330,130},{392,130},{523,460}};
static const uint16_t finish_notes[][2]={{659,130},{784,130},{988,360}};
static const uint16_t lose_notes[][2]={{523,160},{440,160},{349,160},{262,480}};
#define NOTES(n) n,(int)(sizeof n/sizeof n[0])
static const effect effects[]={
    [RACE_SFX_TICK]={NULL,0,25,1800,1800,2200,0,0,0,90,0,WAVE_SQUARE},
    [RACE_SFX_COUNT]={NOTES(count_notes),0,0,0,4200,0,0,0,6,0,WAVE_SQUARE},
    [RACE_SFX_GO]={NOTES(go_notes),0,0,0,4600,0,0,0,3,0,WAVE_SQUARE},
    [RACE_SFX_BOOST]={NULL,0,450,300,1400,2400,0,2600,0,2.5f,.35f,WAVE_SQUARE},
    [RACE_SFX_BUMP]={NULL,0,180,260,110,1500,4000,1500,4500,16,.3f,WAVE_SINE},
    [RACE_SFX_OVERTAKE]={NOTES(overtake_notes),0,0,0,3300,0,0,0,5,0,WAVE_SQUARE},
    [RACE_SFX_LAP]={NOTES(lap_notes),0,0,0,6000,0,0,0,4,0,WAVE_TRIANGLE},
    [RACE_SFX_FINAL_LAP]={NOTES(final_notes),0,0,0,3600,0,0,0,3,0,WAVE_SQUARE},
    [RACE_SFX_WALL]={NULL,0,70,900,500,1200,5000,0,1800,45,.55f,WAVE_SINE},
    [RACE_SFX_CHECKPOINT]={NOTES(checkpoint_notes),0,0,0,6000,0,0,0,5,0,WAVE_TRIANGLE},
    [RACE_SFX_WIN]={NOTES(win_notes),0,0,0,4000,0,0,0,2,0,WAVE_SQUARE},
    [RACE_SFX_FINISH]={NOTES(finish_notes),0,0,0,3600,0,0,0,2.5f,0,WAVE_SQUARE},
    [RACE_SFX_LOSE]={NOTES(lose_notes),0,0,0,5200,0,0,0,2,0,WAVE_TRIANGLE},
};
static const effect win_low={NOTES(win_low_notes),0,0,0,3000,0,0,0,2,0,WAVE_TRIANGLE};
static float clamp(float v,float a,float b){return fminf(b,fmaxf(a,v));}
static float noise(race_synth *s){
    uint32_t x=s->noise;x^=x<<13;x^=x>>17;x^=x<<5;s->noise=x;
    return (float)(int32_t)x*(1.0f/2147483648.0f);
}
static float wave(uint8_t kind,float phase){
    switch(kind){
    case WAVE_SQUARE:return phase<.25f?1.0f:-.333f;
    case WAVE_TRIANGLE:return 4*fabsf(phase-.5f)-1;
    case WAVE_SAW:return 2*phase-1;
    default:return sinf(6.2831853f*phase);
    }
}
void race_synth_init(race_synth *s,unsigned rate){
    memset(s,0,sizeof *s);s->rate=rate;s->noise=0x9e3779b9u;s->engine_freq=85;
}
static void start(race_synth *s,const effect *e,float strength){
    race_voice *v=&s->voices[0];
    for(int i=0;i<RACE_SYNTH_VOICES;i++){
        if(!s->voices[i].len){v=&s->voices[i];break;}
        if(s->voices[i].pos>v->pos)v=&s->voices[i];
    }
    memset(v,0,sizeof *v);
    v->notes=e->notes;v->note_count=e->note_count;v->wave=e->wave;
    v->tone=e->tone+e->tone_per_strength*strength;v->noise=e->noise+e->noise_per_strength*strength;
    v->decay=expf(-e->decay/(float)s->rate);v->lowpass=e->lowpass;v->env=1;
    v->f0=e->f0;v->f1=e->f1;
    uint32_t ms=e->sweep_ms;for(int i=0;i<e->note_count;i++)ms+=e->notes[i][1];
    v->len=ms*s->rate/1000;
}
void race_synth_trigger(race_synth *s,race_sfx sfx,float strength){
    if((unsigned)sfx>=sizeof effects/sizeof effects[0])return;
    strength=clamp(isfinite(strength)?strength:0,0,1);
    start(s,&effects[sfx],strength);
    if(sfx==RACE_SFX_WIN)start(s,&win_low,strength);
}
static float voice(race_synth *s,race_voice *v){
    if(!v->len)return 0;
    float freq;uint32_t attack=s->rate*3/1000,release=s->rate*5/1000,local=v->pos;
    if(v->notes){
        uint32_t edge=0;int n=0;
        for(;n<v->note_count;n++){uint32_t span=v->notes[n][1]*s->rate/1000;if(v->pos<edge+span)break;edge+=span;}
        if(n>=v->note_count)n=v->note_count-1;
        if(n!=v->note){v->note=n;v->env=1;}
        freq=v->notes[n][0];local=v->pos-edge;
        uint32_t span=v->notes[n][1]*s->rate/1000;
        if(span-local<release)v->env*=.9f;
    }else freq=v->f0+(v->f1-v->f0)*(float)v->pos/(float)v->len;
    v->phase+=freq/(float)s->rate;if(v->phase>=1)v->phase-=1;
    float gain=v->env*(local<attack?(float)local/(float)attack:1)*(v->len-v->pos<release?(float)(v->len-v->pos)/(float)release:1);
    float out=wave(v->wave,v->phase)*v->tone;
    if(v->noise>0){v->filtered+=(noise(s)-v->filtered)*v->lowpass;out+=v->filtered*v->noise;}
    v->env*=v->decay;
    if(++v->pos>=v->len)v->len=0;
    return out*gain;
}
void race_synth_render(race_synth *s,const race_synth_params *p,int16_t *out,size_t count){
    float speed=clamp(p->engine_speed,0,1.5f),roll=clamp(p->roll,0,1.3f),rate=(float)s->rate;
    float engine_freq=85+150*speed,engine_amp=p->engine?1300+900*speed:0;
    float hiss=p->engine&&p->boost?1100:0,rumble=p->engine&&p->offroad?2200:0;
    float roll_amp=3200*roll,roll_lp=.10f+.3f*fminf(roll,1);
    for(size_t i=0;i<count;i++){
        s->engine_freq+=(engine_freq-s->engine_freq)*.0015f;
        s->engine_amp+=(engine_amp-s->engine_amp)*.002f;
        s->hiss_amp+=(hiss-s->hiss_amp)*.002f;
        s->rumble_amp+=(rumble-s->rumble_amp)*.002f;
        s->roll_amp+=(roll_amp-s->roll_amp)*.003f;
        s->roll_lp+=(roll_lp-s->roll_lp)*.003f;
        float mix=0;
        if(s->engine_amp>1){
            s->engine_phase+=s->engine_freq/rate;if(s->engine_phase>=1)s->engine_phase-=1;
            s->engine_phase2+=s->engine_freq*2.01f/rate;if(s->engine_phase2>=1)s->engine_phase2-=1;
            mix+=(wave(WAVE_SAW,s->engine_phase)*.7f+wave(WAVE_SQUARE,s->engine_phase2)*.3f)*s->engine_amp;
        }
        if(s->hiss_amp>1)mix+=noise(s)*s->hiss_amp;
        if(s->rumble_amp>1){
            s->rumble_phase+=22/rate;if(s->rumble_phase>=1)s->rumble_phase-=1;
            s->roll_filtered+=(noise(s)-s->roll_filtered)*.15f;
            mix+=s->roll_filtered*s->rumble_amp*(.6f+.4f*sinf(6.2831853f*s->rumble_phase));
        }else if(s->roll_amp>1){
            /* Rolling noise pulses once per ball revolution: speed / (2 pi * 5.4 px) is about 9 Hz at 300 px/s. */
            s->roll_phase+=roll*9/rate;if(s->roll_phase>=1)s->roll_phase-=1;
            s->roll_filtered+=(noise(s)-s->roll_filtered)*s->roll_lp;
            mix+=s->roll_filtered*s->roll_amp*(.65f+.35f*sinf(6.2831853f*s->roll_phase));
        }
        for(int v=0;v<RACE_SYNTH_VOICES;v++)mix+=voice(s,&s->voices[v]);
        out[i]=(int16_t)lroundf(RACE_SYNTH_PEAK*tanhf(mix/RACE_SYNTH_PEAK));
    }
}
