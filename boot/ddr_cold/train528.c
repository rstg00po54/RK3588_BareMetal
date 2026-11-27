/* v1.18 41b8 mask 0x10, with bounded waits and final sample recheck.
 * No physical IO backend. Other training modes are not implemented here. */
#include "train528.h"
int cold_ca_select(unsigned,uint32_t,unsigned *,unsigned *);
int cold_mrr_fragment(const struct cold_io *,unsigned,unsigned,unsigned,unsigned,uint8_t *);
static uint32_t rd(const struct cold_io *i,uintptr_t a){return i->read(i->ctx,a);}
static void wr(const struct cold_io *i,uintptr_t a,uint32_t v){i->write(i->ctx,a,v);}
static void rmw(const struct cold_io *i,uintptr_t a,uint32_t keep,uint32_t bits){wr(i,a,(rd(i,a)&keep)|bits);}
static int commit(const struct cold_io *i,uintptr_t c,unsigned n){
    wr(i,c+0xc80,1);
    while(n--){if(rd(i,c+0xc84)&1)return COLD_OK;if(n)i->delay_us(i->ctx,1);}
    return COLD_TIMEOUT;
}
int cold_train528_ca(const struct cold_io *i,unsigned ch,unsigned polls,struct cold_ca_result *out){
    if(!i||!i->read||!i->write||!i->delay_us||ch>=4||!polls||!out)return COLD_BAD_ARG;
    uintptr_t b=0xf7000000UL+ch*0x1000000UL,c=b+0x10000,p=0xfe0c0000UL+ch*0x10000UL;
    struct cold_ca_result r={0};
    int restore_zq=!(rd(i,c+0x280)&0x80000000U);
    if(restore_zq)rmw(i,c+0x280,0xffffffff,0x80000000);
    rmw(i,c+0x208,0xffffffff,1);wr(i,c+0x208,rd(i,c+0x208)^16);
    rmw(i,p+4,0xffbfffff,0);rmw(i,p+0x50,0xefffffff,0);
    rmw(i,p+0xb0,0xffffffff,0x800000);rmw(i,p+8,0xf80fffff,0x800000);
    r.base=rd(i,p)&63;
    if(r.base<3||r.base>48)return COLD_BAD_ARG;
    for(unsigned d=(r.base&1)?1:2;d<=18;d+=2){
        rmw(i,p+0xa2c,0xfffff83f,d<<6);
        unsigned expected=rd(i,b+0x508)&0xffff;
        wr(i,c+0xc80,0);rmw(i,b+0x580,0xffc0ffff,(r.base-3+d)<<16);
        if(commit(i,c,polls))return COLD_TIMEOUT;
        uint8_t v=0;
        if(cold_mrr_fragment(i,ch,1,12,polls,&v))return COLD_TIMEOUT;
        if(v==expected)r.passing|=1U<<d;
    }
    if(cold_ca_select(r.base,r.passing,&r.selected,&r.dfi))return -3;
    /* Vendor uses extrema; don't select a known failing gap. */
    if(!(r.passing&(1U<<r.selected)))return -4;
    rmw(i,p+0xa2c,0xfffff83f,r.selected<<6);
    rmw(i,p+0x688,0xfffffe1f,0x100);
    wr(i,c+0xc80,0);rmw(i,b+0x580,0xffc0ffff,r.dfi<<16);
    if(commit(i,c,polls))return COLD_TIMEOUT;
    /* Deliberate extra verification before success/restoration. */
    unsigned expected=rd(i,b+0x508)&0xffff;uint8_t v=0;
    if(cold_mrr_fragment(i,ch,1,12,polls,&v))return COLD_TIMEOUT;
    if(v!=expected)return -4;
    rmw(i,p+0xb0,0xff7fffff,0);rmw(i,p+0x160,0xfffffffe,0);
    /* 16c0: pulse PHY+0x50 bit19 for 1 us. */
    rmw(i,p+0x50,0xffffffff,0x80000);i->delay_us(i->ctx,1);rmw(i,p+0x50,0xfff7ffff,0);
    if(restore_zq)rmw(i,c+0x280,0x7fffffff,0);
    rmw(i,c+0x208,0xfffffffe,0);wr(i,c+0x208,rd(i,c+0x208)^16);
    *out=r;return COLD_OK;
}
