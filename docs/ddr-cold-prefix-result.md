# SRAM v0.5.4 CH0 cold prefix result

User's target log: stage0 STAT=0, PMU power cycle skipped. Stage1
completed every write, including restoration of MODE_CON=00000104 and
CLKSEL1=00000165. Post-prefix:
- FD7C0A50 = 00001180
- FD800A00 = 00000002
- FD59C130 = 00000400
- USB core reset and RUN reached; repeated bus reset/connect observed.
  Final configured or host ping was not present in the supplied capture.

This validates MMIO completion and clock restoration, not measured
24 MHz, PHY readiness, reset semantics, or functioning DRAM. It is an
isolated prefix and not an exact complete vendor call (global clock
fields restored before the PHY routine here).

## Next function branch (vendor v1.18 SHA1
8d5e55ebf63d94618b448b79757b1f07733fd076)

At 0x8b48 w4=4; 0x8b5c calls 0x6aac. w3 is the caller's selected
mode value. 0x6ab8 compares w3 with 1; 0x6ac8 branches to 0x6b14
if not 1. For the cold prefix call, confirm w3's actual value from the
caller rather than assuming the branch based on w4.
- mode 1: configuration pointer from [x2+0x228], type [ptr+0x28],
  calls 0x6180 with w3=0, then RMW PHY+0 with mask 0xfcf7ffff
  and bits 25,19 set for type==9.
- other modes: w5=w4, w3=w4=0, calls 0x6214; afterward clears bit18
  in PHY+0xa1c (0x6b24..0x6b2c).
- 0x6214 consumes configuration pointers at x2+0x228 and x2+0x230,
  uses type at first pointer+0x28 and frequency at second pointer+0.
  It has conditional writes based on geometry, frequency, memory type,
  mode and configuration table entries. A full register replay is not
  justified by the post-init baseline.
- 0x6180 also uses the configuration pointer and memory-type branch;
  its output is not a universal constant.

Next: confirm USB ping after prefix and capture ddrclk0 then ddrphy0
on the current partial state. No new write stage until input parameters
and clock/reset prerequisites for the PHY routine are reconstructed.
An inaccessible cold PHY can cause a bus hang despite probe32.


## Monitor v0.6.1 hardware follow-up

After strict-alignment C app fix (4bf546f), cold preflight returned
44440001 (CRU_SOFTRST=40, PHY_CRU_RESET=6). Stage1 repeated successfully,
USB configured and ping passed. Independent 1168-byte DDR app returned
44440002 on the same USB handle in 16 ms total; CRC32=860d3367.
CH0 PHY raw B0=2000017E, B4=00200100, B8=00002640,
+000=44005011, +A1C=00020820. DDR is still not initialized.

## Exact first cold PHY branch, corrected disassembly

Re-decoded original vendor bytes with AArch64 llvm-objdump. At 8a40
mov w28,#1; 8b40 mov w3,w28; 8b48 mov w4,#0 (NOT #4);
8b4c mov x2,x20; 8b50 mov w1,w21; 8b54 mov x0,x19;
8b5c bl 6aac. Therefore this first wrapper call selects mode 1.
The earlier w4=4 description above is incorrect.

6ad4 sets w3=0 for 6180. 6198 loads config=[x2+228].
If config+28 != 9, 6200 sets write value 9; otherwise:
lookup=fn3228(9, word[config+2c]);
value=(byte[lookup+12]<<4) | (word[config+60]==128 ? 10 : 12).
61e4..61ec writes value to PHY+BF4 for mode 0.
Then 6ae4..6b04 writes PHY+000 =
(old & 0xfcf7ffff) | (type==9 ? 0x02080000 : 0).
For observed old value 44005011 and type==9, arithmetic prediction
is 46085011, NOT an authorized write sequence. Need establish exact
parameter table values and restore temporary global clock conditions
as required by the original call before advancing writes.
Do not clear PHY+A1C bit18 in this first mode-1 branch; that belongs
to the separate mode!=1 path. Added guarded read-only BF4 sample
to DDR app (bd28e9b); no new register writes.


## BF4 observation and cold memory-type selection

User's second app invocation confirmed PHY+BF4=00000009, PHY+000=
44005011, PHY+A1C=00020820, and unchanged prefix signature. Returned
44440002. No register write is required merely to reproduce those values.

Re-disassembled vendor image at 992c..9c18, 89dc..8c1c, 6180..6214,
3228..32a0, 6aac..6b34 with llvm-objdump. SDK reference
reference/u-boot/arch/arm/include/asm/arch-rockchip/sdram.h defines
LPDDR4=7, LPDDR4X=8, LPDDR5=9. Do not confuse LPDDR4X with type 9.

Original unpatched binary: parameter header at 115d0, byte +a=13,
selected block at 11604, word +14=00005100; bits [8:5]=8.
9a58..9ab0 initializes candidate order [7,8,9]; 9ac0..9ad0 changes
it to [8,7,9] for preference 8. On the normal path (not retention
magic 02468ace at 9ae8), 9be4..9be8 selects the candidate.
9c10 sets frequency argument 528; 9c18 calls 6dc4 with candidate in w4.
This establishes the normal first candidate for the unpatched blob;
the exact generated config pointer contents still need checking.

For config type 8, 6180 takes 6200 and writes 9 to PHY+BF4;
6ae4..6b04 clears mask bits 24,25,19 (mask fcf7ffff) in PHY+000.
For observed 44005011 this produces 44005011 unchanged.
Both observed values already equal the predicted type-8 results.
That does not prove that the writes can be skipped or that PHY setup
has run; writes may have hardware side effects.

For type 9 only, fn3228 lookup table starts at 11778, stride 0x15.
Frequency buckets <=533, <=800, <=1600, <=2133, >2133 select
byte+12 values [1,1,1,0,0]. At 528, type 9 yields 0x1a or 0x1c
depending on config+60; this is not the LPDDR4X path.

Next original CH0 sequence after 8b5c (derived, not executed):
8b7c PHY_CRU+A00 write 00020000;
8b8c DDR_GRF+130 write 00080008;
8ba0 CRU+A50 write 00080008; delay 10 us;
8bb8 CRU+A50 write 00080000; delay 50 us;
8bd8 calls 5e38 with w3=0 on initial call.
5e38 is a substantially longer config-dependent routine; it is NOT
appropriate to replay from a trained register snapshot.
Original 89dc has temporary global clock fields active across this
sequence, unlike our isolated stage1. Retain current partial state
until a complete guarded continuation is built; do not run stage1 again.
No new firmware or register writes in this analysis.
