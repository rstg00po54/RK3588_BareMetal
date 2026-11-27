/* First 89dc(H,R,0) for original type8/FSP0/528 MHz probe configuration.
 * Host-only sequencing. Do not attach to physical MMIO yet. */
#include "first528.h"
int cold_ctl528_profile(const struct cold_io *,unsigned,unsigned);
int cold_phy528_electrical(const struct cold_io *,unsigned,unsigned);
int cold_phy528_timing(const struct cold_io *,unsigned);
int cold_dfi_start_fragment(const struct cold_io *,unsigned,unsigned);
int cold_dfi_complete_fragment(const struct cold_io *,unsigned,unsigned);
int cold_mrw_fragment(const struct cold_io *,unsigned,unsigned,unsigned,unsigned,unsigned);
int cold_phy_ack_fragment(const struct cold_io *,unsigned,unsigned);
int cold_switch528_fragment(const struct cold_io *,unsigned,unsigned);
static uint32_t rd(const struct cold_io *i,uintptr_t a){return i->read(i->ctx,a);}
static void wr(const struct cold_io *i,uintptr_t a,uint32_t v){i->write(i->ctx,a,v);}
static void rmw(const struct cold_io *i,uintptr_t a,uint32_t keep,uint32_t bits){wr(i,a,(rd(i,a)&keep)|bits);}
static int wait(const struct cold_io *i,uintptr_t a,uint32_t mask,uint32_t v,unsigned n){
    while(n--){if((rd(i,a)&mask)==v)return 0;if(n)i->delay_us(i->ctx,1);}return COLD_TIMEOUT;
}
static uintptr_t b(unsigned ch){return 0xf7000000UL+ch*0x1000000UL;}
static uintptr_t p(unsigned ch){return 0xfe0c0000UL+ch*0x10000UL;}
static uintptr_t g(unsigned ch){return (ch<2?0xfd59c000UL:0xfd59d000UL)+0x130+(ch&1)*4;}
static uintptr_t reset(unsigned ch){return 0xfd7c0000UL+4*(0x294+ch+ch/2);}
static uint32_t reset_mask(unsigned ch){return 0x1180U>>(4*(ch&1));}
static void prefix(const struct cold_io *i,unsigned ch){
    uintptr_t pc=0xfd800000UL+ch*0x4000UL, r=reset(ch), gr=g(ch), ph=p(ch);
    uint32_t sh=4*(ch&1), bit=0x40U>>sh, m=reset_mask(ch), last=8U>> (3*(ch&1));
    wr(i,pc,0x80008000); /* 17bc, 24 MHz bypass A */
    wr(i,pc+0xa00,0x00160016);i->delay_us(i->ctx,10);
    wr(i,pc+0xa00,0x00100000);wr(i,r,bit|(bit<<16));
    wr(i,gr,0x00080000);wr(i,pc+0xa00,0x00040000);i->delay_us(i->ctx,5);
    wr(i,gr,0x00080008);i->delay_us(i->ctx,5);wr(i,pc+0xa00,0x00020000);
    wr(i,r,m|(m<<16));i->delay_us(i->ctx,5);wr(i,r,bit<<16);i->delay_us(i->ctx,5);
    wr(i,gr,0x00080000);wr(i,pc+0xa00,0x00020002);
    wr(i,ph+0xbf4,9);rmw(i,ph,0xfcf7ffff,0); /* 6180 / 6aac mode1 */
    wr(i,pc+0xa00,0x00020000);wr(i,gr,0x00080008);
    wr(i,r,last|(last<<16));i->delay_us(i->ctx,10);wr(i,r,last<<16);i->delay_us(i->ctx,50);
}
static void map_initial(const struct cold_io *i,unsigned ch){
    /* Original 11530, row4, initial equal row17, two-rank candidate. */
    static const uint32_t words[]={0x3f3f3f07,0x00070707,0x00003f3f,0x1f000000,
        0,0x1f080808,0x08080808,0x08080808,0x08080808,0x00000808};
    uintptr_t a=b(ch)+0x30000;
    wr(i,a+4,words[0]);for(unsigned j=1;j<10;j++)wr(i,a+8+4*j,words[j]);
    rmw(i,a+0x1c,~0U,0x1f000000); /* 91d4, disable row18 */
}
#define ST(s,n) do{out->stage=(s);out->ch=(n);}while(0)
#define TRY(expr) do{int rc=(expr);if(rc){out->rc=rc;return rc;}}while(0)
int cold_first528(const struct cold_io *i,unsigned polls,struct first528_result *out){
    if(!out)return COLD_BAD_ARG;
    *out=(struct first528_result){.stage=F528_NONE,.ch=4,.rc=COLD_BAD_ARG};
    if(!i||!i->read||!i->write||!i->delay_us||!polls)return COLD_BAD_ARG;
    ST(F528_CLOCKS,4);const uintptr_t cru=0xfd7c0000UL;
    uint32_t mode=rd(i,cru+0x280),sel=rd(i,cru+0x304);
    wr(i,cru+0x280,0x03000000);wr(i,cru+0x304,0x0fc009c0);
    for(unsigned ch=0;ch<4;ch++){
        uintptr_t base=b(ch),c=base+0x10000,a=base+0x20000;
        ST(F528_RESET,ch);prefix(i,ch);
        ST(F528_CTL,ch);TRY(cold_ctl528_profile(i,ch,0));
        rmw(i,base+0xd0c,0xff00ffe0,13);rmw(i,c+0x280,~0U,0x80000000);
        rmw(i,a+4,~0U,0x10000);rmw(i,c+8,0xfffffffc,0);
        ST(F528_ELECTRICAL,ch);TRY(cold_phy528_electrical(i,ch,polls));
        ST(F528_MAP,ch);map_initial(i,ch);
        ST(F528_RELEASE,ch);wr(i,reset(ch),reset_mask(ch)<<16);
        rmw(i,c+0xb84,~0U,2);rmw(i,a+0x90,0xfffffffe,0);
        wr(i,c+0xc80,0);rmw(i,c+0x510,0xfffffffe,0);wr(i,c+0xc80,1);
        TRY(wait(i,c+0xc84,1,1,polls));rmw(i,a+0x90,~0U,1);rmw(i,c+0xb84,0xfffffffd,0);
        ST(F528_TIMING,ch);TRY(cold_phy528_timing(i,ch));
    }
    /* 8e7c..8edc: ALL starts must finish before any completion. */
    for(unsigned ch=0;ch<4;ch++){ST(F528_DFI_START,ch);TRY(cold_dfi_start_fragment(i,ch,polls));}
    for(unsigned ch=0;ch<4;ch++){ST(F528_DFI_COMPLETE,ch);TRY(cold_dfi_complete_fragment(i,ch,polls));}
    for(unsigned ch=0;ch<4;ch++){
        uintptr_t base=b(ch),c=base+0x10000;
        ST(F528_NORMAL,ch);TRY(wait(i,c+0x14,7,1,polls));
        ST(F528_MR,ch);uint32_t mr=rd(i,base+0x508);
        TRY(cold_mrw_fragment(i,ch,1,11,(mr>>16)&255,polls));
        TRY(cold_mrw_fragment(i,ch,2,11,(mr>>16)&0x8f,polls));
        TRY(cold_mrw_fragment(i,ch,3,12,mr&255,polls));
        mr=rd(i,base+0x50c);TRY(cold_mrw_fragment(i,ch,3,22,(mr>>16)&255,polls));
        ST(F528_ZQ,ch);rmw(i,c+0x280,~0U,0x80000000);
        TRY(wait(i,c+0xb90,0x10000,0,polls));rmw(i,c+0xb8c,~0U,0x10000);
        TRY(wait(i,c+0xb90,0x10000,0,polls));rmw(i,c+0x280,0x7fffffff,0);
        ST(F528_PHY_ACK,ch);TRY(cold_phy_ack_fragment(i,ch,polls));
        ST(F528_SWITCH,ch);TRY(cold_switch528_fragment(i,ch,polls));
    }
    ST(F528_RESTORE,4);wr(i,cru+0x280,mode|0xffff0000U);wr(i,cru+0x304,sel|0xffff0000U);
    for(unsigned ch=0;ch<4;ch++){
        uintptr_t gr=g(ch),ph=p(ch);ST(F528_PHY_READY,ch);
        wr(i,gr,0x00200020);wr(i,gr,0x00100010);rmw(i,ph+0xb0,0xffffff9f,0x20);
        rmw(i,ph+0xb0,~0U,0x40);TRY(wait(i,ph+0xb4,0x40000,0x40000,polls));
        wr(i,gr,0x00100000);wr(i,gr,0x00200000);
    }
    ST(F528_DONE,4);out->rc=0;return 0;
}
