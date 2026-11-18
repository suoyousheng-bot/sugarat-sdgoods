/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "card_view.h"
#include <stdio.h>
#include <string.h>

#define T(zh,en) (v->language==CARD_EN?(en):(zh))
enum {BG=0xf7f2e7,INK=0x26382b,MUTED=0x5e685c,SURFACE=0xe2e7da,ACCENT=0xd4ef79};
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *label(lv_obj_t *parent,const char *value,int x,int y,int width,sw_font_t font,uint32_t color,lv_text_align_t align)
{
    lv_obj_t *o=sw_label(parent,value,font,SW_TONE_TEXT);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);lv_obj_set_style_text_align(o,align,0);
    lv_obj_set_height(o,sw_font(font)->line_height);
    lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;
}
static lv_obj_t *marquee_label(lv_obj_t *parent,const char *value,int y,sw_font_t font,uint32_t color)
{
    lv_obj_t *o=label(parent,value,64,y,232,font,color,LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_anim_speed(o,32,0);lv_label_set_long_mode(o,LV_LABEL_LONG_SCROLL_CIRCULAR);return o;
}
static void footnote(card_view_t *v,const char *text,int y)
{
    lv_obj_t *o=label(v->content,text,64,y,232,SW_FONT_CAPTION,MUTED,LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_obj_set_height(o,36);
}
static void title(card_view_t *v,const char *text)
{label(v->content,text,64,56,232,SW_FONT_HEADLINE,INK,LV_TEXT_ALIGN_CENTER);}
static void act(void *context)
{
    card_button_context_t *c=context;card_view_t *v=c->view;
    if(v->paused||v->gesture.moved||v->dragging||v->animating)return;
    v->action(v->context,c->action);
}
static lv_obj_t *button(card_view_t *v,lv_obj_t *parent,const char *text,int x,int y,int w,int h,card_action_t action)
{
    unsigned slot=v->button_count++;LV_ASSERT(slot<10);
    lv_obj_t *o=sw_button(parent,SW_ICON_NONE,NULL,SW_BUTTON_FILLED,SW_TONE_TEAL);lv_obj_set_layout(o,0);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_min_width(o,0,0);
    lv_obj_set_style_radius(o,24,0);lv_obj_set_style_bg_color(o,lv_color_hex(SURFACE),0);
    lv_obj_set_style_bg_color(o,lv_color_hex(ACCENT),LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(o,0,LV_STATE_PRESSED);lv_obj_set_style_transform_height(o,0,LV_STATE_PRESSED);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_ADV_HITTEST);
    if(text)label(o,text,8,(h-sw_font(SW_FONT_BODY)->line_height)/2,w-16,SW_FONT_BODY,INK,LV_TEXT_ALIGN_CENTER);
    v->button_contexts[slot]=(card_button_context_t){v,action};sdgoods_tap_bind(o,&v->taps[slot],act,&v->button_contexts[slot]);return o;
}
static void qr_icon(lv_obj_t *parent)
{
    for(unsigned i=0;i<3;++i) {
        int x=34+(i==1?17:0),y=8+(i==2?17:0);
        lv_obj_t *finder=box(parent,x,y,11,11,INK,0);lv_obj_set_style_bg_opa(finder,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(finder,lv_color_hex(INK),0);lv_obj_set_style_border_width(finder,2,0);
        box(parent,x+4,y+4,3,3,INK,0);
    }
    box(parent,51,25,4,4,INK,0);box(parent,58,26,4,7,INK,0);box(parent,51,32,6,4,INK,0);
}
static void slide(void *object,int32_t y){lv_obj_set_y(object,(lv_coord_t)y);}
static void slide_ready(lv_anim_t *animation)
{
    card_view_t *v=lv_anim_get_user_data(animation);v->animating=false;
    if(!v->open){v->visible=false;lv_obj_add_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);}
    if(v->render_pending)card_view_refresh(v);
}
void card_view_drawer(card_view_t *v,bool open)
{
    if(open&&(v->page!=CARD_PAGE_HOME||v->paused))return;
    v->open=open;v->dragging=false;v->visible=true;lv_obj_clear_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(v->drawer,slide);v->animating=true;
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,v->drawer);lv_anim_set_exec_cb(&a,slide);
    lv_anim_set_values(&a,lv_obj_get_y(v->drawer),open?104:360);lv_anim_set_time(&a,170);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);
    lv_anim_set_user_data(&a,v);lv_anim_set_ready_cb(&a,slide_ready);lv_anim_start(&a);
}
static void outside(void *context)
{card_view_t *v=context;if(!v->gesture.moved&&!v->paused)card_view_drawer(v,false);}
static void drag(card_view_t *v,int y)
{
    lv_anim_del(v->drawer,slide);v->animating=false;v->dragging=v->visible=true;
    lv_obj_clear_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);lv_obj_set_y(v->drawer,LV_CLAMP(104,y,360));
}
static void gesture(lv_event_t *event)
{
    card_view_t *v=lv_event_get_user_data(event);lv_event_code_t code=lv_event_get_code(event);
    if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_PRESSING&&code!=LV_EVENT_RELEASED&&code!=LV_EVENT_PRESS_LOST)return;
    lv_indev_t *input=lv_indev_get_act();if(!input)return;lv_point_t p;lv_indev_get_point(input,&p);
    if(code==LV_EVENT_PRESSED) {
        memset(&v->gesture,0,sizeof(v->gesture));if(v->paused||(v->page!=CARD_PAGE_HOME&&v->page!=CARD_PAGE_CONTACTS)||p.y<50)return;
        if(v->page==CARD_PAGE_CONTACTS&&p.x<56)return;
        v->gesture.active=true;v->gesture.start=p;v->gesture.was_open=v->open;
        v->gesture.eligible=v->page==CARD_PAGE_HOME&&(v->visible||p.y>=232);v->gesture.origin_y=v->visible?lv_obj_get_y(v->drawer):360;return;
    }
    if(!v->gesture.active)return;
    int dx=p.x-v->gesture.start.x,dy=p.y-v->gesture.start.y;
    v->gesture.max_distance=LV_MAX(v->gesture.max_distance,LV_MAX(LV_ABS(dx),LV_ABS(dy)));
    if(code==LV_EVENT_PRESSING) {
        if(!v->gesture.axis&&v->gesture.max_distance>=10)v->gesture.axis=LV_ABS(dy)*4>LV_ABS(dx)*5?2:1;
        if(v->gesture.max_distance<10)return;
        v->gesture.moved=true;
        if(v->gesture.eligible&&v->gesture.axis==2)drag(v,v->gesture.origin_y+dy);
        return;
    }
    v->gesture.active=false;
    if(v->page==CARD_PAGE_CONTACTS) {
        if(code==LV_EVENT_RELEASED&&LV_ABS(dx)>=38&&LV_ABS(dx)>2*LV_ABS(dy)) {
            if(dx<0&&v->selected+1<v->state->count)v->action(v->context,CARD_ACT_NEXT);
            else if(dx>0&&v->selected)v->action(v->context,CARD_ACT_PREV);
        }
        return;
    }
    if(v->visible&&v->gesture.start.x<=24&&dx>64&&dx>2*LV_ABS(dy)){card_view_drawer(v,false);return;}
    if(v->dragging) {
        bool open=code==LV_EVENT_PRESS_LOST?v->gesture.was_open:
            v->gesture.was_open?lv_obj_get_y(v->drawer)<154:lv_obj_get_y(v->drawer)<324;
        card_view_drawer(v,open);
    }
}
static void profile_content(card_view_t *v,const card_profile_t *p)
{
    lv_obj_t *s=v->content;char meta[sizeof(p->role)+sizeof(p->company)+8],value[sizeof(p->contact)+32];
    marquee_label(s,p->name[0]?p->name:T("先编辑名片","Edit your card"),104,SW_FONT_TITLE,INK);
    if(p->role[0]&&p->company[0])snprintf(meta,sizeof(meta),"%s · %s",p->role,p->company);
    else snprintf(meta,sizeof(meta),"%s",p->role[0]?p->role:p->company[0]?p->company:T("身份 · 公司","Role · Company"));
    marquee_label(s,meta,152,SW_FONT_BODY,MUTED);box(s,112,184,136,1,SURFACE,0);
    const char *kind[]={T("邮箱","Email"),T("电话","Phone"),T("微信","WeChat")};
    snprintf(value,sizeof(value),"%s · %s",kind[p->kind],p->contact[0]?p->contact:T("未填写","Not set"));
    marquee_label(s,value,208,SW_FONT_BODY,INK);
    const char *link=p->link;if(!strncmp(link,"https://",8))link+=8;else if(!strncmp(link,"http://",7))link+=7;
    char url[sizeof(p->link)+32];snprintf(url,sizeof(url),"%s · %s",T("主页","Web"),link[0]?link:T("未填写","Not set"));
    marquee_label(s,url,240,SW_FONT_BODY,INK);
}
static void home(card_view_t *v)
{
    label(v->content,"Sugarat",128,56,104,SW_FONT_CAPTION,MUTED,LV_TEXT_ALIGN_CENTER);
    profile_content(v,&v->state->profile);
    if(v->notice.zh)footnote(v,T(v->notice.zh,v->notice.en),272);
    box(v->content,160,320,40,4,MUTED,2);
}
static void make_qr(card_view_t *v,const char *data,size_t size,int x,int y,int width)
{
    lv_obj_t *white=box(v->content,x-12,y-12,width+24,width+24,0xffffff,16);
    lv_obj_t *q=lv_qrcode_create(white,width,lv_color_hex(0),lv_color_hex(0xffffff));lv_obj_set_pos(q,12,12);lv_obj_clear_flag(q,LV_OBJ_FLAG_CLICKABLE);
    if(lv_qrcode_update(q,data,size)!=LV_RES_OK){lv_obj_del(white);footnote(v,T("内容过长，请使用设备交换","Card too long. Use badge exchange."),168);}
}
static void qr(card_view_t *v)
{
    title(v,T("我的名片码","My card QR"));
    char card[1600];size_t size=card_profile_vcard(&v->state->profile,card,sizeof(card));
    if(size)make_qr(v,card,size,92,112,176);else footnote(v,T("请先编辑并保存名片","Edit and save your card first."),168);
    footnote(v,T("扫码保存联系人","Scan to save contact"),312);
}
static void nearby(card_view_t *v)
{
    lv_obj_t *s=v->content;const card_exchange_t *e=v->exchange;
    title(v,T("交换名片","Exchange cards"));
    if(!e->count) {
        lv_obj_t *icon=sw_icon(s,SW_ICON_PEOPLE,SW_SIZE_XL,SW_TONE_TEXT);lv_obj_set_style_text_color(icon,lv_color_hex(MUTED),0);lv_obj_set_pos(icon,160,136);
        label(s,T("等待附近设备","Looking for badges"),64,192,232,SW_FONT_BODY,INK,LV_TEXT_ALIGN_CENTER);
        footnote(v,T("双方都打开交换页面","Open Exchange on both badges."),240);return;
    }
    unsigned index=v->selected%e->count;
    box(s,64,112,232,96,SURFACE,24);marquee_label(s,e->peers[index].name,128,SW_FONT_TITLE,INK);
    char count[80];snprintf(count,sizeof(count),T("附近 %u 台 · 第 %u 台","%u nearby · Badge %u"),e->count,index+1);
    footnote(v,count,176);
    button(v,s,T("下一台","Next"),72,240,96,56,CARD_ACT_NEXT);button(v,s,T("选择对方","Select"),192,240,96,56,CARD_ACT_SELECT);
}
static void verify(card_view_t *v)
{
    lv_obj_t *s=v->content;const card_exchange_t *e=v->exchange;
    title(v,e->phase==CARD_EX_DONE?T("交换成功","Cards exchanged"):T("确认交换","Check the code"));
    marquee_label(s,e->peer.name,104,SW_FONT_TITLE,INK);
    if(e->phase==CARD_EX_DONE) {
        footnote(v,T("双方名片已保存","Both cards saved."),176);
        button(v,s,T("查看名片夹","View contacts"),72,240,216,56,CARD_ACT_CONTACTS);return;
    }
    char code[16];snprintf(code,sizeof(code),"%03u %03u",e->code/1000,e->code%1000);
    label(s,code,64,152,232,SW_FONT_NUM_M,INK,LV_TEXT_ALIGN_CENTER);
    footnote(v,e->local_confirmed?(e->peer_confirmed?T("正在交换，请稍候","Exchanging cards…"):T("等待对方核对并确认","Waiting for their confirmation.")):T("请核对双方屏幕上的数字","Compare codes on both badges."),200);
    button(v,s,T("取消","Cancel"),72,240,96,56,CARD_ACT_CANCEL);
    lv_obj_t *b=button(v,s,e->local_confirmed?T("已确认","Confirmed"):T("确认交换","Confirm"),192,240,96,56,CARD_ACT_CONFIRM);
    if(e->local_confirmed)lv_obj_add_state(b,LV_STATE_DISABLED);
}
static void edit(card_view_t *v)
{
    lv_obj_t *s=v->content;char value[96];
    title(v,v->edit_connected?T("手机编辑","Edit on phone"):T("连接手机","Connect phone"));
    if(v->edit_connected) {
        footnote(v,T("2  用手机相机扫码打开","2  Scan with your phone camera"),104);
        static const char entry[]="http://192.168.4.1/";make_qr(v,entry,strlen(entry),108,152,144);
        footnote(v,"http://192.168.4.1",312);
    } else {
        footnote(v,T("1  打开手机 Wi-Fi 设置","1  Open phone Wi-Fi settings"),112);
        snprintf(value,sizeof(value),"Wi-Fi: %s",v->ssid?v->ssid:"");
        label(s,value,64,152,232,SW_FONT_HEADLINE,INK,LV_TEXT_ALIGN_CENTER);
        bool secured=v->password&&v->password[0];
        if(secured)snprintf(value,sizeof(value),T("密码：%s","Password: %s"),v->password);else snprintf(value,sizeof(value),"%s",T("无需密码，直接连接","No password needed"));
        label(s,value,64,192,232,SW_FONT_BODY,INK,LV_TEXT_ALIGN_CENTER);
        footnote(v,T("提示无互联网时保持连接","No internet? Stay connected."),224);
        button(v,s,secured?T("移除密码","Remove password"):T("使用密码","Use password"),72,256,216,48,CARD_ACT_EDIT_SECURITY);
    }
    if(v->notice.zh)footnote(v,T(v->notice.zh,v->notice.en),312);
}
static void contacts(card_view_t *v)
{
    char text[64];snprintf(text,sizeof(text),T("名片夹 %u / %u","Contacts %u / %u"),v->state->count?v->selected+1:0,v->state->count);title(v,text);
    if(!v->state->count){footnote(v,T("还没有收到名片","No cards received yet."),168);return;}
    profile_content(v,&v->state->contacts[v->selected%v->state->count].profile);
    footnote(v,T("左右滑动切换名片","Swipe to browse contacts"),288);
}
void card_view_refresh(card_view_t *v)
{
    if(v->dragging||v->animating){v->render_pending=true;return;}v->render_pending=false;
    if(v->page!=CARD_PAGE_HOME){v->open=v->visible=false;lv_obj_add_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);lv_obj_set_y(v->drawer,360);}
    const char *names[]={T("名片码","QR code"),T("交换","Exchange"),T("编辑","Edit"),T("名片夹","Contacts")};
    for(unsigned i=0;i<4;++i)lv_label_set_text(v->button_labels[i],names[i]);
    lv_label_set_text(v->language_label,T("English","中文"));
    lv_obj_clean(v->content);v->button_count=5;
    switch(v->page) {
    case CARD_PAGE_HOME:home(v);break;
    case CARD_PAGE_QR:qr(v);break;
    case CARD_PAGE_NEARBY:nearby(v);break;
    case CARD_PAGE_VERIFY:verify(v);break;
    case CARD_PAGE_EDIT:edit(v);break;
    case CARD_PAGE_CONTACTS:contacts(v);break;
    case CARD_PAGE_NOTICE:
        title(v,T("暂时无法继续","Unable to continue"));
        footnote(v,v->notice.zh?T(v->notice.zh,v->notice.en):T("请返回重试","Go back and try again."),152);
        footnote(v,T("从左侧向右滑返回","Swipe right from the left edge"),272);break;
    }
}
void card_view_create(card_view_t *v,const card_state_t *state,const card_exchange_t *exchange,card_action_fn action,void *context)
{
    memset(v,0,sizeof(*v));v->language=CARD_EN;v->state=state;v->exchange=exchange;v->action=action;v->context=context;
    v->screen=sw_screen();lv_obj_set_style_bg_color(v->screen,lv_color_hex(BG),0);
    lv_obj_add_flag(v->screen,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);lv_obj_add_event_cb(v->screen,gesture,LV_EVENT_ALL,v);
    v->content=box(v->screen,0,0,360,360,BG,0);
    v->shade=box(v->screen,0,0,360,360,INK,0);lv_obj_set_style_bg_opa(v->shade,LV_OPA_40,0);
    lv_obj_add_flag(v->shade,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_EVENT_BUBBLE);sdgoods_tap_bind(v->shade,&v->shade_tap,outside,v);
    v->drawer=box(v->screen,0,360,360,256,BG,40);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK);
    box(v->drawer,160,16,40,4,MUTED,2);
    const sw_icon_t icons[]={SW_ICON_NONE,SW_ICON_SYNC,SW_ICON_SETTINGS,SW_ICON_PEOPLE};
    for(unsigned i=0;i<4;++i) {
        lv_obj_t *b=button(v,v->drawer,NULL,72+(i%2)*120,32+(i/2)*72,96,64,(card_action_t)i);v->buttons[i]=b;
        if(i==0)qr_icon(b);else {
            lv_obj_t *icon=sw_icon(b,icons[i],SW_SIZE_L,SW_TONE_TEXT);lv_obj_set_style_text_color(icon,lv_color_hex(INK),0);lv_obj_align(icon,LV_ALIGN_TOP_MID,0,8);lv_obj_clear_flag(icon,LV_OBJ_FLAG_CLICKABLE);
        }
        v->button_labels[i]=label(b,"",0,40,96,SW_FONT_CAPTION,INK,LV_TEXT_ALIGN_CENTER);
    }
    lv_obj_t *language=button(v,v->drawer,NULL,128,184,104,40,CARD_ACT_LANGUAGE);
    v->language_label=label(language,"",0,10,104,SW_FONT_BODY,INK,LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);card_view_refresh(v);
}
void card_view_pause(card_view_t *v,bool paused)
{
    v->paused=paused;v->gesture.active=false;lv_anim_del(v->drawer,slide);v->open=v->visible=v->dragging=v->animating=false;
    lv_obj_add_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);lv_obj_set_y(v->drawer,360);
}
void card_view_dispose(card_view_t *v)
{if(v->drawer)lv_anim_del(v->drawer,slide);if(v->screen)lv_obj_del(v->screen);memset(v,0,sizeof(*v));}
bool card_view_profile_supported(const card_profile_t *p)
{
    const char *strings[]={p->company,p->name,p->role,p->contact,p->link};
    for(unsigned k=0;k<5;++k){uint32_t i=0;while(strings[k][i]){uint32_t cp=_lv_txt_encoded_next(strings[k],&i);lv_font_glyph_dsc_t d;
        if(!lv_font_get_glyph_dsc(sw_font(k==1?SW_FONT_TITLE:SW_FONT_BODY),&d,cp,0))return false;}}
    return true;
}
