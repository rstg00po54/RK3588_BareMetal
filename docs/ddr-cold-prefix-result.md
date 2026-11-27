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
