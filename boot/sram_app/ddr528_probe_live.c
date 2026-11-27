/* Standalone experimental first-pass and geometry probe diagnostic; no DRAM reads/writes or USB restart.
 * Default build is locked read-only. An exact fresh-cold snapshot must be
 * reviewed and supplied via a generated header before writes are allowed. */
#include "../ddr_cold/probe528.h"
#include "../ddr_cold/cold528_guard.h"
#define SYS(reg) ({uint64_t x;__asm__ volatile("mrs %0, " #reg:"=r"(x));x;})
#define UART 0xfeb50000UL
static uint32_t raw(uintptr_t a){return *(volatile const uint32_t*)a;}
static void tx(char c){for(unsigned j=0;j<1000000;j++)if(raw(UART+0x14)&0x20){*(volatile uint32_t*)UART=(unsigned char)c;return;}}
static void puts1(const char *s){while(*s){if(*s=='\n')tx('\r');tx(*s++);}}
static void hex(uint64_t x){puts1("0x");for(int j=60;j>=0;j-=4)tx("0123456789ABCDEF"[(x>>j)&15]);}
static void val(const char *s,uint64_t x){puts1(s);hex(x);puts1("\n");}
static void flush(void){for(unsigned j=0;j<1000000;j++)if(raw(UART+0x14)&0x40)return;}
static __attribute__((noreturn)) void stop(const char *s){puts1(s);flush();for(;;)__asm__ volatile("wfe");}
void ddr528_live_fatal(uint64_t e,uint64_t l,uint64_t f){val("[DDR528] ESR ",e);val("ELR ",l);val("FAR ",f);stop("[DDR528] FATAL; cold reset required\n");}
struct ctx {struct probe528_result state; uintptr_t last; uint32_t previous; unsigned have,write_enabled;};
/* Restrict IO to the controller, timing-bank, PHY, reset and GRF regions
 * used by the first-pass code; this is not a silicon access guarantee. */
static int allowed(uintptr_t a){
    if(a&3)return 0;
    if((a>=0xfd58a208UL&&a<=0xfd58a214UL)||a==0xfd8d8110UL||a==0xfd7c0280UL||a==0xfd7c0304UL||a==0xfd7c0594UL)return 1;
    for(unsigned ch=0;ch<4;ch++){
        uintptr_t b=0xf7000000UL+ch*0x1000000UL,p=0xfe0c0000UL+ch*0x10000UL,pc=0xfd800000UL+ch*0x4000UL;
        if((a>=b&&a<=b+0x10d04)||(a>=b+0x20000&&a<=b+0x20090)||(a>=b+0x30000&&a<=b+0x3002c))return 1;
        for(unsigned k=1;k<=3;k++)if(a==b+k*0x100000UL+0x804)return 1;
        if((a>=p&&a<=p+0xbf4)||(a>=p+0xdd0&&a<=p+0xddc))return 1;
        if((a>=pc&&a<=pc+0x38)||a==pc+0x300||a==pc+0xa00)return 1;
        if(a==0xfd7c0000UL+4*(0x294+ch+ch/2)||a==(ch<2?0xfd59c000UL:0xfd59d000UL)+0x130+(ch&1)*4)return 1;
    }return 0;
}
static uint32_t rd(void *v,uintptr_t a){struct ctx *c=v;if(!allowed(a)){val("[DDR528] invalid read ",a);stop("STOP\n");}
    unsigned same=c->have&&c->last==a;if(!same){val("[DDR528] R ",a);flush();}
    uint32_t x=raw(a);if(!same||x!=c->previous)val(" = ",x);c->last=a;c->previous=x;c->have=1;return x;
}
static void wr(void *v,uintptr_t a,uint32_t x){struct ctx *c=v;if(!c->write_enabled||!allowed(a))stop("[DDR528] WRITE BLOCKED\n");
    if(c->state.stage==P528_FIRST){val("[DDR528] first stage ",c->state.first.stage);val("ch ",c->state.first.ch);}else{val("[DDR528] probe stage ",c->state.stage);val("ch ",c->state.ch);}val("W ",a);val("value ",x);flush();
    *(volatile uint32_t*)a=x;__asm__ volatile("dsb sy":::"memory");c->have=0;
}
static void delay(void *v,unsigned us){(void)v;uint64_t t=SYS(CNTPCT_EL0);for(unsigned j=0;j<1000000;j++)if(SYS(CNTPCT_EL0)-t>=24ULL*us)return;stop("[DDR528] timer stalled\n");}
#ifdef DDR528_COLD_SIGNATURE
#include DDR528_COLD_SIGNATURE
/* Header must define static const struct cold528_snapshot expected_cold. */
static int matches(const struct cold528_snapshot *s){if(s->mode!=expected_cold.mode||s->sel!=expected_cold.sel)return 0;
    for(unsigned i=0;i<4;i++)if(s->reset[i]!=expected_cold.reset[i]||s->phy_reset[i]!=expected_cold.phy_reset[i]||s->grf[i]!=expected_cold.grf[i])return 0;
    return 1;}
#endif
#if defined(DDR528_COLD_SIGNATURE) && defined(DDR528_MR14_DIAG)
/* Diagnostic only after a completed CA and successful but rejected MR14.
 * MRR issues controller commands, never MRW or DRAM address accesses.
 * Stop at the first timeout; never resume initialization. */
int cold_mrr_fragment(const struct cold_io *,unsigned,unsigned,unsigned,unsigned,uint8_t *);
static void mr14_diag(const struct cold_io *io,const struct probe528_result *r){
    static const unsigned mrs[]={14,12,5,8,14,12,14};
    puts1("[DDR528] MR14 DIAG; CH0 rank selector 1; no MRW\n");
    val("CA passing ",r->channel[0].ca.passing);
    val("CA selected ",r->channel[0].ca.selected);
    val("CA DFI ",r->channel[0].ca.dfi);
    for(unsigned j=0;j<sizeof(mrs)/sizeof(mrs[0]);j++){
        uint8_t v=0;
        val("MR index ",mrs[j]);
        int rc=cold_mrr_fragment(io,0,1,mrs[j],1000,&v);
        val("MR read rc ",(uint32_t)rc);
        if(rc)stop("[DDR528] MR14 DIAG timeout; cold reset required\n");
        val("MR value ",v);
    }
    stop("[DDR528] MR14 DIAG complete; original rejection retained; cold reset required\n");
}
#endif
void ddr528_live_main(void){
    puts1("[DDR528 PROBE LIVE] standalone SRAM; USB must be halted by Monitor run\n");
    if(SYS(CurrentEL)!=12||(SYS(SCTLR_EL3)&5)||SYS(CNTFRQ_EL0)!=24000000)stop("[DDR528] unsafe CPU context\n");
    delay(0,1);struct ctx c={0};struct cold_io io={&c,rd,wr,delay};struct cold528_snapshot snap;
    if(cold528_snapshot(&io,&snap)){val("[DDR528] faults ",snap.faults);stop("[DDR528] REFUSE; cold reset required\n");}
#ifndef DDR528_COLD_SIGNATURE
    stop("[DDR528] LOCKED; no reviewed fresh-cold signature\n");
#else
    if(!matches(&snap))stop("[DDR528] signature mismatch; REFUSE\n");
    /* Explicit cold power-cycle is still required: matching registers do not prove it. */
    c.write_enabled=1;int rc=cold_probe528(&io,1000,&c.state);
    val("[DDR528] rc ",(uint32_t)rc);
    val("first stage ",c.state.first.stage);val("first channel ",c.state.first.ch);
    val("probe stage ",c.state.stage);val("probe channel ",c.state.ch);
    val("candidate mask ",c.state.candidate_mask);
#ifdef DDR528_MR14_DIAG
    if(rc==-5 && c.state.stage==P528_MR14_CHECK && c.state.ch==0)
        mr14_diag(&io,&c.state);
#endif
    for(unsigned ch=0;ch<4;ch++){
        if(!(c.state.candidate_mask&(1U<<ch)))continue;
        struct probe528_channel *r=&c.state.channel[ch];
        val("CH ",ch);val("CA passing ",r->ca.passing);
        val("CA selected ",r->ca.selected);val("CA DFI ",r->ca.dfi);
        val("mode8 period ",r->mode8.period);
        val("MR8 rank0 ",r->mr.mr8_rank0);val("rank comparisons ",r->compared);
        for(unsigned j=0;j<sizeof(r->geometry.g)/sizeof(r->geometry.g[0]);j++){
            val("geometry index ",j);val("geometry value ",r->geometry.g[j]);
        }
    }
    stop(rc?"[DDR528] PROBE FAIL; cold reset required\n":"[DDR528] first probe complete; geometry candidates ONLY; NO DRAM ACCESS\n");
#endif
}
