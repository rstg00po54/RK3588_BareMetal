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
