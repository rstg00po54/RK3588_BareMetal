/* Fixed first-cold LPDDR4X 528 MHz fragment from v1.18 2944/17bc.
 * Injected IO only. NOT safe to invoke before the DFI/MR/PHY prerequisites.
 * A timeout leaves partial hardware state; do not retry or continue. */
#include "cold_steps.h"
static uint32_t rd(const struct cold_io *i,uintptr_t a){return i->read(i->ctx,a);}
static void wr(const struct cold_io *i,uintptr_t a,uint32_t v){i->write(i->ctx,a,v);}
static void rmw(const struct cold_io *i,uintptr_t a,uint32_t clear,uint32_t set){wr(i,a,(rd(i,a)&~clear)|set);}
static int wait(const struct cold_io *i,uintptr_t a,uint32_t mask,uint32_t expect,unsigned n){
    while(n--){if((rd(i,a)&mask)==expect)return COLD_OK;if(n)i->delay_us(i->ctx,1);}
    return COLD_TIMEOUT;
}
static int commit(const struct cold_io *i,uintptr_t c,unsigned n){wr(i,c+0xc80,1);return wait(i,c+0xc84,1,1,n);}
int cold_switch528_fragment(const struct cold_io *i,unsigned ch,unsigned polls){
    if(!i || !i->read || !i->write || !i->delay_us || ch>=4 || !polls)return COLD_BAD_ARG;
    uintptr_t c=0xf7010000UL+ch*0x1000000UL;
    uintptr_t p=0xfd800000UL+ch*0x4000UL;
    uintptr_t g=(ch<2?0xfd59c000UL:0xfd59d000UL)+0x130+(ch&1)*4;
    const uintptr_t route=0xfd7c0594UL;
    /* 2944..29c4: exit DFI initialization state before changing clocks. */
    wr(i,c+0xc80,0);rmw(i,c+0x510,1,0);rmw(i,c+0x510,0,0x20);
    if(commit(i,c,polls)||wait(i,c+0x514,1,0,polls))return COLD_TIMEOUT;
    wr(i,g,0x00080000);i->delay_us(i->ctx,1);wr(i,g,0x00030000);
    /* 17bc: original header bitfield selects integer PLL, bank A.
     * 24 MHz input; desired PLL output 1056, VCO 4224, M=88,P=1,S=2. */
    uint32_t saved=rd(i,route);wr(i,route,0x00c00040);
    wr(i,p,0x80008000);wr(i,p+4,0x20002000);
    wr(i,p,0x03ff0058);wr(i,p+4,0x01ff0081);wr(i,p+8,0xffff0000);
    wr(i,p+4,0x20000000);i->delay_us(i->ctx,1);
    if(wait(i,p+0x18,0x8000,0x8000,polls)){
        /* Deliberate departure from vendor: do NOT un-bypass unlocked PLL. */
        wr(i,route,0x00c00000|(saved&0xc0));return COLD_TIMEOUT;
    }
    wr(i,p,0x80000000);(void)rd(i,route);wr(i,route,0x00c00000|(saved&0xc0));
    /* 2a24..2a8c: select A, resume DFI, commit bit0 separately. */
    wr(i,p+0x300,0x00010000);wr(i,p+0xa00,0x00020000);wr(i,g,0x00080008);
    wr(i,c+0xc80,0);rmw(i,c+0x510,0x20,0);
    if(commit(i,c,polls)||wait(i,c+0x514,1,1,polls))return COLD_TIMEOUT;
    wr(i,c+0xc80,0);rmw(i,c+0x510,0,1);
    return commit(i,c,polls);
}
