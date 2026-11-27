# DDR v1.18 second training operation and first geometry detection

Scope: LPDDR4X type 8, 528 MHz, first cold probe, not the complete
89dc or main initialization. No target MMIO or DRAM access.

## Second training operation

Main 9f28..9f3c writes MR14 rank mask 3 using B+50c low halfword;
9f40..9f6c invokes 41b8 with operation mask 8 instead of 0x10.
Arguments: H,ch,0,R,0,0,8,1, stack argument ff.
Correction to earlier notes: in 9f8c..9fbc, if the special MR14
read returns 4d for type7 or 5d for type8, it branches to failure
(w26=-1), NOT success. Otherwise it branches to 9f28.
The full main state machine still needs reconstruction.

For 528, PHY+b8 high two bits zero, the operation enters 4dc0:
- PHY+4 set bit26, PHY+14 clear low six bits, PHY+160 clear bit1.
- 462c..4654 sets five pattern registers +98,+9c,+a0,+a4,+a8.
  Values 00ff55aa, ff00aa55, 00ff55aa, ff00aa55, 00005aa5.
- PHY+a24 set bit6; clear bits14/15 then select rank using
  4000<<rank_index (or c000 for rank mask 3); set bit0.
- 4dec waits bit1; original infinite wait; new driver must bound.
- 4e00,4e0c,4e18 clear bit0, bits14/15, bit6 separately.
- 4e20..4e84 reads 10 training words at PHY+1f0+12*i
  for i=0..8, and PHY+254 for i=9. Reads PHY+b4 low11
  bits; halves this value if PHY+b0 bit0 is zero.
- 4e88..4f54 computes per-word fine/coarse delay, duplicates
  fields into paired output banks. For each packed 10-bit low
  and high sample, period L=as above; a=sample+floor(L/2),
  q=a/L, r=a%L. Fine pair = low_r | (high_r<<16).
  Write raw words to +8b0+4*i and +910+4*i, fine words to
  +880+4*i and +8e0+4*i. Coarse quotients are placed in
  3-bit slots in +dd0/+dd8 (low) and +dd4/+ddc (high).
  Original does not explicitly guard period=0 or quotient>7;
  any new implementation should reject these cases.
- 4f5c calls 16c0, clears PHY+10 bit9, sets PHY+14 low bits
  to 2f, clears PHY+4 bit26; clears all temporary output
  registers just listed and +dd0/dd4/dd8/ddc; calls 16c0 again.
  Do not conclude the computed delays are permanent settings.
- 4fc4..4fdc reads PHY+e8 low16 and +ec low2; nonzero => failure,
  zero => restores 3114/3154 and PHY state, returns success.

## Actual local restricted interpreter results

/tmp/emu_second.py derived from the prior saved /tmp/emu_train.py.
First scan ran with scripted MR12 pass window 8..14, then second
operation mask8. Scripted a24 bit1 when bit0 set, b4=100,
raw training sample 01000080 at each of ten locations,
status e8/ec zero (success) or e8=1 (failure).
As before, 16c0 update and UART output are stubbed.
Success: returned 0, total 3688 steps, second phase 240 accesses.
Failure: returned ffffffff, total 3651 steps, 229 accesses.
Both retain synthetic first-phase DFITMG0=07110204.
Example period 256, raw 01000080 -> fine 00800000, both coarse 1.
The failure case is a status injection, not measured PHY failure.
Training code section hashes checked against repository:
41b8..49d0 21ceff52, 49d0..50bc 3a25f3f8.
This extension is not yet integrated into full-blob interpreter.

## First geometry decode (main a06c onward)

Verified local copied words against original repository bytes:
a06c..a380 FNV-1a32 a29c56c0; a6d0..a850 8c29817c.
For each active channel, after training:
a030..a068 chooses compare MR list [12,5,14], type from R+1b4,
sets G+0c=1, calls MRR rank selector 1, MR8.
For type7/8 a080..a0f4:
  G+08=3; G+04=10; G+10=1
  row0=14+ceil(((MR8>>2)&15)/2)
  if MR8[7:6]==1: G+10=0; row0++
  G+18=row0; G+14=MR8.bit2 (a0e0/a6f8)
Then save C+180 and write zero. For freq<=799:
a120..a178 reads MR12/MR5/MR14 from rank masks 1 and 2,
compares each low byte. Any mismatch -> a734 sets G+1c and
G+2c zero and treats rank1 as absent. If all match, read rank
mask2 MR8 at a18c. G+1c same row formula, add 1 when width
code is 1; if width category differs between ranks, reads rank
mask2 MR0 and accepts mismatch only if bit1 set (a1f0/a81c);
otherwise takes error/retry branch. G+10=0 if either rank's
width code is 1. a79c..a7dc updates rank count, saves second
row, copies first row to G+28, restores C+180.
Actual rank count and density depend on board MR results, which
we do NOT yet have. Matching MR bytes alone may be an alias.
Raw field names preserved where TRM meaning uncertain.

For example MR8=10 gives row0=16, width category 1 (G+10),
G+14=0, col=10 and bank bits=3. With a single 16-bit channel
rank and conventional geometry this corresponds to 1 GiB;
four such channels would match earlier vendor 4GB log. This
is only an example MR8, NOT an observed value from this board.

## C reference and host test

boot/ddr_cold/lp4x_geometry.c implements only the decoded low-
frequency raw geometry formula, not the register probing,
retry, address map or capacity detection. Uses successful
MRR values as inputs. Fresh structure starts zero, unlike the
full vendor structure which retains unrelated earlier fields.
Return -2 on width mismatch without MR0.bit1, no output mutation.
No validity filtering of reserved MR8 encodings, deliberately
tests raw arithmetic only.
Actual host test:
  cc -std=c11 -Wall -Wextra -Werror -O2 \
    boot/ddr_cold/test_lp4x_geometry.c -o /tmp/test-lp4x-geometry
  /tmp/test-lp4x-geometry
  LPDDR4X geometry: 65536 MR8 pairs, width consistency and rank candidate tests PASS

Next: follow a230 onward to final parameter regeneration and
address map; integrate guarded cold stages and collect actual
DFI, MR and PHY status. No board operations requested this turn.
