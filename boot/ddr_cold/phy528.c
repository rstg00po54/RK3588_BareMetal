/* Independent fixed LPDDR4X 528 MHz first-probe PHY profile.
 * Derived from v1.18 32a0 / 6214 / 6aac. Requires initial controller
 * profile, original electrical parameters, FSP0 and probe geometry.
 * No physical IO backend. A failure leaves partial state; cold restart. */
#include "cold_steps.h"
static int valid(const struct cold_io *i,unsigned ch){return i&&i->read&&i->write&&i->delay_us&&ch<4;}
static uint32_t rd(const struct cold_io *i,uintptr_t a){return i->read(i->ctx,a);}
static void wr(const struct cold_io *i,uintptr_t a,uint32_t v){i->write(i->ctx,a,v);}
static void rmw(const struct cold_io *i,uintptr_t a,uint32_t keep,uint32_t bits){wr(i,a,(rd(i,a)&keep)|bits);}
static int commit(const struct cold_io *i,uintptr_t c,unsigned polls){
    wr(i,c+0xc80,1);
    while(polls--){if(rd(i,c+0xc84)&1)return COLD_OK;if(polls)i->delay_us(i->ctx,1);}
    return COLD_TIMEOUT;
}
int cold_phy528_electrical(const struct cold_io *i,unsigned ch,unsigned polls){
    if(!valid(i,ch)||!polls)return COLD_BAD_ARG;
    uintptr_t p=0xfe0c0000UL+ch*0x10000UL,b=0xf7000000UL+ch*0x1000000UL,c=b+0x10000;
    rmw(i,p+0x3c8,0x08ffffff,0xf7000000);
    rmw(i,p+0x3d4,0xffc0c0c0,0x003f3f3f);
    rmw(i,p+0x3dc,0xffffffff,0x073f0000);
    rmw(i,p+0x3e0,0xffffc7c7,0x3030);
    rmw(i,p+0x3e0,0xfffffdfd,0);
    rmw(i,p+0xc,0xbfffffff,0);
    rmw(i,p+0x3ec,0xffffc0c0,0xa0a);
    wr(i,c+0xc80,0);
    rmw(i,b+0x508,0xffff0000,0x5c);
    rmw(i,b+0x50c,0xffff0000,0x5c);
    if(commit(i,c,polls))return COLD_TIMEOUT;
    uint32_t mr1=((rd(i,b+0x504)>>16)&0xffffffc6)|0x31;
    uint32_t mr22=((rd(i,b+0x50c)>>16)&0xfffffff8)|6;
    wr(i,c+0xc80,0);
    rmw(i,b+0x504,0xffff,mr1<<16);
    rmw(i,b+0x508,0xffff,0);
    rmw(i,b+0x50c,0xffff,mr22<<16);
    return commit(i,c,polls);
}
int cold_phy528_timing(const struct cold_io *i,unsigned ch){
    if(!valid(i,ch))return COLD_BAD_ARG;
    uintptr_t p=0xfe0c0000UL+ch*0x10000UL,b=0xf7000000UL+ch*0x1000000UL;
    wr(i,p+0xa2c,0x00020146);wr(i,p+0xa30,0x67c28808);wr(i,p+0xa34,0x01818280);
    uint32_t v=rd(i,b+0x500);
    rmw(i,p+0xa80,0xffffff00,v>>16);
    rmw(i,p+0xa80,0xffff00ff,(v&0xffff)<<8);
    v=rd(i,b+0x508);
    rmw(i,p+0xa80,0xff00ffff,v&0xffff0000);
    rmw(i,p+0xa80,0x00ffffff,v<<24);
    v=rd(i,b+0x504);rmw(i,p+0xa84,0xffffff00,v&0xffff);
    wr(i,p+0xa38,0x80);
    rmw(i,p,0xffffc0c0,0x100a);
    rmw(i,p+0xa1c,0xffffffc0,0xa);
    v=rd(i,b+0x504);rmw(i,p,0xf7ffffff,((v>>17)&1)<<27);
    rmw(i,p+0xb8,0xfffff000,0x210);
    rmw(i,p+0xb0,0xfff800fe,0x2ef01);
    rmw(i,p+4,0xcfffffff,0x30000000);
    wr(i,p+0xc,0x04050300);
    rmw(i,p+0x14,0xffbfffff,0);
    rmw(i,p,0xfbff7fbf,0);
    rmw(i,p+0x18,0xfff3b9bc,0xc4440);
    rmw(i,p+0x10,0xfffff7ff,0);
    rmw(i,p+0x400,0xffffff00,0);
    rmw(i,p+0xa1c,0xfffbffff,0);
    return COLD_OK;
}
