# Offline v1.18 LPDDR4X cold generator and DDRCTL trace

## Scope and verification

Input: original 75320-byte DDR v1.18 binary, SHA1
8d5e55ebf63d94618b448b79757b1f07733fd076. This run used a local
restricted interpreter of LLVM disassembly (not Unicorn). The
memset routine at a0c was implemented as a byte-fill shortcut.
All other executed instructions were interpreted. No hardware access.
The 1072 distinct executed instruction words, sorted by address,
have FNV-1a32 b26c2d45, independently recomputed against the repository
binary. Data block 115d0..11828 has FNV-1a32 a1b636de, also matched.
Generator 6dc4 returned 0 after 641 interpreter steps (including
the memset shortcuts). Controller 5e38 returned after 497 additional
steps, 53 synthetic writes and 15 reads. Controller initial MMIO
values were zero, not actual cold reset values; RMW writes below
cannot be copied blindly. No training, physical memory or actual
controller state machine is emulated.

## Corrections to static analysis

* Initial geometry words (decimal) at every channel:
  2,11,3,1,1,0,17,17,0,0,0,0,0,0,0.
  G+10 is 1, not 0. The remainder of the zeroed 0x6c block is zero.
* P+10 is 16 (template+20 = 0x10, shift by 4 then 8<<1).
  Therefore T+1f4 is 10, not 12.
* C+000 width expression in 5ef8..5f04 is
  P[34]==16 ? 0x80000 : 0x40000, opposite of earlier prose.
  Actual simulated C+000=03080002.
* H.f0=1,H.f4=1,H.f8=0, H.e8=0,H.ec=13. R+1b0=528,
  R+1b4=8,R+1b8=4,R+228=P,R+230=T.
* Oracle geometry assertion corrected in 99e3b3d. Unicorn itself
  is not installed in the current environment and has not run.

## Generated P and T blocks

All values below hex. Unlisted words zero. P 148 bytes; T 684 bytes.
These are reference data for fixed 528 MHz first LPDDR4X candidate,
not a general DDR timing generator. T includes postprocessing
by 6dc4 at 7140..717c.

```
P
000:15 004:2 008:80000000 00c:80000000 010:10
028:8 02c:210 030:1 034:10 04c:28 088:1

T
000:210 004:19c80 008:6 00c:101d00
010:3 014:420 018:19c80 01c:101d00
020:80c 024:a 028:a 02c:c 030:a 038:8 03c:23
040:21 044:6 048:8 04c:8 054:486c 058:17 05c:16
060:c9 064:65 06c:2 070:cd 074:cd 078:7 084:6
09c:a 0a0:8 0a4:d 0c8:1 0d0:2 0d4:1 0d8:2
100:1b 104:28 108:b 10c:14 114:84 124:b 128:1
134:14 138:9 13c:30 16c:20 170:30 174:4 178:3
17c:8 180:3 184:1 188:7 18c:1 190:7 194:a
198:210 19c:10 1a0:3 1a4:84 1a8:35 1ac:6a
1b0:35 1b4:84 1b8:4 1bc:4 1c0:3 1c4:7
1e8:a 1ec:6 1f0:10 1f4:a
```

## Full first-call synthetic DDRCTL trace

Original instruction offsets and addresses below. R = read, W = write.
RMW source reads all zero except the repeat at F7010510. Values are
not a hardware script. First 29 writes are timing setup inside 53e4.

```
W pc=05488 addr=f7000000 value=19161217
W pc=05498 addr=f7000018 value=00000009
W pc=054b8 addr=f7000600 value=82100101
W pc=054c4 addr=f7000604 value=000000c9
W pc=054d0 addr=f7000b80 value=00020000
W pc=054e4 addr=f7000588 value=00000204
R pc=054ec addr=f70005a4 value=00000000
W pc=054fc addr=f70005a4 value=00000801
W pc=0550c addr=f70005a8 value=00500003
W pc=05568 addr=f7000b04 value=0b180d07
W pc=0558c addr=f7000b08 value=00000025
W pc=055a4 addr=f7000b00 value=05417580
W pc=055c4 addr=f7000d04 value=00000808
W pc=0560c addr=f7000004 value=00070821
W pc=05678 addr=f7000008 value=060a1817
W pc=056ac addr=f7000d08 value=0000201f
W pc=0571c addr=f700000c value=000a1c21
W pc=0574c addr=f7000010 value=0a08060a
W pc=05784 addr=f7000014 value=08070806
W pc=057a0 addr=f7000030 value=00030000
W pc=057c0 addr=f7000034 value=00200004
W pc=057ec addr=f7000038 value=001600cd
W pc=0581c addr=f7000064 value=00041908
W pc=0582c addr=f7000608 value=00300000
W pc=05858 addr=f7000500 value=00140009
W pc=0586c addr=f7000504 value=00300000
W pc=05880 addr=f7000508 value=00000000
W pc=05894 addr=f700050c value=00000000
W pc=058b8 addr=f7000580 value=07190204
W pc=058c8 addr=f7000800 value=00100210
R pc=05ec4 addr=f7010280 value=00000000
W pc=05ecc addr=f7010280 value=20000000
W pc=05f34 addr=f7010000 value=03080002
W pc=05f54 addr=f7010200 value=00000136
W pc=05f5c addr=f7010288 value=00000001
W pc=05f64 addr=f7010508 value=80008000
W pc=05f88 addr=f7010c94 value=00000001
W pc=05fb8 addr=f7010d00 value=00020030
W pc=05fd4 addr=f7010d04 value=00060000
R pc=05fd8 addr=f7010380 value=00000000
W pc=05fe4 addr=f7010380 value=00000000
R pc=06034 addr=f7010394 value=00000000
W pc=06044 addr=f7010394 value=00000000
R pc=06048 addr=f7010c90 value=00000000
W pc=06054 addr=f7010c90 value=0000f000
R pc=06058 addr=f7010184 value=00000000
W pc=06060 addr=f7010184 value=00000001
R pc=06064 addr=f7010500 value=00000000
W pc=06078 addr=f7010500 value=00100011
W pc=0607c addr=f701018c value=00000000
R pc=06080 addr=f7010510 value=00000000
W pc=06088 addr=f7010510 value=00000004
W pc=06098 addr=f7010104 value=00000001
W pc=0609c addr=f7010108 value=00000001
R pc=060a0 addr=f7010118 value=00000000
W pc=060a8 addr=f7010118 value=00000000
R pc=060ac addr=f7010518 value=00000000
W pc=060b8 addr=f7010518 value=00000001
R pc=060bc addr=f7000804 value=00000000
W pc=060c8 addr=f7000804 value=000f0000
R pc=060cc addr=f7100804 value=00000000
W pc=060d8 addr=f7100804 value=000f0000
R pc=060e4 addr=f7200804 value=00000000
W pc=060f0 addr=f7200804 value=000f0000
R pc=060f4 addr=f7300804 value=00000000
W pc=06100 addr=f7300804 value=000f0000
R pc=06104 addr=f7010510 value=00000004
W pc=0610c addr=f7010510 value=00000004
```

## Comparison with 528 MHz vendor hardware baseline

The first call is pre-detection; baseline docs/ddr528-baseline.md
is post-init. DRAMSET1TMG0..6,12,13,25, INITMR0, DFITMG2,
RFSHSET1TMG0/2, ZQSET1TMG0, and RANKTMG1 match the recorded
528 MHz values. The differences are useful evidence that subsequent
configuration is necessary:
DRAMSET1TMG14=001600cd vs 00160098;
RFSHSET1TMG1=000000c9 vs 0000004a;
INITMR2=00000000 vs 0000005c;
INITMR3=00000000 vs 0006005c;
DFITMG0=07190204 vs 07110204.
Do not interpret agreement as DDR readiness. Next trace 89dc after
8bd8, then its PHY setup and DFI/training calls. Temporary global
clock state still needs to be integrated before any target writes.
