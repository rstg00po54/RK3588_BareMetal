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

    val("[DDRCLK] channel", ch, 1);
    val("DDRPHY_CRU", base, 8);

    if (!ddr_regs(base, ddr_pll_regs,
                  sizeof(ddr_pll_regs) / sizeof(ddr_pll_regs[0])))
        return;

    puts0("[DDRCLK] snapshot complete\n");
}
