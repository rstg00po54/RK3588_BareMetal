/* Host-tested fragments from DDR v1.18. NOT a complete DDR init.
 * No physical MMIO backend. Call only after prerequisites are established.
 */
#include "cold_steps.h"

static int valid(const struct cold_io *io,unsigned ch,unsigned polls)
{
    return io && io->read && io->write && io->delay_us && ch<4 && polls;
}
static uintptr_t ctl(unsigned ch){return 0xf7010000UL+ch*0x1000000UL;}
static uintptr_t phy(unsigned ch){return 0xfe0c0000UL+ch*0x10000UL;}
static uint32_t rd(const struct cold_io *io,uintptr_t a){return io->read(io->ctx,a);}
static void wr(const struct cold_io *io,uintptr_t a,uint32_t v){io->write(io->ctx,a,v);}
static void rmw(const struct cold_io *io,uintptr_t a,uint32_t clear,uint32_t set)
{
    wr(io,a,(rd(io,a)&~clear)|set);
}
static int wait_mask(const struct cold_io *io,uintptr_t a,uint32_t mask,uint32_t expect,unsigned polls)
{
    while(polls--){
        if((rd(io,a)&mask)==expect)return COLD_OK;
        if(polls)io->delay_us(io->ctx,1);
    }
    return COLD_TIMEOUT;
}
/* 0x2730: configuration update acknowledgement. */
static int commit(const struct cold_io *io,uintptr_t c,unsigned polls)
{
    wr(io,c+0xc80,1);
    return wait_mask(io,c+0xc84,1,1,polls);
}
/* 0x9218..9250. One channel only, not the original all-channel loop. */
int cold_dfi_start_fragment(const struct cold_io *io,unsigned ch,unsigned polls)
{
    if(!valid(io,ch,polls))return COLD_BAD_ARG;
    uintptr_t c=ctl(ch);
    rmw(io,phy(ch)+0xb0,0x20,0);
    wr(io,c+0xc80,0);
    rmw(io,c+0x510,0,0x20);
    if(commit(io,c,polls))return COLD_TIMEOUT;
    return wait_mask(io,c+0x514,1,1,polls);
}
/* 0x8ea8..8ed0: called after ALL active channels have acknowledged start. */
int cold_dfi_complete_fragment(const struct cold_io *io,unsigned ch,unsigned polls)
{
    if(!valid(io,ch,polls))return COLD_BAD_ARG;
    uintptr_t c=ctl(ch);
    wr(io,c+0xc80,0);
    rmw(io,c+0x510,0x20,0);
    rmw(io,c+0x510,0,1);
    return commit(io,c,polls);
}
/* 0x2290: rank selector and 8-bit MR data. No MR read support. */
int cold_mrw_fragment(const struct cold_io *io,unsigned ch,unsigned rank_mask,
                      unsigned mr,unsigned data,unsigned polls)
{
    if(!valid(io,ch,polls)||rank_mask<1||rank_mask>3||mr>255||data>255)return COLD_BAD_ARG;
    uintptr_t c=ctl(ch);
    if(wait_mask(io,c+0x90,1,0,polls))return COLD_TIMEOUT;
    wr(io,c+0x80,rank_mask<<4);
    wr(io,c+0x84,(mr<<8)|data);
    rmw(io,c+0x80,0,0x80000000U);
    if(wait_mask(io,c+0x80,0x80000000U,0,polls))return COLD_TIMEOUT;
    return wait_mask(io,c+0x90,1,0,polls);
}
/* 0x9084..90d8: PHY request/ack, raw register names until TRM confirmed. */
int cold_phy_ack_fragment(const struct cold_io *io,unsigned ch,unsigned polls)
{
    if(!valid(io,ch,polls))return COLD_BAD_ARG;
    uintptr_t p=phy(ch);
    rmw(io,p+0x3d0,0xffff,7);
    rmw(io,p+0x3c8,0x0004000c,0x00040004);
    rmw(io,p+0x3c8,0,2);
    if(wait_mask(io,p+0x3cc,1,1,polls))return COLD_TIMEOUT;
    rmw(io,p+0x3c8,0x00040002,0);
    return COLD_OK;
}
/* 0x2dd8: MRR preamble, trigger completion and data-valid wait. */
int cold_mrr_fragment(const struct cold_io *io,unsigned ch,unsigned rank_mask,
                      unsigned mr,unsigned polls,uint8_t *data)
{
    if(!valid(io,ch,polls)||rank_mask<1||rank_mask>3||mr>255||!data)return COLD_BAD_ARG;
    uintptr_t c=ctl(ch);
    wr(io,c+0x80,0x01000030U);
    if(wait_mask(io,c+0x90,1,0,polls))return COLD_TIMEOUT;
    wr(io,c+0x80,(rank_mask<<4)|1);
    wr(io,c+0x84,mr<<8);
    rmw(io,c+0x80,0,0x80000000U);
    if(wait_mask(io,c+0x80,0x80000000U,0,polls))return COLD_TIMEOUT;
    if(wait_mask(io,c+0x90,0x10000,0x10000,polls))return COLD_TIMEOUT;
    *data=(uint8_t)rd(io,c+0x94);
    return COLD_OK;
}
