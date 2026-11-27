/* v1.18 41b8 mask8, 528 MHz, first probe, FSP0, rank index0.
 * Host-injected IO only. No recovery or physical MMIO backend. */
#include "train528_mode8.h"
static uint32_t rd(const struct cold_io *i,uintptr_t a){return i->read(i->ctx,a);}
static void wr(const struct cold_io *i,uintptr_t a,uint32_t v){i->write(i->ctx,a,v);}
static void rmw(const struct cold_io *i,uintptr_t a,uint32_t keep,uint32_t bits){wr(i,a,(rd(i,a)&keep)|bits);}
static void pulse(const struct cold_io *i,uintptr_t p){
    rmw(i,p+0x50,~0U,0x80000);i->delay_us(i->ctx,1);rmw(i,p+0x50,0xfff7ffff,0);
}
int cold_train528_mode8(const struct cold_io *i,unsigned ch,unsigned polls,struct cold_mode8_result *out){
    if(!i||!i->read||!i->write||!i->delay_us||ch>=4||!polls||!out)return COLD_BAD_ARG;
    uintptr_t c=0xf7010000UL+ch*0x1000000UL,p=0xfe0c0000UL+ch*0x10000UL;
    struct cold_mode8_result r={0};
    int restore=!(rd(i,c+0x280)&0x80000000U);
    if(restore)rmw(i,c+0x280,~0U,0x80000000);
    rmw(i,c+0x208,~0U,1);wr(i,c+0x208,rd(i,c+0x208)^16);
    rmw(i,p+4,0xffbfffff,0);rmw(i,p+0x50,0xefffffff,0);
    rmw(i,p+0xb0,~0U,0x800000);rmw(i,p+8,0xf80fffff,0x800000);
    if(rd(i,p+0xb8)>>30)return COLD_BAD_ARG;
    (void)rd(i,p+4); /* original frequency/bit29 check, constant at 528 */
    rmw(i,p+4,~0U,0x4000000);rmw(i,p+0x14,0xffffffc0,0);rmw(i,p+0x160,0xfffffffd,0);
    const uint32_t patterns[]={0x00ff55aa,0xff00aa55,0x00ff55aa,0xff00aa55,0x5aa5};
    for(unsigned j=0;j<5;j++)wr(i,p+0x98+4*j,patterns[j]);
    rmw(i,p+0xa24,~0U,0x40);rmw(i,p+0xa24,0xffff3fff,0x4000);rmw(i,p+0xa24,~0U,1);
    unsigned n=polls;
    while(!(rd(i,p+0xa24)&2)){if(!--n)return COLD_TIMEOUT;i->delay_us(i->ctx,1);}
    rmw(i,p+0xa24,0xfffffffe,0);rmw(i,p+0xa24,0xffff3fff,0);rmw(i,p+0xa24,0xffffffbf,0);
    if(rd(i,p+0xb8)>>30)return COLD_BAD_ARG;
    uint32_t b0=rd(i,p+0xb0);r.period=rd(i,p+0xb4)&0x7ff;
    if(!(b0&1))r.period/=2;
    if(!r.period)return -3;
    for(unsigned j=0;j<10;j++){
        r.raw[j]=rd(i,p+0x1f0+(j==9?0x64:12*j));
        unsigned a=(r.raw[j]&1023)+r.period/2,b=((r.raw[j]>>16)&1023)+r.period/2;
        unsigned q=a/r.period,t=b/r.period;
        if(q>7||t>7)return -3;
        r.fine[j]=(a%r.period)|((b%r.period)<<16);
        r.coarse_lo|=q<<(3*j);r.coarse_hi|=t<<(3*j);
    }
    rmw(i,p+4,~0U,0x20);rmw(i,p+0x10,~0U,0x200);
    for(unsigned j=0;j<10;j++){
        wr(i,p+0x8b0+4*j,r.raw[j]);wr(i,p+0x910+4*j,r.raw[j]);
        wr(i,p+0x880+4*j,r.fine[j]);wr(i,p+0x8e0+4*j,r.fine[j]);
        unsigned shift=3*j;uint32_t mask=7U<<shift;
        rmw(i,p+0xdd0,~mask,r.coarse_lo&mask);rmw(i,p+0xdd8,~mask,r.coarse_lo&mask);
        rmw(i,p+0xdd4,~mask,r.coarse_hi&mask);rmw(i,p+0xddc,~mask,r.coarse_hi&mask);
    }
    pulse(i,p);rmw(i,p+0x10,0xfffffdff,0);rmw(i,p+0x14,0xffffffc0,0x2f);rmw(i,p+4,0xfbffffff,0);
    for(unsigned j=0;j<10;j++){
        wr(i,p+0x8b0+4*j,0);wr(i,p+0x910+4*j,0);wr(i,p+0x880+4*j,0);wr(i,p+0x8e0+4*j,0);
    }
    wr(i,p+0xdd0,0);wr(i,p+0xdd4,0);wr(i,p+0xdd8,0);wr(i,p+0xddc,0);pulse(i,p);
    if((rd(i,p+0xe8)&0xffff)||(rd(i,p+0xec)&3))return -4;
    rmw(i,p+0xb0,0xff7fffff,0);rmw(i,p+0x160,0xfffffffe,0);pulse(i,p);
    if(restore)rmw(i,c+0x280,0x7fffffff,0);
    rmw(i,c+0x208,0xfffffffe,0);wr(i,c+0x208,rd(i,c+0x208)^16);
    *out=r;return COLD_OK;
}
