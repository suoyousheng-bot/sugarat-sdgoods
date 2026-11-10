/* Host test for sugarat_https_backoff.h: cc -std=c11 -fsanitize=address,undefined -Iinclude test/host_test.c */
#include <assert.h>
#include <stdio.h>
#include "sugarat_https_backoff.h"

int main(void)
{
    sugarat_https_backoff_t b;sugarat_https_backoff_init(&b,1000,60000);
    assert(sugarat_https_backoff_ready(&b,0));
    sugarat_https_backoff_record(&b,0,false,0,0);            // transport failure: base delay
    assert(!sugarat_https_backoff_ready(&b,999) && sugarat_https_backoff_ready(&b,1000));
    sugarat_https_backoff_record(&b,1000,true,503,0);        // doubles
    assert(sugarat_https_backoff_wait_ms(&b,1000)==2000);
    sugarat_https_backoff_record(&b,3000,true,429,0);        // 429 without Retry-After doubles too
    assert(sugarat_https_backoff_wait_ms(&b,3000)==4000);
    sugarat_https_backoff_record(&b,7000,true,429,90);       // Retry-After wins over the cap
    assert(sugarat_https_backoff_wait_ms(&b,7000)==90000);
    for(int i=0;i<40;i++)sugarat_https_backoff_record(&b,0,false,0,0);
    assert(sugarat_https_backoff_wait_ms(&b,0)==60000);     // capped, no shift overflow
    sugarat_https_backoff_record(&b,0,true,404,0);           // a definite answer clears the failures
    assert(sugarat_https_backoff_ready(&b,0) && b.failures==0);
    sugarat_https_backoff_record(&b,5,true,500,0);
    assert(sugarat_https_backoff_wait_ms(&b,5)==1000);
    puts("PASS backoff base delay, doubling, Retry-After, cap and reset");
    return 0;
}
