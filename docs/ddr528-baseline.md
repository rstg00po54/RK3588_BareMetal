# 528 MHz board baseline (frequency-only experiment, 2026-11-27)

v0.6 shell reached, all four DDRCTL and PHY CRU snapshots completed.
Original DFS and boot FSP retained; only lp4x_freq changed.
This is observed state, not an initialization replay sequence.

OS_REG2/4 32C512C5; OS_REG3/5 30001005.
All 55 non-STAT/DFISTAT DDRCTL samples identical across channels in this capture.
STAT/DFISTAT CH0/1 = 1/1, CH2/3 = 2/7.
Unchanged common register values: see ddr-board-baseline.md.
21 changed configuration names below (CH0 values, hexadecimal):

| Register | 1848 | 528 |
|---|---|---|
| DRAMSET1TMG0_F0 | 3B4A3F4E | 19161217 |
| DRAMSET1TMG1_F0 | 00100E70 | 00070821 |
| DRAMSET1TMG2_F0 | 10202C2C | 060A1817 |
| DRAMSET1TMG3_F0 | 001A4349 | 000A1C21 |
| DRAMSET1TMG4_F0 | 22081322 | 0A08060A |
| DRAMSET1TMG5_F0 | 08101C10 | 08070806 |
| DRAMSET1TMG6_F0 | 00000012 | 00000009 |
| DRAMSET1TMG12_F0 | 00040000 | 00030000 |
| DRAMSET1TMG13_F0 | 1A200004 | 00200004 |
| DRAMSET1TMG14_F0 | 004A0214 | 00160098 |
| DRAMSET1TMG25_F0 | 00043B0E | 00041908 |
| INITMR0_F0 | 006C0036 | 00140009 |
| INITMR2_F0 | 00260024 | 0000005C |
| INITMR3_F0 | 00060006 | 0006005C |
| DFITMG0_F0 | 072D020E | 07110204 |
| DFITMG2_F0 | 0000020E | 00000204 |
| RFSHSET1TMG0_F0 | 82100385 | 82100101 |
| RFSHSET1TMG1_F0 | 00000103 | 0000004A |
| RFSHSET1TMG2_F0 | 00A70000 | 00300000 |
| ZQSET1TMG0_F0 | 00380738 | 00100210 |
| RANKTMG1_F0 | 00003434 | 0000201F |

PLL all channels: APLL CON0=58 CON1=81 CON2=FFFF0000 CON6=A400.
BPLL CON0=82 CON1=41 CON2=FFFF0000; CON6=C000 CH0/1, BC00 CH2/3.
CLKSEL_CON00=0 all channels. Nominal APLL setting corresponds to 528 MHz
DDR clock, not a direct frequency measurement. No stress testing yet.
