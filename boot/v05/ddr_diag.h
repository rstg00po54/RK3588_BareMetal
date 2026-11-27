/* RK3588 DDR diagnostic, read-only after vendor DDR init.
 * Register offsets: RK3588 TRM Part2, section 2.4.1.
 * No PHY scans, MR commands, clock changes, or writes.
 */
struct ddr_reg {
    unsigned offset;
    const char *name;
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

    puts0(name);
    puts0(" @ ");
    hex(addr, 8);
    puts0(" = ");

    if (!probe32(addr, &v)) {
        puts0("ABORT\n");
        return 0;
    }

    hex(v, 8);
    put('\n');
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
    puts0("[DDR] PMU1_GRF scratch registers; read only\n");

    if (!ddr_read("OS_REG2", 0xfd58a208UL))
        return;
    if (!ddr_read("OS_REG3", 0xfd58a20cUL))
        return;
    if (!ddr_read("OS_REG4", 0xfd58a210UL))
        return;
    ddr_read("OS_REG5", 0xfd58a214UL);

    puts0("[DDR] Use ddr0/1/2/3 for individual channels\n");
}

static void ddr_channel(unsigned ch) {
    if (ch >= 4)
        return;

    /* Print before each MMIO load so a bus hang identifies the address.
     * probe32 catches synchronous aborts, not an unresponsive bus.
     */
    uintptr_t base = 0xf7000000UL + ch * 0x1000000UL;

    val("[DDR] channel", ch, 1);
    val("DDRCTL", base, 8);
    val("DDRPHY base (not accessed)", 0xfe0c0000UL + ch * 0x10000UL, 8);
    val("DDRPHY_CRU base (not accessed)", 0xfd800000UL + ch * 0x4000UL, 8);

    if (!ddr_regs(base, ddr_core_regs,
                  sizeof(ddr_core_regs) / sizeof(ddr_core_regs[0])))
        return;

    if (!ddr_regs(base, ddr_timing_regs,
                  sizeof(ddr_timing_regs) / sizeof(ddr_timing_regs[0])))
        return;

    puts0("[DDR] snapshot complete; F0 timing bank is not necessarily active\n");
}
