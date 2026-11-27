/* Host-only reference for v1.18 41b8 mode 0x10, type 8, 528 MHz.
 * Passing mask is measured/scripted MR12 results, not guessed hardware data. */
#include <stdint.h>
#include <stddef.h>
int cold_ca_select(unsigned base,uint32_t passing,unsigned *selected,unsigned *dfi)
{
    if(!selected||!dfi||base<3||base>48)return -1;
    unsigned lo=255,hi=0;
    for(unsigned d=(base&1)?1:2;d<=18;d+=2){
        if(!(passing&(UINT32_C(1)<<d)))continue;
        if(d<lo)lo=d;
        if(d>hi)hi=d;
    }
    if(lo==255)return -2;
    unsigned s=lo+2;
    if(s>hi)s=hi;
    if(s>18)s=18;
    if((s+base)&1)--s;
    *selected=s;*dfi=base-3+s;
    return 0;
}
