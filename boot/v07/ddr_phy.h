/* Minimal read-only PHY snapshot based on DDR v1.18 disassembly.
 * File offsets 0x230c..0x2328: channel descriptor +8 is PHY base,
 * then loads at PHY +0xb8, +0xb4 and +0xb0.
 * 0x2454..0x245c: reads PHY +0x580 + rank*0x1e8 in DQS print path.
 * Only sample rank 0 here. No scan, writes, or training commands.
 */
static const struct ddr_reg ddr_phy_regs[] = {
    {0x00b0, "PHY_RAW_B0"},
    {0x00b4, "PHY_RAW_B4"},
    {0x00b8, "PHY_RAW_B8"},
    {0x0580, "PHY_RD_DQS_R0_RAW"},
};

static void ddr_phy(unsigned ch) {
    if (ch >= 4)
        return;

    uintptr_t base = 0xfe0c0000UL + ch * 0x10000UL;

    val("[DDRPHY] channel", ch, 1);
    val("DDRPHY", base, 8);

    if (!ddr_regs(base, ddr_phy_regs,
                  sizeof(ddr_phy_regs) / sizeof(ddr_phy_regs[0])))
        return;

    puts0("[DDRPHY] snapshot complete; raw values only\n");
}
