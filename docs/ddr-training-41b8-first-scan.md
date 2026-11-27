# DDR v1.18 first LPDDR4X training/probe scan (41b8 mode 0x10)

Original binary SHA1 8d5e55ebf63d94618b448b79757b1f07733fd076.
Source instruction ranges copied into local assembler and decoded
by clang/llvm-objdump. FNV-1a32 verified against repository bytes:
41b8..49d0=21ceff52, 49d0..50bc=3a25f3f8.
No physical MMIO, no board access, no hardware training claim.

## First call arguments

Main 9e9c: x0=H,w1=channel,w2=0,x3=R,w4=0,w5=0,
w6=0x10,w7=1, ninth argument on caller stack=0xff.
For type9 the mask is 0x18; do not use that path on LPDDR4X.
At 41d0 the ninth argument is read from stack; low byte w23=ff.
w6 is a mask of distinct operations, not a scalar training mode.
For 0x10, bits0,1,2,3 are clear. 4288,4410,4418,44ec select
the delay/MR scan at 4a0c. 4a40 uses w7=1 and w23=ff to enter
the full sweep at 4a50, not the early helper path.

## Preparation and restoration

B=channel DDRCTL base, C=B+10000, PHY=descriptor+8.
4230 calls 3114(B); it sets C+280 bit31 if not set, returns 1
if newly set (0 otherwise), sets C+208 bit0, toggles C+208 bit4.
4254 clears PHY+4 bit22, 4260 clears PHY+50 bit28,
426c sets PHY+b0 bit23, 4280 modifies PHY+8 bits20..26
to 0x800000 for 528 MHz (threshold 1199).
On success 4ff8..5040 clears PHY+b0 bit23, clears PHY+160 bit0,
calls 16c0(PHY) and 3154(B,saved_3114_return).
3154 clears C+280 bit31 only if 3114 originally set it,
clears C+208 bit0, toggles C+208 bit4.
On all-fail original returns -1 at 49b0 without this restoration.
A new driver must report failure and require a cold restart, not
continue to normal DRAM accesses.

## Scan algorithm (LPDDR4X 528, bank 0, rank selector 1)

4aa0 reads base = PHY+000 & 0x3f.
The isolated prior stages produced PHY+000=4000500a, hence base=10.
Global image+14a04 is zero in the tested cold synthetic state.
At 4ac0..4ad0, delta starts at 2 if base even, 1 if base odd.
At 4c3c..4c74 it increases by 2 until delta>18.
For each candidate:
1. PHY+a2c = (old & fffff83f) | (delta<<6) (4b3c..4b48).
2. Expected MR12 = B+508 low halfword (4d50..4d58).
3. At 528 MHz, type!=9: dfi_field=base-3+delta (4d44..4d5c).
4. C+c80=0; B+580=(old & ffc0ffff)|(dfi_field<<16);
   commit via 2730; read rank1 MR12 via 2dd8 (4bf4..4c2c).
5. If MRR result equals expected, update minimum and maximum
   passing delta (4b74..4b94).

If none pass (minimum=255 or maximum=0), return -1.
Otherwise at 4c8c..4cb8:
  chosen=min(minimum+2, maximum,18);
  if ((base+chosen)&1) chosen--;
This is not midpoint or longest contiguous eye; original considers
extrema and may select a gap when the responses are noncontiguous.
A new driver should recheck selected sample before proceeding.
Final update at 4ce4, 4cf4, 4d34:
  PHY+a2c same RMW with chosen
  PHY+688=(old & fffffe1f)|100
  B+580 DFI field = base-3+chosen
  commit via 2730.
4d40 calls 1e48(T,H,ch,0,ff); at frequency<=799, 1e54 returns
0 without executing the more complex training procedure.
Then 44f0 skips bit3 and restoration path returns success.

## Actual restricted offline experiment

Extended local /tmp/emu_elec.py harness with 41b8, helper 3114/3154,
2dd8 and 1e48 code. Prior generator/controller/electrical/PHY
stages are run before each scan. Simulated C+c84=1, MRSTAT data
valid, MR command trigger clears. MR12 values scripted from delta.
16c0 PHY update and 100f4 UART output were explicitly stubbed.
No real PHY effects or MR responses are modeled.

All five cases ran to function return:
  scenario     passes               return      selected   B+580
  window       6,8,10,12,14         0           8          070f0204
  all          2,4,...,18           0           4          070b0204
  single       8                    0           8          070f0204
  match        8,10,12,14           0           10         07110204
  none         none                 ffffffff    (failure)  07190204

Total local interpreter steps for match = 2882, training phase
188 MMIO accesses. The match scenario produces the same DFITMG0
as the original 528 MHz post-init board snapshot. This explains
how training can modify that timing field; it does NOT establish
which candidate window actually passed on the board.
Earlier isolated B+580=07190204. 19 of 21 frequency-dependent
baseline words can now be reproduced with a suitable synthetic
window; remaining timing differences DRAMSET1TMG14 and RFSHSET1TMG1
are not yet explained. Do not call this 19 verified hardware matches.

## Host C reference

boot/ddr_cold/ca_scan.c reproduces candidate selection from a
supplied passing mask, with base 3..48 range guard to avoid field
overflow and invalid geometry. Does NOT operate registers.
test_ca_scan.c tests explicit cases, all fail, invalid base,
noncontiguous windows, and all 512 masks for each of base 9 and 10.
Local native cc -std=c11 -Wall -Wextra -Werror -O2 build and run:
CA scan selection: explicit vectors + 1024 window masks PASS

Remaining: actual controller/PHY sweep integration, mode 0x8 path
at 44f8 after MR14 write, MR8 geometry decode, complete reset/clock
and address-map sequencing, physical DFI and DRAM tests.
The local interpreter extension is not yet integrated into
tools/ddr-v118-interpret.py. Board remains at stage1; do not run
these isolated fragments on physical hardware.
