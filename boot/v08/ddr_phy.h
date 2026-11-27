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

    val("[DDRPHY] channel", ch, 1);
    val("DDRPHY", base, 8);

    if (!ddr_regs(base, ddr_phy_regs,
                  sizeof(ddr_phy_regs) / sizeof(ddr_phy_regs[0])))
        return;

    puts0("[DDRPHY] snapshot complete; raw values only\n");
}
