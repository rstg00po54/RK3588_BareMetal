# RK3588 DDR v1.18: after first DDRCTL configuration

Source: unpatched DDR v1.18 binary in reference/rkbin, SHA1
8d5e55ebf63d94618b448b79757b1f07733fd076. Instruction offsets are
file-relative. Re-decoded actual instruction words with LLVM objdump.
No target execution or hardware writes in this work.

## 1. First loop in 89dc, after 5e38 returns

B=DDRCTL descriptor[0], C=B+10000, A=B+30000, PHY=descriptor[8].
DDR GRF region is H[c8] (channels 0/1) or H[d0] (2/3).
G=R+channel*6c. Original initial geometry rank field=2,
G[18]=G[1c]=17. Normal first LPDDR4X candidate type=8, mode=0.

- 8c08: B+d0c = (old & ff00ffe0) | ((H[e8]&ff)<<16) | (H[ec]&1f).
  With original H flags this inserts 13 into the low five bits.
- 8c14: C+280 |= 80000000.
- 8c20: (B+20000)+4 |= 10000. LPDDR5-only branch skipped.
- 8c7c: C+8 &= fffffffc.
- 8c90: calls 32a0(H,ch,R,0,0,0). Large PHY/electrical configuration
  routine; still needs full branch and output reconstruction.
- 8cd8 -> 91c8: non-type9 address-map row = 4*(!asymmetric_or_single).
  With initial equal rows and G[0]=2, row=4.
  8d20 writes table word to A+4; 8d24 stores row index into G+38.
  Row 4 at 11530, 40 bytes:
  073f3f3f070707003f3f00000000001f000000000808081f08080808080808080808080808080000
  First word little endian = 3f3f3f07. 8d44 copies remaining 36 bytes
  to A+c via 1150. This is initial address map, not final geometry.
- 8d88 -> 91d4: because row field=17, OR 1f into the selected
  5-bit field of A+1c/20; loop until row threshold. 9204 calls
  c6e8 for non-single rank; c6e8 is RET in this exact blob.
- 8ddc calls 175c(H[88],ch,0) to release controller reset.
- 8de8: C+b84 |= 2.
- 8df4: (B+20000)+90 &= ~1.
- 8df8: C+c80=0; 8e04: C+510 &= ~1.
- 8e0c: calls 2730(B) (see below).
- 8e24: (B+20000)+90 |= 1; 8e34: C+b84 &= ~2.
- 8e3c: calls 6aac(H,ch,R,0,0), hence 6214 instead of initial
  mode-1 6180. Full 6214 electrical register setup remains unverified.
  6b24..6b2c clears PHY+a1c bit18 after 6214 returns.
- 8e58..8e64 loops over all active channels; entry 0xf means absent.

## 2. Separate all-channel loops for DFI initialization

This is an essential ordering constraint: do not start and complete
each channel in one call while other active channels are unconfigured.

First loop (8e80 -> 9218..9254):
    PHY+b0 &= ~0x20
    C+c80 = 0
    C+510 (DFIMISC) |= 0x20
    2730(B)
    wait (C+514 (DFISTAT) & 1) == 1
    advance to next channel

Second loop (8e9c..8edc), after all first-loop acknowledgements:
    C+c80 = 0
    C+510 &= ~0x20
    C+510 |= 1
    2730(B)

2730..2744 is a configuration commit:
    C+c80=1; wait (C+c84 & 1)==1.
Original polls indefinitely. C fragments add finite poll bounds
and 1 us delay between unsuccessful reads. These bounds cannot
recover a bus transaction that itself hangs.

## 3. MR operations and initial normal-state check

89dc third loop 8f00:
- 8fb4 polls C+14 STAT[2:0] == 1.
- Types 7/8 and H[100]==0 then issue mode-register commands:
  8ff8: MR11, rank mask as passed in w1 (STAT masked to 1),
        data = upper halfword of B+508, truncated to byte by 2290.
  9010: MR11, rank mask 2, data = same & ~0x70.
  9024: MR12, rank mask 3, data = lower halfword of B+508.
  9048: MR14 only if mode !=0 (not this first cold call).
  9060: MR22, rank mask 3, data = upper halfword of B+50c.
  These data must be read from the configured timing bank at this stage,
  not inferred from earlier zero-initialized synthetic trace alone.
- 906c sets C+280 bit31; 9074 calls 169c(B) then 9080 clears bit31.
  169c waits C+b90 bit16 clear, sets C+b8c bit16, waits clear again.

2290 MR write:
    wait MRSTAT(C+90).bit0 == 0
    C+80=rank_mask<<4
    C+84=(mr<<8)|(data&ff)
    C+80 |= 80000000
    wait C+80.bit31 == 0
    wait MRSTAT.bit0 == 0

2dd8 MR read:
    C+80=01000030
    wait MRSTAT.bit0 == 0
    C+80=(rank_mask<<4)|1
    C+84=mr<<8
    C+80 |= 80000000
    wait C+80.bit31 == 0
    wait MRSTAT.bit16 == 1
    return C+94 & ff
Note original 2de4 preamble write precedes the busy wait.

## 4. PHY handshake and clock switch

9084..90d8 (per channel):
    PHY+3d0=(old & ffff0000)|7
    PHY+3c8=(old & fffbfff3)|00040004
    PHY+3c8 |= 2
    wait PHY+3cc.bit0 == 1
    PHY+3c8 &= fffbfffd
90f0 calls 2944(H,ch,&R[1b4],H[fc],2*R[1b0]).
For first LPDDR4X 528 candidate, requested value is 1056. This
is a PLL routine argument, not a new measured DDR bus frequency.
2944 initially sets DFIMISC bit5 and clears bit0, commits and waits
DFISTAT.bit0 == 0; changes DDR GRF routing and calls 17bc.
2a2c writes PHY_CRU+300; then clears DFIMISC.bit5, commits,
waits DFISTAT.bit0 == 1 and sets bit0 before tail-calling 2730.
Exact PLL parameter selection still needs verification.
9100..9118 restores temporary global clocks via bad8(H,R,0).
911c..91a4 per channel updates PHY+b0 with table byte1, sets bit6,
waits PHY+b4 bit18 set, then writes GRF+130 (for CH0) with 100000
and 200000. 89dc returns 0 only after this loop.

## 5. First detection after 89dc returns

Main 9e24 calls 89dc(H,R,0); nonzero abort path a850.
For LPDDR4X the first 41b8 call at 9e9c uses w6=16
(9f10 branch), w2=w4=w5=0, w7=1, stack argument=255.
41b8 is a larger training/probe routine; precise training meaning
of each mode not established. Do not label it a specific JEDEC training
operation until its body has been decoded.
9f9c performs MRR rank1 MR14; tests result 4d for LPDDR4 or 5d for
LPDDR4X; otherwise follows 9f28 MR14 write and another 41b8 call
at 9f6c with w6=8. Error handling depends on channel and result.
a06c calls MRR rank1 MR8. Type7/8 branch a080..a0f0 updates geometry:
    G+8=3, G+4=10, G+10=1
    G+18=14+((((MR8>>2)&15)+1)>>1)
    if (((MR8>>6)&3)==1) { G+10=0; G+18++; }
    additional bit2 branch controls G+14.
These are instruction-derived provisional geometry field formulas,
not actual observed MR8 data. Follow a6f8 and later code to complete
the geometry decoder and address map.

## 6. Implemented and tested host fragments

boot/ddr_cold/dfi_fragments.c and test_dfi_fragments.c implement the
five isolated primitives (DFI start, complete, MRW, MRR, PHY ack).
Built locally with:
    cc -std=c11 -Wall -Wextra -Werror -O2 \
      boot/ddr_cold/dfi_fragments.c boot/ddr_cold/test_dfi_fragments.c \
      -o /tmp/test-dfi
    /tmp/test-dfi
Actual result: DFI/MRW/MRR/PHY fragments: mock success, timeout and argument tests PASS
MRR test initially failed because the mock inherited MRW's read value
before trigger; corrected mock now preserves the MRR selector.
These are host-only tests using injected register callbacks. No
firmware integration, no claim of DDR/DFI/training hardware success.
Next focus: reconstruct 32a0 and 6214 actual cold paths; decode
41b8 and remaining geometry/second-pass init. Keep the existing board
in its partial prefix state; no new board operations requested.
