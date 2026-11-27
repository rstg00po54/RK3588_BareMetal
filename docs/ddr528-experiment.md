# DDR 528 MHz comparison experiment

v0.6 clock baseline, measured after vendor DDR v1.18:
All four channels: APLL CON0=0x4d CON1=1 CON2=0xffff0000 CON6=0x9c00.
BPLL CON0=0x82 CON1=0x41 CON2=0xffff0000.
BPLL CON6=0xc000 (CH0/1), 0xbc00 (CH2/3).
CLKSEL_CON00=0 in all four channels.
Nominal DDRPLL output calculations (24 MHz reference, doubled reference):
APLL 3696 MHz, BPLL 3120 MHz. Corresponding LPDDR4X clock targets
1848 and 1560 MHz. These are decoded nominal settings, not measured
frequency nor proof that the inactive PLL is running.

Run bash tools/build-ddr528.sh from repo root. It uses the SDK ddrbin_tool
and its template, patches a local copy only (LPDDR4/LPDDR4X F0=528,
boot_fsp=0, dfs_disable=1), checks readback and packages the unchanged
v0.6 payload. If tool output uses different parameter names or rejects
the patch, the script stops; report the output, do not bypass verification.
Reset to MaskROM and use db only. Do not flash.
Capture the vendor startup log, then ddr, ddr0..3, ddrclk0..3 before usbstart.
If it fails, power cycle and use the original v0.6 loader.
The original vendor blob, SDK and v0.6 source are untouched.
This is not an independent DDR implementation. Build and hardware run
not verified by author. See reference/rkbin/tools/ddrbin_tool_user_guide.txt.
