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

/* Vendor v1.18 main function 0x9a2c..0x9a54:
 * LDR DDRCTL0 base; LDR STAT at +0x10014; TST #7; B.EQ 0x9a54.
 * The 0xced0 PMU power-cycle sequence is called ONLY if STAT[2:0] != 0.
 * The preceding message string at 0x10c17 is "pd/pu vd_ddr".
 * A truly cold start skips it. Never power-cycle live DDR from this shell.
 */
static void ddr_stage0(void){
    uint32_t stat;
    out("[DDR S0] read-only; vendor cold/warm branch check\n");
    out("DDRCTL0 STAT @ 0xF7010014 = ");
    for(unsigned i=0;i<1000000U;++i)if(ur(0x14)&0x40)break;
    if(!probe32(0xf7010014UL,&stat)){
        out("ABORT; cannot establish vendor branch\n");return;
    }
    usb_hex(stat,8);out("\n");
    if(stat&7){
        out("[DDR S0] STAT[2:0] nonzero; vendor would power-cycle DDR\n");
        out("[DDR S0] REFUSE: independent cold state not established\n");
        return;
    }
    out("[DDR S0] STAT[2:0] = 0; skip PMU power-cycle as vendor does\n");
    /* Read only named PMU2 registers to document the starting state. */
    static const struct ddr_reg pmu2_regs[]={
        {0x110,"BUS_IDLE_SFTCON1"},
        {0x118,"BUS_IDLE_ACK0"},
        {0x120,"BUS_IDLE_ST0"},
        {0x154,"PWR_GATE_SFTCON2"},
        {0x160,"VOL_GATE_CON2"},
        {0x180,"PWR_GATE_ST0"},
        {0x184,"PWR_GATE_ST1"},
        {0x1d8,"QCHANNEL_STATUS"},
    };
    if(!ddr_regs(0xfd8d8000UL,pmu2_regs,sizeof(pmu2_regs)/sizeof(pmu2_regs[0])))return;
    out("[DDR S0] cold branch established; no DDR writes performed\n");
}
