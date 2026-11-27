# RK3588 DDR v1.18: first DDRCTL configuration (0x5e38)

Source: reference/rkbin/bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin,
SHA1 8d5e55ebf63d94618b448b79757b1f07733fd076.
Offsets below are binary offsets, not runtime addresses. Decoded actual
little-endian AArch64 words using clang and llvm-objdump.
Read-only analysis; no target changes.

## 1. First cold call and structure layout

0x8bd8 BL 0x5e38, x0=hardware descriptor table, w1=channel,
x2=runtime configuration, w3=mode (from caller stack+0xac).
Previous CH0 prefix experiment has not reached this call.
At 0x5e70, channel descriptor stride=0x20; first 64-bit member is
DDRCTL base B (CH0 F7000000). 0x5e7c sets C=B+0x10000, so most
writes here are to F701xxxx, NOT PHY FE0Cxxxx.

Use these provisional names, without assigning unsupported register names:
H=hardware descriptor, R=runtime parameters, P=*(R+0x228),
T=*(R+0x230), G=R+channel*0x6c.
P is a 0x94-byte configuration buffer; T is a 0x2ac-byte timing buffer
(0x6e20 and 0x6f1c zero them). G contains channel geometry.
R+0x1b0 holds input frequency; 0x5e9c divides it by 24 (528 -> 22).
H+0xf0 and H+0xf4 are flags; do not assume their values.
At 0x5e8c the raw parameter-header word is read; its bit3 controls
C+0x380 bit2. For original word 0x5100 that bit is zero.

## 2. Parameter generation confirmed

0x6dc4 accepts memory type in w4 and frequency in w3.
Types 7 and 8 select template 0x117e8 (0x6fd0 branches back to 0x6df4);
type 9 selects 0x11828. 0x6dfc sets template+0x2c to type,
0x6e00 sets template+0x38 to frequency. At 0x6e08..14 stores frequency,
P and T pointers into R. 0x6e20 clears P, 0x6e30 calls 0x1404.
0x14ac copies template+0x2c and +0x38 to P+0x28 and +0x2c.
Thus the initial type-8 candidate genuinely reaches P.type=8,
P.frequency=528, subject to normal first-candidate path established
in ddr-cold-prefix-result.md. This is not a guess from final DDR ID.

0x6e38 selects electrical parameters through 0x2060.
0x6e48 looks up frequency-dependent byte fields via 0x3228.
0x6e90 copies table byte9 to P+0x78.
0x7128 copies table byte0x13 to P+0x80, sets P+0x88=1,
P+0x8c=0 for non-type9. 0x6f1c clears T, 0x6f24 calls 0xd9e4(0),
0x6f30 calls 0xd9f0(P,T). On success, type!=9 branch 0x7140
adds 2 to T offsets 0x1c4,0x78,0x188,0x190,0x84.
Do not replace T with trained controller snapshots.
Remaining work: full 0xd9f0 type-8 timing generator and geometry
initialization; exact electrical table fields and H flags.

## 3. 0x5e38 control flow

First 0x5ea4 calls 0x53e4(H,ch,R,0). It writes timing bank 0
(the bank offset is mode << 20 in 0x5424).
For type 8, 0x542c jumps to 0x58fc; first timing word:
  B+0 = ((T[0x1ec]+T[0x30]+(T[0x1f0]>>1)+1)<<24)
        | ((T[0x54]>>10)<<8) | (T[0x5c]<<16) | T[0x58].
  B+0x18 = T[0x78]+2.
  B+0x600 = H.f4 ? (0x82100000 | (T[0x20]>>3))
                         : (0x02100000 | ((T[0x20]+31)>>5)).
  B+0x604 = T[0x60].
  B+0xb80 = 0x20000.
  B+0x588 = (T[0x1ec]-2)|0x200.
  B+0x5a4 = (old & 0xffffe0e0)|0x801.
  B+0x5a8 = 0x00500003.
  Additional timing operations depend on H.f0 and type; type 7/8
  enters 0x55b8. This is only a partial trace of 0x53e4.
  T[offset] here denotes a 32-bit word, not array indexing by words.

After 0x53e4 returns, first mode=0 and type!=9 sets C+0x280 bit29.
C+0x000 uses rank/geometry flags, not just memory type:
  v = G[0]==2 ? 0x03000000 : 0x01000000;
  v |= P[0x34]==16 ? 0x00040000 : 0x00080000;
  if (G[0xc]==0) v |= 0x1000;
  if (P[0x28] == 7 || P[0x28] == 8) v |= 2;
  else if (P[0x28] == 9) v |= 8;
  write(C+0x000,v);
Do not assume final rank/width equals initial probe geometry.

## 4. Main controller writes, type 8, in instruction order

W = full 32-bit write; RMW = preserve other bits. Not a runnable script.
The earlier 0x53e4 timing writes must precede this list.

| Instruction | C offset | operation |
|---|---|---|
| 5ecc | 280 | RMW set bit29 for first mode |
| 5f34 | 000 | geometry expression above |
| 5f54 | 200 | W H.f4 ? 0x136 : 0 |
| 5f5c | 288 | W 1 |
| 5f64 | 508 | W 0x80008000 |
| 5f88 | c94 | W (P[44] ? 5 : 1) OR (P[48] ? 2 : 0) |
| 5fb8 | d00 | W ((q(T[14])+1)<<16) OR (q(T[0c])+1) |
| 5fd4 | d04 | W (q(T[18])+1)<<16 |
| 5fe4 | 380 | RMW (old & ~4) OR (((header_word>>3)&1)<<2) |
| 6044 | 394 | RMW old & 0xcfffffff |
| 6054 | c90 | RMW (old & 0xff000fff) OR 0xf000 |
| 6060 | 184 | RMW set bit0 |
| 6078 | 500 | RMW (old & 0xffeefeee) OR 0x00100011 |
| 607c | 18c | W 0 |
| 6088 | 510 | RMW set bit2 |
| 6098/609c | 104/108 | W 1 only if H.f0 nonzero |
| 60a8 | 118 | RMW clear bit0 only if H.f0 nonzero |
| 60b8 | 518 | RMW set bit0 |
| 60c8..6100 | B-relative 804 + n*0x100000 | RMW (old & 0xfff00000) OR 0x000f0000, n=0..3 |
| 610c | 510 | RMW set bit2 again |

q(t) = ((t / (R.freq / 24)) + 1023) >> 10, unsigned integer arithmetic.
At 528, divisor=22; do not supply unverified t values.
LPDDR5-only writes at 5ff4..6030 are skipped.
Mode zero returns at 0x6164..617c; nonzero mode tail-calls 0x5e30.

## 5. Next steps / boundaries

Current hardware is still at the verified isolated CH0 reset prefix.
PHY BF4=9, +000=44005011 already match expected type-8 expressions,
but do not establish that original writes ran. Original 89dc maintains
temporary global clocks; our prefix restored them. Do not run the
controller writes above in isolation. First reconstruct the complete
type-8 timing generator and its initial geometry/flags, then evaluate
the resulting register plan on host and compare with vendor baseline.
No independent DDR initialization or training has been proven.
