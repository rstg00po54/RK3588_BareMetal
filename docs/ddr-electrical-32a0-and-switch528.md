# RK3588 v1.18 LPDDR4X: electrical configuration and first clock switch

Original binary SHA1 8d5e55ebf63d94618b448b79757b1f07733fd076.
All code addresses are file offsets. No board operation was performed.

## 1. Electrical configuration (32a0)

First call at 8c90 uses H,ch,R,0,0,0. The routine reads the
LPDDR4X electrical block at image+11658 via 2060, and the 528 MHz
frequency row at image+11725 via 3228. Table pointer at 125f0 is
ff012a08, corresponding to image+11a08. Table is 64 unsigned
16-bit entries, increasing 166..699. 32a0 clamps the requested value
then selects the first table entry >= requested value (354c loop).
For this input the request is 249 (0xf9), index 10.
34a0..34d8 computes integer drive-related fields from 240/divisor,
not trained register snapshots. For the selected 30,30,30,40
values: R[1c4,1c8,1cc,1d0]=7,7,7,6.
R[2c8]=10 (CH0); R[2d8]=0.
Exact register field names still provisional.

At 3808/381c, helper 1f88 converts both original table values
500 to MR byte 0x5c in the type-8 branch. First writes the low
halves of B+508 and B+50c; commits. Then adjusts upper halves of
B+504,508,50c and commits again.

## 2. Actual local offline execution

The existing restricted LLVM disassembly interpreter was extended
with actual 32a0 instructions, 1f88, 2730 and table bytes.
Generator 641 steps, DDRCTL 497 steps, electrical 436 steps,
second PHY wrapper 313 steps: total 1887. 1703 distinct instruction
words FNV-1a32 4e98a04d, independently matched repository bytes.
Data table FNV 0aea8583 (11a08..11a88), pointer block FNV 4eed1215
(125f0..12600); independently matched. The interpreter uses partial
code/data segments, not a full-system emulator. Code 3ba729e extends
the full-blob wrapper, which remains integration-untested.

Electrical trace, 16 writes + 16 reads:
```
R pc=03560 addr=fe0c03c8 value=00000000
W pc=03588 addr=fe0c03c8 value=f7000000
R pc=03594 addr=fe0c03d4 value=00000000
W pc=035c4 addr=fe0c03d4 value=003f3f3f
R pc=035cc addr=fe0c03dc value=00000000
W pc=035d4 addr=fe0c03dc value=073f0000
R pc=035dc addr=fe0c03e0 value=00000000
W pc=035f4 addr=fe0c03e0 value=00003030
R pc=035f8 addr=fe0c03e0 value=00003030
W pc=03608 addr=fe0c03e0 value=00003030
R pc=0360c addr=fe0c000c value=00000000
W pc=03614 addr=fe0c000c value=00000000
R pc=03620 addr=fe0c03ec value=00000000
W pc=03630 addr=fe0c03ec value=00000a0a
W pc=038ac addr=f7010c80 value=00000000
R pc=038bc addr=f7000508 value=00000000
W pc=038c8 addr=f7000508 value=0000005c
R pc=038d0 addr=f700050c value=00000000
W pc=038dc addr=f700050c value=0000005c
W pc=02738 addr=f7010c80 value=00000001
R pc=0273c addr=f7010c84 value=00000001
R pc=038f0 addr=f7000504 value=00300000
R pc=03a30 addr=f700050c value=0000005c
W pc=03a78 addr=f7010c80 value=00000000
R pc=03a7c addr=f7000504 value=00300000
W pc=03a88 addr=f7000504 value=00310000
R pc=03a8c addr=f7000508 value=0000005c
W pc=03a98 addr=f7000508 value=0000005c
R pc=03a9c addr=f700050c value=0000005c
W pc=03aa8 addr=f700050c value=0006005c
W pc=02738 addr=f7010c80 value=00000001
R pc=0273c addr=f7010c84 value=00000001
```

Both c84 reads are SCRIPTED acknowledgements. Six PHY prefix
registers are seeded from user observations; other registers initially
zero. Real resets/address mapping and commit side effects are not
modeled. Do not replay as physical writes.

The resulting INITMR2=0000005c, INITMR3=0006005c match the original
post-init 528 MHz observations, explaining two of the previous five
differences. 18 of 21 frequency-related baseline words now match;
DRAMSET1TMG14, RFSHSET1TMG1, DFITMG0 remain different.
Second PHY call now reads updated MR words and produces PHY+a80=
5c000914 instead of 00000914. Other 6214 trace values unchanged.
This supersedes the isolated no-32a0 comparison, not the actual
hardware baseline. No training or physical DDR state is simulated.

## 3. Frequency switch (2944 -> 17bc)

At 90f0, first call has channel, bank=0, requested PLL rate=1056.
105f0 returns 24000000; 17e8 divides by 1000000 -> 24.
1858..18b8 searches S=0..6 such that VCO in 3300..6600.
For 1056, S=2, VCO=4224. First P=1; M is reduced from
floor(6600/48)=137 to 88 so VCO=48*88/1.
Thus predicted CON0=58 (M), CON1=81 (P=1,S=2), consistent with
vendor 528 MHz baseline after bypass removal. PLL request is 1056,
DDR rate argument 528; no direct hardware frequency measurement.

Original header selected block +c=0d680000 has bits [8:1]=0,
so 18f8 branches to 1ac0, then 1a90 for bank A integer path.
CRU+594 saved at 18ec, temporary write 00c00040 at 18f0;
1a90..1a5c sets PLL bypass, programs M/P/S, clears bypass after
lock wait, then 1a60..1a70 restores masked route bits. Original
code proceeds even if the 1000-poll lock wait expires; our code
deliberately returns on timeout without removing PLL bypass.

Decoded CH0 2944 sequence:
- C+c80=0, C+510 clear bit0, set bit5, commit, wait DFISTAT.bit0=0.
- GRF+130=00080000, delay 1us, GRF+130=00030000.
- LPDDR5-only PHY reset write skipped.
- 17bc integer PLL above.
- PHY_CRU+300=00010000, PHY_CRU+a00=00020000.
- GRF+130=00080008.
- C+c80=0, C+510 clear bit5, commit, wait DFISTAT.bit0=1.
- C+c80=0, C+510 set bit0, commit.
Other channels use the same descriptor strides and GRF selection
(ch0/1 fd59c000, ch2/3 fd59d000; offset 130+4*(ch&1)).

## 4. Host-tested C fragment

boot/ddr_cold/freq528.c implements the fixed cold type-8, bank-A
528 path with injected IO, not a complete init. No physical backend.
Mock test built with:
    cc -std=c11 -Wall -Wextra -Werror -O2 boot/ddr_cold/freq528.c \
       boot/ddr_cold/test_freq528.c -o /tmp/test-freq528
    /tmp/test-freq528
Actual local output:
    528 MHz frequency switch: 4 channel address tests + 6 timeout tests PASS
24 writes on success; test covers each of three commit timeouts,
both DFISTAT waits, PLL lock wait, and invalid channel. On PLL
timeout temporary CRU routing is restored without removing PLL bypass.
Partial hardware state would still require cold restart. The previous
DFI/MRW/MRR/PHY mock tests were also rerun and passed.
No change to monitor or actual DDR writes. Next: 41b8 first training
path and complete 89dc ordering; real status/geometry feedback must be
verified on board before claiming working DRAM.
