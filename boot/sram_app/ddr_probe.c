/* Read-only DDR cold/partial-state probe, resident monitor ABI 1.
 * Never power-cycle DDR or touch DRAM itself. No static pointer tables
 * or BSS; all strings are PC-relative and the monitor API is passed in.
 * Direct MMIO is limited to previously tested addresses, plus two
 * vendor PHY wrapper registers only after matching the CH0 prefix.
 * An unresponsive bus can still hang the entire EL3 monitor.
 */
#include "../sram/app_abi.h"

/* With MMU disabled, unaligned wide loads/stores can fault even when
 * SCTLR.A is clear. Avoid compiler-generated wide literal copies. */
static void hex32(const struct sram_app_api *api, uint32_t v)
{
    volatile char b[11] __attribute__((aligned(8)));
    b[0]='0';b[1]='x';
    for(unsigned i=0;i<8;++i){
        unsigned n=(v>>(28-4*i))&15;
        b[i+2]=(char)(n<10?'0'+n:'A'+n-10);
    }
    b[10]=0;
    api->puts((const char *)b);
}
static uint32_t rd(const struct sram_app_api *api, uintptr_t a)
{
    api->puts("MMIO ");
    hex32(api,(uint32_t)a);
    api->puts(" = ");
    uint32_t v=*(volatile const uint32_t *)a;
    hex32(api,v);
    api->puts("\n");
    return v;
}
uint32_t app_main(const struct sram_app_api *api)
{
    if(!api || api->abi!=SRAM_APP_ABI || api->size<sizeof(*api))
        return 0x444400ffU;
    api->puts("[DDR APP] read-only CH0 preflight\n");
    uint64_t s;
    __asm__ volatile("mrs %0, sctlr_el3":"=r"(s));
    if(s&5)return 0x444400feU;
    unsigned flags=0;
    for(unsigned i=0;i<4;++i)
        flags|=rd(api,0xfd58a208UL+4*i);
    uint32_t stat=rd(api,0xf7010014UL);
    if(flags || (stat&7)){
        api->puts("[DDR APP] initialized/unknown state; stop\n");
        return 0x44440003U;
    }
    uint32_t idle=rd(api,0xfd8d8110UL);
    uint32_t mode=rd(api,0xfd7c0280UL);
    uint32_t sel=rd(api,0xfd7c0304UL);
    uint32_t reset=rd(api,0xfd7c0a50UL);
    uint32_t phy_reset=rd(api,0xfd800a00UL);
    uint32_t grf=rd(api,0xfd59c130UL);
    uint32_t pll0=rd(api,0xfd800000UL);
    uint32_t pll1=rd(api,0xfd800004UL);
    if(idle || pll0!=0x8073U || pll1!=1U){
        api->puts("[DDR APP] unexpected clock/PMU state; stop\n");
        return 0x44440004U;
    }
    if(mode!=0x104U || sel!=0x165U || reset!=0x1180U ||
       phy_reset!=2U || grf!=0x400U){
        api->puts("[DDR APP] CH0 prefix signature not present; PHY not accessed\n");
        return 0x44440001U;
    }
    api->puts("[DDR APP] CH0 prefix signature matches previous test\n");
    rd(api,0xfe0c00b0UL);
    rd(api,0xfe0c00b4UL);
    rd(api,0xfe0c00b8UL);
    /* These two offsets are accessed by vendor 0x6aac wrapper.
     * They are raw observations, not a configuration recipe. */
    rd(api,0xfe0c0000UL);
    rd(api,0xfe0c0a1cUL);
    api->puts("[DDR APP] snapshot complete; DDR NOT initialized\n");
    return 0x44440002U;
}
