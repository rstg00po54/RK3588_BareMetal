/* RK3588 DDR diagnostic, read-only, including experimental cold-state probes.
 * Register offsets: RK3588 TRM Part2, section 2.4.1.
 * No PHY scans, MR commands, clock changes, or writes.
 */
struct ddr_reg {
    unsigned offset;
    char name[32];
};

static const struct ddr_reg ddr_core_regs[] = {
    {0x10000, "MSTR0"},
    {0x10008, "MSTR2"},
    {0x10010, "MSTR4"},
    {0x10014, "STAT"},
    {0x10090, "MRSTAT"},
    {0x1011c, "DERATESTAT0"},
    {0x10180, "PWRCTL"},
    {0x10184, "HWLPCTL"},
    {0x1018c, "CLKGATECTL"},
    {0x10200, "RFSHMOD0"},
    {0x10208, "RFSHCTL0"},
    {0x10280, "ZQCTL0"},
    {0x10284, "ZQCTL1"},
    {0x10288, "ZQCTL2"},
    {0x1028c, "ZQSTAT"},
    {0x10304, "DQSOSCSTAT0"},
    {0x10500, "DFILPCFG0"},
    {0x10508, "DFIUPD0"},
    {0x10510, "DFIMISC"},
    {0x10514, "DFISTAT"},
    {0x10518, "DFIPHYMSTR"},
};

static const struct ddr_reg ddr_timing_regs[] = {
    {0x0000, "DRAMSET1TMG0_F0"},
    {0x0004, "DRAMSET1TMG1_F0"},
    {0x0008, "DRAMSET1TMG2_F0"},
    {0x000c, "DRAMSET1TMG3_F0"},
    {0x0010, "DRAMSET1TMG4_F0"},
    {0x0014, "DRAMSET1TMG5_F0"},
    {0x0018, "DRAMSET1TMG6_F0"},
    {0x001c, "DRAMSET1TMG7_F0"},
    {0x0020, "DRAMSET1TMG8_F0"},
    {0x0024, "DRAMSET1TMG9_F0"},
    {0x0030, "DRAMSET1TMG12_F0"},
    {0x0034, "DRAMSET1TMG13_F0"},
    {0x0038, "DRAMSET1TMG14_F0"},
    {0x005c, "DRAMSET1TMG23_F0"},
    {0x0060, "DRAMSET1TMG24_F0"},
    {0x0064, "DRAMSET1TMG25_F0"},
    {0x0078, "DRAMSET1TMG30_F0"},
    {0x0500, "INITMR0_F0"},
    {0x0504, "INITMR1_F0"},
    {0x0508, "INITMR2_F0"},
    {0x050c, "INITMR3_F0"},
    {0x0580, "DFITMG0_F0"},
    {0x0584, "DFITMG1_F0"},
    {0x0588, "DFITMG2_F0"},
    {0x0590, "DFITMG4_F0"},
    {0x0594, "DFITMG5_F0"},
    {0x0600, "RFSHSET1TMG0_F0"},
    {0x0604, "RFSHSET1TMG1_F0"},
    {0x0608, "RFSHSET1TMG2_F0"},
    {0x0610, "RFSHSET1TMG4_F0"},
    {0x0800, "ZQSET1TMG0_F0"},
    {0x0804, "ZQSET1TMG1_F0"},
    {0x0d00, "TMGCFG_F0"},
    {0x0d04, "RANKTMG0_F0"},
    {0x0d08, "RANKTMG1_F0"},
    {0x0d0c, "PWRTMG_F0"},
};

static int ddr_read(const char *name, uintptr_t addr) {
    uint32_t v;

    out(name);
    out(" @ ");
    usb_hex(addr, 8);
    out(" = ");

    /* Flush the address before potentially inaccessible cold-state MMIO. */
    for(unsigned i=0;i<1000000U;++i)if(ur(0x14)&0x40)break;
    if (!probe32(addr, &v)) {
        out("ABORT\n");
        return 0;
    }

    usb_hex(v, 8);
    usb_put('\n');
    return 1;
}

static int ddr_regs(uintptr_t base, const struct ddr_reg *regs,
                    unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        if (!ddr_read(regs[i].name, base + regs[i].offset))
            return 0;
    }
    return 1;
}

static void ddr_info(void) {
    out("[DDR] PMU1_GRF scratch registers; read only\n");

    if (!ddr_read("OS_REG2", 0xfd58a208UL))
        return;
    if (!ddr_read("OS_REG3", 0xfd58a20cUL))
        return;
    if (!ddr_read("OS_REG4", 0xfd58a210UL))
        return;
    ddr_read("OS_REG5", 0xfd58a214UL);

    out("[DDR] Use ddr0/1/2/3 for individual channels\n");
}

static void ddr_channel(unsigned ch) {
    if (ch >= 4)
        return;

    /* Print before each MMIO load so a bus hang identifies the address.
     * probe32 catches synchronous aborts, not an unresponsive bus.
     */
    uintptr_t base = 0xf7000000UL + ch * 0x1000000UL;

    usb_val("[DDR] channel", ch, 1);
    usb_val("DDRCTL", base, 8);
    usb_val("DDRPHY base (not accessed)", 0xfe0c0000UL + ch * 0x10000UL, 8);
    usb_val("DDRPHY_CRU base (not accessed)", 0xfd800000UL + ch * 0x4000UL, 8);

    if (!ddr_regs(base, ddr_core_regs,
                  sizeof(ddr_core_regs) / sizeof(ddr_core_regs[0])))
        return;

    if (!ddr_regs(base, ddr_timing_regs,
                  sizeof(ddr_timing_regs) / sizeof(ddr_timing_regs[0])))
        return;

    out("[DDR] snapshot complete; F0 timing bank is not necessarily active\n");
}

/* Read-only DDR PHY CRU clock snapshot.
 * Offsets independently listed in rk3588-tools/rk3588_telemetry.py.
 * Do not write clock registers while executing from DRAM.
 */
static const struct ddr_reg ddr_pll_regs[] = {
    {0x0000, "APLL_CON0"},
    {0x0004, "APLL_CON1"},
    {0x0008, "APLL_CON2"},
    {0x0018, "APLL_CON6"},
    {0x0020, "BPLL_CON0"},
    {0x0024, "BPLL_CON1"},
    {0x0028, "BPLL_CON2"},
    {0x0038, "BPLL_CON6"},
    {0x0300, "CLKSEL_CON00"},
};

static void ddr_clock(unsigned ch) {
    if (ch >= 4)
        return;

    uintptr_t base = 0xfd800000UL + ch * 0x4000UL;

    usb_val("[DDRCLK] channel", ch, 1);
    usb_val("DDRPHY_CRU", base, 8);

    if (!ddr_regs(base, ddr_pll_regs,
                  sizeof(ddr_pll_regs) / sizeof(ddr_pll_regs[0])))
        return;

    out("[DDRCLK] snapshot complete\n");
}

/* Expanded read-only PHY snapshot based on DDR v1.18 disassembly.
 * File offsets 0x230c..0x2328: channel descriptor +8 is PHY base,
 * then loads at PHY +0xb8, +0xb4 and +0xb0.
 * 0x2454..0x245c: reads PHY +0x580 + rank*0x1e8 in DQS print path.
 * 0x24c4..0x24f0: rank-zero branch, paired reads at 0x190 + 12*i
 * and 0x614 + 12*i, i=0..7; special i=8 branch 0x26e4..0x26ec
 * uses 0x18c and 0x610. 0x2578..0x25ac reads rank-zero 0xdc0/0xdc4.
 * Names are provisional groups, not a verified register bitfield map.
 * Only sample rank 0 here. No scan, writes, or training commands.
 */
static const struct ddr_reg ddr_phy_regs[] = {
    {0x00b0, "PHY_RAW_B0"},
    {0x00b4, "PHY_RAW_B4"},
    {0x00b8, "PHY_RAW_B8"},
    {0x0580, "PHY_RD_DQS_R0_RAW"},
    {0x0190, "PHY_RD_A0_RAW"},
    {0x0614, "PHY_RD_B0_RAW"},
    {0x019c, "PHY_RD_A1_RAW"},
    {0x0620, "PHY_RD_B1_RAW"},
    {0x01a8, "PHY_RD_A2_RAW"},
    {0x062c, "PHY_RD_B2_RAW"},
    {0x01b4, "PHY_RD_A3_RAW"},
    {0x0638, "PHY_RD_B3_RAW"},
    {0x01c0, "PHY_RD_A4_RAW"},
    {0x0644, "PHY_RD_B4_RAW"},
    {0x01cc, "PHY_RD_A5_RAW"},
    {0x0650, "PHY_RD_B5_RAW"},
    {0x01d8, "PHY_RD_A6_RAW"},
    {0x065c, "PHY_RD_B6_RAW"},
    {0x01e4, "PHY_RD_A7_RAW"},
    {0x0668, "PHY_RD_B7_RAW"},
    {0x018c, "PHY_RD_A8_RAW"},
    {0x0610, "PHY_RD_B8_RAW"},
    {0x0dc0, "PHY_WR_R0_LANE0_RAW"},
    {0x0dc4, "PHY_WR_R0_LANE1_RAW"},
};

static void ddr_phy(unsigned ch) {
    if (ch >= 4)
        return;

    uintptr_t base = 0xfe0c0000UL + ch * 0x10000UL;

    usb_val("[DDRPHY] channel", ch, 1);
    usb_val("DDRPHY", base, 8);

    if (!ddr_regs(base, ddr_phy_regs,
                  sizeof(ddr_phy_regs) / sizeof(ddr_phy_regs[0])))
        return;

    out("[DDRPHY] snapshot complete; raw values only\n");
}

/* First-stage status only: no DDR bus accesses, writes, or training.
 * These addresses are taken from the vendor 0xced0 handshake and
 * the established PHY CRU snapshots, not inferred from DDR contents.
 * A successful read is not evidence that DRAM is usable.
 */
static void ddr_preflight(void){
    out("[DDR PRE] read-only; no DDR writes or memory accesses\n");
    usb_val("image",(uintptr_t)_start,8);
    usb_val("SCTLR_EL3",SYS(SCTLR_EL3),16);
    usb_val("CNTFRQ_EL0",SYS(CNTFRQ_EL0),8);
    if(!ddr_read("OS_REG2",0xfd58a208UL) ||
       !ddr_read("OS_REG3",0xfd58a20cUL) ||
       !ddr_read("OS_REG4",0xfd58a210UL) ||
       !ddr_read("OS_REG5",0xfd58a214UL))return;
    if(!ddr_read("HANDSHAKE_118",0xfd8d8118UL) ||
       !ddr_read("HANDSHAKE_120",0xfd8d8120UL) ||
       !ddr_read("HANDSHAKE_184",0xfd8d8184UL))return;
    for(unsigned ch=0;ch<4;++ch){
        uintptr_t b=0xfd800000UL+ch*0x4000UL;
        usb_val("[DDR PRE] channel",ch,1);
        if(!ddr_regs(b,ddr_pll_regs,sizeof(ddr_pll_regs)/sizeof(ddr_pll_regs[0])))return;
    }
    out("[DDR PRE] complete; cold initialization NOT attempted\n");
}

/* First write experiment only: vendor DDR v1.18 file offsets 0xced0..0xcf34.
 * Mirrors cold_global_handshake in boot/ddr_cold/cold_steps.c, with explicit
 * step tracing and abort handling. This is not DDR initialization.
 * The register block's complete semantics are not yet established.
 */
static unsigned ddr_handshake_attempted;
static void ddr_flush(void){
    for(unsigned i=0;i<1000000U;++i)if(ur(0x14)&0x40)break;
}
static void ddr_handshake_write(unsigned off,uint32_t v){
    uintptr_t a=0xfd8d8000UL+off;
    usb_val("[DDR HS] write address",a,8);
    usb_val("[DDR HS] write value",v,8);
    ddr_flush();
    *(volatile uint32_t *)a=v;
    __asm__ volatile("dsb sy" ::: "memory");
    out("[DDR HS] write complete\n");
}
static int ddr_handshake_wait(unsigned off,uint32_t mask,int set){
    uintptr_t a=0xfd8d8000UL+off;
    usb_val("[DDR HS] poll address",a,8);
    usb_val("[DDR HS] mask",mask,8);
    out(set?"[DDR HS] wait any set\n":"[DDR HS] wait all clear\n");
    ddr_flush();
    uint64_t start=SYS(CNTPCT_EL0);
    uint32_t v=0;
    for(unsigned i=0;i<1000000U;++i){
        if(!probe32(a,&v)){out("[DDR HS] read ABORT\n");return 0;}
        if((!!(v&mask))==set){
            usb_val("[DDR HS] match",v,8);return 1;
        }
        if(SYS(CNTPCT_EL0)-start>=2400000ULL)break;
    }
    usb_val("[DDR HS] timeout last",v,8);
    return 0;
}
static int ddr_cold_guard(void){
    uint32_t v,a,b,c;
    /* Scratch flags are not proof of cold state. Also require the observed
     * cold PLL values and handshake signature. Operator must have booted
     * independent CODE471 without any vendor DDR stage since power-on. */
    if(!alternate() || SYS(CurrentEL)!=12 || (SYS(SCTLR_EL3)&5) ||
       !timer_ok || SYS(CNTFRQ_EL0)!=24000000 || !counter_moves())
        return 0;
    for(unsigned i=0;i<4;++i){
        if(!probe32(0xfd58a208UL+4*i,&v) || v)return 0;
    }
    for(unsigned ch=0;ch<4;++ch){
        uintptr_t base=0xfd800000UL+ch*0x4000UL;
        if(!probe32(base,&v) || v!=0x8073U ||
           !probe32(base+4,&v) || v!=1U ||
           !probe32(base+0x20,&v) || v!=0x8073U ||
           !probe32(base+0x24,&v) || v!=1U)return 0;
    }
    if(!probe32(0xfd8d8118UL,&a) || !probe32(0xfd8d8120UL,&b) ||
       !probe32(0xfd8d8184UL,&c))return 0;
    return a==0xf023ffffU && b==0x0023ffffU && c==0;
}
static void ddr_handshake(void){
    out("[DDR HS] experimental cold handshake ONLY; no PLL/PHY/CTL programming\n");
    if(ddr_handshake_attempted){
        out("[DDR HS] already attempted; cold power cycle before retry\n");return;
    }
    if(upload_arm || upload_pending || upload_state==1){
        out("[DDR HS] upload active; refuse\n");return;
    }
    if(!ddr_cold_guard()){
        out("[DDR HS] cold signature mismatch; refuse writes\n");return;
    }
    /* Quiesce our USB DMA before global hardware state changes. */
    if(!usb_owned){
        out("[DDR HS] start our USB first to establish controller ownership\n");return;
    }
    if(!usb_stop_for_jump4()){
        out("[DDR HS] cannot halt USB; refuse writes\n");return;
    }
    ddr_handshake_attempted=1;
    ddr_handshake_write(0x110,0xf000f000U);
    if(!ddr_handshake_wait(0x118,0xf0000000U,1))goto fail;
    if(!ddr_handshake_wait(0x120,0xf0000000U,1))goto fail;
    ddr_handshake_write(0x160,0x00030003U);
    ddr_handshake_write(0x154,0x00030003U);
    if(!ddr_handshake_wait(0x184,3,1))goto fail;
    ddr_handshake_write(0x154,0x00030000U);
    if(!ddr_handshake_wait(0x184,3,0))goto fail;
    ddr_handshake_write(0x160,0x00030000U);
    ddr_handshake_write(0x110,0xf0000000U);
    out("[DDR HS] sequence PASS (DDR NOT initialized)\n");
    if(!ddr_read("HANDSHAKE_118",0xfd8d8118UL) ||
       !ddr_read("HANDSHAKE_120",0xfd8d8120UL) ||
       !ddr_read("HANDSHAKE_184",0xfd8d8184UL))goto fail;
    if(timer_ok && counter_moves()){
        out("[DDR HS] restart USB\n");usbstart4();
    }else out("[DDR HS] counter stalled; USB remains off\n");
    return;
fail:
    out("[DDR HS] STOP: partial hardware state; power cycle before retry\n");
    /* No guessed rollback and no USB restart in unknown state. */
}
