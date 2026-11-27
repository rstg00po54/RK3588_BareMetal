/* Read-only preflight using existing SRAM Monitor ABI 1. No MMIO writes. */
#include "../sram/app_abi.h"
#include "../ddr_cold/cold528_guard.h"
static void hex32(const struct sram_app_api *api,uint32_t v){
    volatile char b[11] __attribute__((aligned(8)));b[0]='0';b[1]='x';
    for(unsigned j=0;j<8;j++){unsigned x=(v>>(28-4*j))&15;b[j+2]=(char)(x<10?'0'+x:'A'+x-10);}
    b[10]=0;api->puts((const char*)b);
}
static uint32_t read32(void *ctx,uintptr_t a){
    const struct sram_app_api *api=ctx;
    api->puts("[DDR528 PRE] R ");hex32(api,(uint32_t)a);api->puts(" = ");
    uint32_t v=*(volatile const uint32_t*)a;hex32(api,v);api->puts("\n");return v;
}
uint32_t app_main(const struct sram_app_api *api){
    if(!api||api->abi!=SRAM_APP_ABI||api->size<sizeof(*api)||!api->puts||!api->ticks)return 0x528e00ff;
    uint64_t el,s,hz;__asm__ volatile("mrs %0, CurrentEL; mrs %1, SCTLR_EL3; mrs %2, CNTFRQ_EL0":"=r"(el),"=r"(s),"=r"(hz));
    api->puts("[DDR528 PRE] READ ONLY; NO DDR WRITES\n");
    if(el!=12||(s&5)||hz!=24000000)return 0x528e00fe;
    uint64_t t=api->ticks();unsigned j=0;while(j<10000&&api->ticks()==t)j++;
    if(j==10000)return 0x528e00fd;
    struct cold_io io={(void*)api,read32,0,0};struct cold528_snapshot snap;
    (void)cold528_snapshot(&io,&snap);
    api->puts("[DDR528 PRE] faults = ");hex32(api,snap.faults);api->puts("\n");
    api->puts(snap.faults?"[DDR528 PRE] REFUSE WRITES\n":"[DDR528 PRE] candidate only; fresh cold boot still required\n");
    return 0x528e0000|snap.faults;
}
