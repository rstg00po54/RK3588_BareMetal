# LPDDR4X v1.18 timing generator: 0xd010

Input binary: reference/rkbin/bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin
SHA1 8d5e55ebf63d94618b448b79757b1f07733fd076, size 75320.
All code addresses below are offsets within that binary.
No new target reads or writes were performed.

## 1. Dispatch and exact cold arguments

At 0x9bfc..0x9c18, x0=R, x1=P, x2=T, w3=528,
w4=selected type (8 on normal first candidate), w5=0.
The w5=0 branch in 0x6dc4 is 0x7010..0x7090, not the
w5!=0 path. It initializes the geometry of ALL four channels.
0x6f24 calls 0xd9e4 with w0=0, storing zero at image+0x14bd0.
0x6f30 calls 0xd9f0 with P,T. At d9fc type=P[0x28];
da08/da14 branch for type 8 to da28, which calls 0xd010.
Type 7 also falls through to da28. Type 9 branches to da4c.
The large body at da4c and beyond is LPDDR5-specific and not our
first target. After d010 returns, d9f0 returns zero unconditionally.

## 2. Initial geometry and flags (before probing/training)

Template at 117e8: sixteen little-endian words:
10,2,11,11,0,0,8,b,1,0,15,7,ff012700,0,320,0 (hex).
6dfc replaces template+2c with 8; 6e00 replaces +38 with 528.
7010..7070 fills each 0x6c-byte channel block with the following
words, at offsets 0,4,...,38 respectively (decimal):
2,11,3,1,1,0,17,17,0,0,0,0,0,0,0.
This is initial probe geometry, NOT the final detected 4GB geometry.
R[1b0]=528, R[1b4]=8, R[1b8]=4, R[1bc]=R[1c0]=0.
The initial memset at 9b38 clears the complete 0x358-byte R.
9c38 checks H+100; normal cold path branches directly to 9e14.
9e14..9e24 calls 89dc(H,R,0). The retention-only block at
9c58 onward does not change this normal cold geometry.

Header at 115d0, byte+a=13, selected block at 11604.
Selected block+4=0000000d; +c=0d680000; +14=00005100.
Decoded 9a78..9aa8:
H[e8]=0, H[ec]=13, H[f0]=1 (bit26), H[f4]=1 (bit27),
H[f8]=0 (inverted bit19). These are original-blob values.
Initial nonretention H[fc]=H[100]=0.
Note that H flags are not hypothetical anymore for this input.

## 3. Basic P input and timing bucket

Template+30 is absolute pointer ff012700, which maps to binary
11700 when the image is loaded at ff001000. Bytes at 11700:
10 00 28 f0 f0 01 00 00 00 00 00 00 00 00 00 00.
0x1404 reads that table. At 14a4 P[10]=16 (template+20=0x10, shifted by 4 gives 1).
14ac sets P[28]=8 and P[2c]=528; 14b0 sets P[30]=1.
14b8 sets P[34]=16; 14c0 sets P[38]=0.
14f4 and 14fc initialize P[44]=0 and P[48]=0.
0x6dc4 then applies electrical/frequency adjustments and clears T.
The following early d010 values are independently decoded for this
input; verify the complete final T against the offline oracle.

d010..d088 scans P's 0x14-stride rank blocks to obtain a maximum
capacity field. d094 clears 0x2ac bytes of T.
d0b0 stores T[0]=528; d0b8 T[1e4]=0; d0c0 T[1f0]=16.
d160 compares frequency against 533 (0x215).
d168 reads global image+14bd0, previously cleared by d9e4.
For <=533 and that global <=6:
d178..d1bc take the P[44]==0 branch:
T[138]=9; T[1e8]=10; T[1ec]=6; T[1f4]=10
(2*(5+(P[10]==8))); T[38]=8; T[134]=20
(20 OR (P[34]==32)); T[a8]=T[ac]=0.
The later shared arithmetic starts at d108 and d42c; these are
not yet the complete timing algorithm. 6f40/7140 adds 2 to
T[1c4], T[78], T[188], T[190], T[84] AFTER d010 returns.

## 4. Controller configuration corrections and predictions

For 5e38(H,0,R,0), initial geometry and type predict:
C+000=03080002, C+200=00000136, C+288=1, C+508=80008000.
C=F7010000. H.f0=1 also enables the extra timing and
controller writes described in ddr-controller-5e38.md.
These are static predictions of the first cold call, not measured
hardware or an approved write script. The original 89dc maintains
temporary global clock fields across 5e38; our stage1 restored them.

## 5. Offline oracle

tools/ddr-v118-oracle.c loads only the exact vendor binary in
Unicorn at its original address. Calls 6dc4 with the actual cold
arguments but isolated synthetic R,P,T buffers; verifies return,
type, frequency and decoded initial geometry. Dumps all three
structures. Optional --ctl calls 5e38 with synthetic H and a 4 MiB
zero-initialized virtual CH0 register bank and prints each MMIO
read/write, including instruction offset. Unmapped access aborts.
No actual device access, no physical DDR or flash modifications.
Initial register values are deliberately synthetic, so RMW values
cannot be used as a hardware initialization sequence.

Build on a host with libunicorn-dev and pkg-config:
    make -f tools/ddr-v118-oracle.mk
Run:
    out/host/ddr-v118-oracle reference/rkbin/bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin --ctl

The current analysis environment lacks Unicorn, so that oracle remains
unexecuted. A restricted LLVM-disassembly interpreter was run locally
and completed both 6dc4 and 5e38: see ddr-v118-offline-results.md.
The reusable exact-blob harness is tools/ddr-v118-interpret.py; its
input/disassembly wrapper still needs full-file integration validation.
Next: compare oracle output against independent C timing calculations,
trace the remaining D010 arithmetic and 53e4 bitfield packing,
then follow the first DFI/training sequence. No board action needed.

For the later offline execution results and corrected values, see [ddr-v118-offline-results.md](ddr-v118-offline-results.md).
