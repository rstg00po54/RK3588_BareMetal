# 1848 MHz PHY control, v0.8

Same payload and read allowlist as the 528 MHz experiment. All four commands completed. All 96 register words nonzero (some subfields zero).

| PHY offset | CH0 | CH1 | CH2 | CH3 |
|---|---|---|---|---|
| 00B0 | 2002EF7E | 2002EF7E | 2002EF7E | 2002EF7E |
| 00B4 | 243C0124 | 23DC0121 | 23DC0122 | 235C011D |
| 00B8 | 00000738 | 00000738 | 00000738 | 00000738 |
| 0580 | 00350049 | 00460044 | 00340035 | 00400041 |
| 0190 | 0044004A | 00480047 | 0049003E | 0045004E |
| 0614 | 0005000A | 00060007 | 00080000 | 0004000C |
| 019C | 004D0052 | 0051004D | 0048003F | 00510048 |
| 0620 | 000E0012 | 0010000C | 00070000 | 00110008 |
| 01A8 | 004F004D | 0055004A | 00580045 | 00540044 |
| 062C | 000F000C | 0014000A | 00170005 | 00160002 |
| 01B4 | 005D004D | 00520050 | 004D0047 | 004C0046 |
| 0638 | 0020000D | 00110011 | 000C0006 | 000C0004 |
| 01C0 | 00480050 | 00470051 | 00400048 | 004F0045 |
| 0644 | 00070010 | 00060011 | 0000000A | 00100005 |
| 01CC | 0046004E | 0048004B | 004C004A | 0053004D |
| 0650 | 0005000E | 0007000B | 000B000A | 0012000C |
| 01D8 | 003F004C | 00480049 | 004E004C | 0047004F |
| 065C | 0000000E | 00070009 | 000E000D | 000A0010 |
| 01E4 | 0046004C | 0048004A | 00520059 | 00440055 |
| 0668 | 0005000E | 0008000B | 00110018 | 00040016 |
| 018C | 004A004D | 0048004F | 004F004B | 0046004D |
| 0610 | 000C0011 | 000F0019 | 0016000F | 000F000B |
| 0DC0 | 01249249 | 01249249 | 00049249 | 00001248 |
| 0DC4 | 01249049 | 01249209 | 00249209 | 01240001 |

Compared with [528 MHz baseline](ddr528-phy-baseline.md), the 84 training-associated words change from zero to nonzero. DLL count B4[10:0] = 292, 289, 290, 285. B8 numerically matches frequency in MHz in both captures. B0 differs at bit 0; function not yet established. These are post-init readbacks, not a write sequence.

## Static analysis leads
Original v1.18 binary file offsets (not runtime addresses): 0x6598..0x65ac read/modify/write +0xb8 relative to x19, 0x65b4..0x65d4 read/modify/write +0xb0 relative to x19, merging a byte from [x0]. Need establish x19 provenance and caller before attributing these writes to PHY and interpreting the byte. Other stores with identical immediate offsets can refer to unrelated structures.

Next: trace actual PHY configuration and cold-start call graph; verify memory stability separately. Do not replay DLL result or assume zero training-related readbacks imply no training.
