/* SPDX-License-Identifier: Apache-2.0 */
/** Two actual session engines communicate through a deterministic lossy/reordering transport. */
#include "fluid/link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t host_mac[6]={1,2,3,4,5,6},guest_mac[6]={6,5,4,3,2,1};
typedef struct { uint8_t bytes[BOTTLE_PACKET_SIZE]; unsigned count; } wire_t;
static void capture(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{
    (void)mac; assert(size==BOTTLE_PACKET_SIZE); wire_t *wire=context;
    memcpy(wire->bytes,bytes,size); wire->count++;
}
static void deliver(bottle_link_t *to,wire_t *wire,const uint8_t mac[6],uint32_t now)
{
    if(wire->count) { bottle_link_receive(to,mac,wire->bytes,sizeof(wire->bytes),now); wire->count=0; }
}
static bottle_link_t host,guest;
static wire_t hwire,gwire;
static uint32_t clock_ms;
static unsigned expected[4]={480,0,0,0};
static void round_trip(bool drop_host,bool drop_guest)
{
    clock_ms+=100;
    bottle_link_tick(&host,clock_ms,capture,&hwire);
    if(!drop_host) deliver(&guest,&hwire,host_mac,clock_ms); else hwire.count=0;
    bottle_link_tick(&guest,clock_ms,capture,&gwire);
    if(!drop_guest) deliver(&host,&gwire,guest_mac,clock_ms); else gwire.count=0;
    if(host.revision) for(unsigned i=0;i<4;i++) assert(host.liquid[0].part[i]+host.liquid[1].part[i]==expected[i]);
    assert(bottle_liquid_valid(&host.liquid[0]) && bottle_liquid_valid(&host.liquid[1]));
}
static void connect_pair(void)
{
    expected[0]=480; expected[1]=expected[2]=expected[3]=0;
    memset(&hwire,0,sizeof(hwire)); memset(&gwire,0,sizeof(gwire)); clock_ms=0;
    bottle_link_solo(&host,0,480); bottle_link_solo(&guest,1,0);
    bottle_link_host(&host,123,0); bottle_link_scan(&guest,456,0);
    round_trip(false,false); assert(guest.phase==BOTTLE_FOUND);
    bottle_link_join(&guest,clock_ms); round_trip(false,false); assert(host.phase==BOTTLE_INVITED);
    bottle_link_accept(&host,clock_ms); round_trip(false,false);
    assert(host.phase==BOTTLE_LINKED && guest.phase==BOTTLE_LINKED);
    assert(!memcmp(host.liquid,guest.liquid,sizeof(host.liquid)));
    bottle_link_input(&host,(bottle_input_t){BOTTLE_RECEIVE,0});
    bottle_link_input(&guest,(bottle_input_t){BOTTLE_RECEIVE,0}); round_trip(false,false);
}
static void quantities(void)
{
    bottle_liquid_t a={{137,29,54,0}},b={{0,0,0,180}};
    unsigned totals[4]; for(unsigned i=0;i<4;i++) totals[i]=a.part[i]+b.part[i];
    for(unsigned i=0;i<2000;i++) {
        bottle_liquid_transfer(i%2?&a:&b,i%2?&b:&a,1+i%23);
        assert(bottle_liquid_valid(&a) && bottle_liquid_valid(&b));
        for(unsigned j=0;j<4;j++) assert(a.part[j]+b.part[j]==totals[j]);
    }
    bottle_liquid_fill(&a,600,0); bottle_liquid_fill(&b,599,1);
    assert(bottle_liquid_transfer(&a,&b,100)==1);
    assert(bottle_liquid_transfer(&a,&b,100)==0);
    bottle_liquid_fill(&b,0,1); assert(bottle_liquid_transfer(&a,&b,600)==599);
    assert(!bottle_liquid_total(&a));
}
static void poses(void)
{
    assert(bottle_input_from_gravity(0,10,0,true).pose==BOTTLE_RECEIVE);
    assert(bottle_input_from_gravity(0,-10,0,true).pose==BOTTLE_POUR);
    assert(bottle_input_from_gravity(0,0,10,true).pose==BOTTLE_CLOSED);
    assert(bottle_input_from_gravity(0,-10,0,false).pose==BOTTLE_CLOSED);
    assert(bottle_input_from_gravity(0,0,0,true).pose==BOTTLE_CLOSED);
}
static void transfer_and_loss(void)
{
    connect_pair(); bottle_link_input(&host,(bottle_input_t){BOTTLE_POUR,6});
    for(unsigned i=0;i<240;i++) round_trip(i%7==0,i%5==0);
    assert(!bottle_liquid_total(&host.liquid[0]));
    assert(bottle_liquid_total(&host.liquid[1])==480);
    bottle_link_input(&host,(bottle_input_t){BOTTLE_RECEIVE,0});
    bottle_link_input(&guest,(bottle_input_t){BOTTLE_POUR,6});
    for(unsigned i=0;i<240;i++) round_trip(i%5==0,i%7==0);
    assert(bottle_liquid_total(&host.liquid[0])==480);
    assert(!bottle_liquid_total(&host.liquid[1]));
    bottle_link_input(&host,(bottle_input_t){BOTTLE_POUR,6});
    bottle_link_input(&guest,(bottle_input_t){BOTTLE_RECEIVE,0}); round_trip(false,false);
    round_trip(true,true); unsigned revision=host.revision;
    for(unsigned i=0;i<30;i++) round_trip(true,true);
    assert(host.revision==revision && host.phase==BOTTLE_LOST && guest.phase==BOTTLE_LOST);
    for(unsigned i=0;i<4;i++) round_trip(false,false);
    assert(host.phase==BOTTLE_LINKED && guest.phase==BOTTLE_LINKED);
    assert(!memcmp(host.liquid,guest.liquid,sizeof(host.liquid)));
    assert(host.rearm && guest.rearm);
    bottle_link_input(&host,(bottle_input_t){BOTTLE_POUR,6});
    assert(host.local.pose==BOTTLE_CLOSED);
    bottle_link_input(&host,(bottle_input_t){BOTTLE_RECEIVE,0});
    bottle_link_input(&guest,(bottle_input_t){BOTTLE_RECEIVE,0});
    for(unsigned i=0;i<10;i++) round_trip(false,false);
    assert(host.revision==revision);
}
static void packets(void)
{
    connect_pair();
    bottle_packet_t packet={.type=BOTTLE_INPUT,.room=host.room,.sender=guest.self,.target=host.self,
        .sequence=host.peer_sequence+1,.acknowledgement=host.revision,.input={BOTTLE_RECEIVE,0}};
    uint8_t bytes[BOTTLE_PACKET_SIZE]; bottle_packet_t decoded;
    bottle_packet_encode(&packet,bytes); assert(bottle_packet_decode(&decoded,bytes,sizeof(bytes)));
    bottle_link_receive(&host,guest_mac,bytes,sizeof(bytes),clock_ms);
    bottle_link_t before=host;
    bottle_link_receive(&host,guest_mac,bytes,sizeof(bytes),clock_ms+10); assert(!memcmp(&before,&host,sizeof(host)));
    uint8_t stranger[6]={9,9,9,9,9,9};
    packet.sequence++; bottle_packet_encode(&packet,bytes);
    bottle_link_receive(&host,stranger,bytes,sizeof(bytes),clock_ms+10); assert(!memcmp(&before,&host,sizeof(host)));
    packet.room++; bottle_packet_encode(&packet,bytes);
    bottle_link_receive(&host,guest_mac,bytes,sizeof(bytes),clock_ms+10); assert(!memcmp(&before,&host,sizeof(host)));
    assert(!bottle_packet_decode(&decoded,bytes,sizeof(bytes)-1));
    bytes[3]='2'; assert(!bottle_packet_decode(&decoded,bytes,sizeof(bytes))); bytes[3]='1';
    bytes[5]=BOTTLE_POUR; bytes[6]=0; assert(!bottle_packet_decode(&decoded,bytes,sizeof(bytes)));
    bottle_packet_encode(&packet,bytes); bytes[28]=255; bytes[29]=255;
    assert(!bottle_packet_decode(&decoded,bytes,sizeof(bytes)));
    bottle_link_leave(&host,capture,&hwire); deliver(&guest,&hwire,host_mac,clock_ms);
    assert(guest.phase==BOTTLE_LOST);
    bottle_link_solo(&host,0,480); bottle_link_host(&host,789,clock_ms);
    for(unsigned i=0;i<20;i++) round_trip(false,false);
    assert(guest.phase==BOTTLE_LOST);
}
static void ordering_and_expiry(void)
{
    connect_pair();
    bottle_link_tick(&host,clock_ms+100,capture,&hwire);
    uint8_t older[BOTTLE_PACKET_SIZE]; memcpy(older,hwire.bytes,sizeof(older));
    bottle_link_input(&host,(bottle_input_t){BOTTLE_CLOSED,0});
    bottle_link_tick(&host,clock_ms+200,capture,&hwire);
    deliver(&guest,&hwire,host_mac,clock_ms+200);
    bottle_link_t before=guest;
    bottle_link_receive(&guest,host_mac,older,sizeof(older),clock_ms+201);
    assert(!memcmp(&before,&guest,sizeof(guest)));
    bottle_packet_t forged; assert(bottle_packet_decode(&forged,older,sizeof(older)));
    forged.acknowledgement=guest.peer_sequence+1; forged.sequence=guest.revision+1;
    forged.liquid[0].part[0]--;
    bottle_packet_encode(&forged,older);
    bottle_link_receive(&guest,host_mac,older,sizeof(older),clock_ms+202);
    assert(!memcmp(&before,&guest,sizeof(guest)));
    bottle_link_t scan; bottle_link_solo(&scan,0,480); bottle_link_scan(&scan,999,0);
    bottle_packet_t hello={.type=BOTTLE_HELLO,.room=123,.sender=123};
    bottle_packet_encode(&hello,older); bottle_link_receive(&scan,host_mac,older,sizeof(older),100);
    assert(scan.phase==BOTTLE_FOUND);
    bottle_link_scan(&scan,998,200); bottle_link_receive(&scan,host_mac,older,sizeof(older),300);
    assert(scan.phase==BOTTLE_SCANNING);
    hello.room=124; hello.sender=124; bottle_packet_encode(&hello,older);
    bottle_link_receive(&scan,host_mac,older,sizeof(older),400); assert(scan.phase==BOTTLE_FOUND);
    bottle_link_tick(&scan,2000,capture,&gwire); assert(scan.phase==BOTTLE_SCANNING);
    bottle_link_receive(&scan,host_mac,older,sizeof(older),2100); bottle_link_join(&scan,2100);
    bottle_link_tick(&scan,12101,capture,&gwire); assert(scan.phase==BOTTLE_LOST);
    assert(!scan.revision);
    bottle_link_input(&scan,(bottle_input_t){BOTTLE_RECEIVE,0});
    assert(scan.rearm && scan.local.pose==BOTTLE_CLOSED);
}
static void cancellation(void)
{
    connect_pair();
    bottle_link_input(&host,(bottle_input_t){BOTTLE_POUR,6}); bottle_link_input(&guest,(bottle_input_t){BOTTLE_POUR,6});
    round_trip(false,false); unsigned both_tipped=host.revision;
    for(unsigned i=0;i<10;i++) round_trip(false,false);
    assert(host.revision==both_tipped);
    bottle_link_input(&guest,(bottle_input_t){BOTTLE_RECEIVE,0}); round_trip(false,false); round_trip(false,false);
    unsigned amount=bottle_liquid_total(&host.liquid[0]);
    bottle_link_input(&host,(bottle_input_t){BOTTLE_CLOSED,0});
    for(unsigned i=0;i<10;i++) round_trip(false,false);
    assert(bottle_liquid_total(&host.liquid[0])==amount);
}
static void mixing(void)
{
    bottle_link_solo(&host,0,240); bottle_link_solo(&guest,1,240);
    memset(&hwire,0,sizeof(hwire)); memset(&gwire,0,sizeof(gwire)); clock_ms=0;
    expected[0]=240; expected[1]=240;
    bottle_link_host(&host,123,0); bottle_link_scan(&guest,456,0);
    round_trip(false,false); bottle_link_join(&guest,clock_ms); round_trip(false,false);
    bottle_link_accept(&host,clock_ms); round_trip(false,false);
    bottle_link_input(&host,(bottle_input_t){BOTTLE_RECEIVE,0});
    bottle_link_input(&guest,(bottle_input_t){BOTTLE_RECEIVE,0}); round_trip(false,false);
    bottle_link_input(&host,(bottle_input_t){BOTTLE_POUR,6});
    for(unsigned i=0;i<100;i++) round_trip(false,false);
    assert(host.liquid[1].part[0]==240 && host.liquid[1].part[1]==240);
    bottle_link_input(&host,(bottle_input_t){BOTTLE_RECEIVE,0}); bottle_link_input(&guest,(bottle_input_t){BOTTLE_POUR,6});
    for(unsigned i=0;i<30;i++) round_trip(false,false);
    assert(host.liquid[0].part[0]>0 && host.liquid[0].part[1]>0);
    assert(!memcmp(host.liquid,guest.liquid,sizeof(host.liquid)));
}
int main(void)
{
    quantities(); poses(); transfer_and_loss(); packets(); cancellation(); mixing(); ordering_and_expiry();
    puts("PASS: conserved ingredients, bidirectional emptying, capacity, lossy acknowledgements, stale peers, recovery and pause.");
}
