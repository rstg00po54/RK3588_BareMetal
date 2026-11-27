# DDR v1.18 cold-start reverse engineering, phase 1

Original firmware blob SHA1 8d5e55ebf63d94618b448b79757b1f07733fd076.
All addresses in call graph are file offsets, not runtime addresses.

## Established call edges

- 0x9f8 BL 0x992c: main DDR initialization path.
- 0x9974 BL 0xcf38: populate 4 channel descriptors, 0x20 bytes/channel.
- 0x9a50 BL 0xced0: global hardware handshake, described below.
- 0x9c18 BL 0x6dc4: configuration generation; also called at 0xa89c.
- 0x8b5c and 0x8e3c BL 0x6aac: channel PHY configuration wrapper.
- 0x6ad8 BL 0x6180 and 0x6b20 BL 0x6214: separate wrapper branches.
- 0x75d8 frequency-switch path calls 0x6180 and 0x6214 at 0x78a4/0x78c0.
- 0xb958 prints "out"; 0xb964 still calls 0x10364 before returning.

0xcf38 builds descriptor {CTL, PHY, PHY_CRU, other}, each pointer 64-bit:
CTL=F7000000+channel*01000000, PHY=FE0C0000+channel*10000,
PHY_CRU=FD800000+channel*4000, other=FE000000+channel*2000.
Global descriptor +0xb8 = FD8D0000 (0xcfe8/0xcfec).

## Global handshake (0xced0..0xcf34)

The base is *(descriptor+0xb8) + 0x8000 = FD8D8000.
Instruction-derived sequence (register block function/name still unknown):
```c
write32(base + 0x110, 0xf000f000);
while (!(read32(base + 0x118) & 0xf0000000)) {}
while (!(read32(base + 0x120) & 0xf0000000)) {}
write32(base + 0x160, 0x00030003);
write32(base + 0x154, 0x00030003);
while (!(read32(base + 0x184) & 3)) {}
write32(base + 0x154, 0x00030000);
while (read32(base + 0x184) & 3) {}
write32(base + 0x160, 0x00030000);
write32(base + 0x110, 0xf0000000);
```
The tests are "any bit", not "all bits". Original loops unbounded.
Do not execute this sequence on a running DDR system. Future implementation
needs bounded polling, confirmed register ownership and cold-start context.

## PHY B0/B8 write provenance

Function 0x6214 computes x6 = descriptor + channel*0x20 (0x6234/623c).
0x6264 loads x19 = [x6+8], the PHY base. 0x6238 loads x20 from [x2+0x230].
0x6598..0x65ac and 0x65b4..0x65d4:
```c
v = read32(phy+0xb8);
write32(phy+0xb8, (v & 0xfffff000) | (read32(cfg+0) & 0xfff));
v = read32(phy+0xb0);
write32(phy+0xb0, (v & 0xfff800fe) | 0x0002ef00 | config_byte);
```
config_byte comes from LDRB [x0] at 0x65cc; exact source/semantics still
unresolved. B8 low 12 bits match observed frequency, B0 bit0 changes
with low/high frequency. Neither fact establishes how to start training.
Do not replay B4 DLL result. Current evidence is insufficient for full CODE471.

## v0.9 diagnostics

ddrseq reads only three status addresses referenced by the above polling loops.
memtest writes only a private 4096-byte BSS array in the loaded image,
with zero, ones, alternating bits, address pattern, inverse address pattern
and walking one patterns. No arbitrary RAM address writes or storage writes.
Checks MMU/D-cache off, stops at first mismatch. This is a smoke test,
not full capacity/retention/temperature validation. Both commands explicit.
Build with bash tools/build-ddr-test.sh freq-only v09.
No target compile/run verified at commit.
