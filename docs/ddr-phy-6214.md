# DDR v1.18: isolated second PHY wrapper, LPDDR4X 528

Original binary SHA1 8d5e55ebf63d94618b448b79757b1f07733fd076.
0x6214..0x6aac is 2200 bytes, FNV-1a32 1d86a539. Local copied
bytes independently matched repository bytes after correcting one
transcription error. The executed instruction set across parameter
generation, DDRCTL and this PHY call contains 1375 unique words,
FNV-1a32 59caa160, independently matched against the blob.

## Preconditions and scope

Actual 8e14 sets w3=0, 8e18 loads w4 from caller stack+ac (0 for
first 89dc cold call). 8e3c calls 6aac(H,ch,R,0,0). Wrapper 6b14
sets w5=w4 and w3=w4=0, then calls 6214(H,ch,R,0,0,0).
6214 uses P=R[228], T=R[230], DDRCTL=H[ch*20], PHY=H[ch*20+8].
Type8 branch at 6268 goes to 676c (frequency unchanged); T[0]=528.
Unlike earlier mode1 call, this is the second PHY configuration.

Actual restricted interpreter run:
- 6dc4 returned after 641 steps.
- 5e38 returned after another 497 steps.
- 6aac -> 6214 -> 6aac return after another 313 steps.
- 22 writes, 21 reads in the isolated PHY phase.

IMPORTANT: this is NOT a replay of full 89dc. Prior 32a0, reset
release, address-map and commit side effects have not been emulated.
DDRCTL values come from first simulated 5e38, and six PHY words
come from the user's measured stage1 prefix; other words zero.
Thus results are useful for formulas and order, not as an exact
hardware register script. In particular 6214 uses R+1d5..1df
electrical fields in other branches; 32a0 must be understood first.

## Isolated trace (addresses relative to PHY, unless stated)

```
W 631c a2c 00020146
W 6364 a30 67c28808
W 63a0 a34 01818280
R 6868 F7000500 00140009
R 686c a80 00000000
W 6878 a80 00000014
R 6880 a80 00000014
W 688c a80 00000914
R 6894 F7000508 00000000
R 6898 a80 00000914
W 68a8 a80 00000914
R 68ac a80 00000914
W 68b8 a80 00000914
R 68c0 F7000504 00300000
R 68c4 a84 00000000
W 68d4 a84 00000000
W 651c a38 00000080
R 6528 000 44005011
W 6550 000 4400500a
R 6554 a1c 00020820
W 6564 a1c 0002080a
R 6578 F7000504 00300000
R 657c 000 4400500a
W 6590 000 4400500a
R 6598 b8 00002640
W 65ac b8 00002210
R 65b4 b0 2000017e
W 65d4 b0 2002ef7f
R 65d8 004 00000000
W 65e8 004 30000000
W 664c 00c 04050300
R 665c 014 00000000
W 666c 014 00000000
R 66b4 000 4400500a
W 66d8 000 4000500a
R 66e0 018 00000000
W 6704 018 000c4440
R 670c 010 00000000
W 671c 010 00000000
R 6724 400 00000000
W 6734 400 00000000
R 6b24 a1c 0002080a
W 6b2c a1c 0002080a
```

## Formula fragments

At 6784..679c for type8:
q = (8*T[0] + floor(P[2c]*46/1000) + 999)/1000,
unsigned integer arithmetic, q=5 for 528.
PHY+a2c = (T[a0]<<14)|(T[274]<<18)|(q<<6)|T[1ec].
PHY+a30 = (T[38]<<8)|(T[24]<<14)|0x67c00008.
PHY+a34 = (T[9c]<<6)|(T[2c]<<13)|
           ((T[1f0]/2-T[1ec]+T[1e8]+T[50])<<21).
All three match isolated trace. At 685c..68d4 the routine packs
controller MR words into PHY+a80/a84; use actual configured bank.
At 6528..6550 PHY+000 = (old & ffffc0c0)|T[1e8]|
          (T[1f0]==16 ? 0x1000 : 0x2000).
At 6554..6564 PHY+a1c = (old & ffffffc0)|T[1e8].
At 6578..6590 PHY+000 bit27 comes from timing bank B+504 bit17.
At 6598..65ac PHY+b8 = (old & fffff000)|(T[0]&fff).
At 65b4..65d4 PHY+b0 = (old & fff800fe)|0002ef00|table_byte0.
This gives 2002ef7f with measured prefix word 2000017e, consistent
with original 528 MHz post-init PHY baseline. The low 12 bits of
b8 are 210 (528), high bits may differ due to earlier/other stages.
At 6b2c PHY+a1c &= ~0x40000.

Remaining PHY fields depend on geometry and lookup table bytes.
Avoid replacing RMW with full constants. No DDR state machine,
training, bus responses or actual memory are simulated.

## Reproduction

tools/ddr-v118-interpret.py extended in 684b381. The local
partial-disassembly harness has executed successfully and hashes
match the original. Exact-full-file wrapper still awaits integration
validation. No change to Monitor, no new board executable.
Next: reconstruct 32a0(H,ch,R,0,0,0), then integrate the entire
guarded first-channel setup before DFI. Do not run fragments on
the board in isolation.
