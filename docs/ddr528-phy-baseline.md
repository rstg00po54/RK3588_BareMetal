# 528 MHz PHY baseline, v0.8

Board reported OpenBoot v0.8 C ENTRY OK, BSS zero PASS, EL3, MMU and
caches off. All four ddrphy commands completed. 24 reads/channel.

| Offset | CH0 | CH1 | CH2 | CH3 |
|---|---|---|---|---|
| 0xB0 | 2002EF7F | 2002EF7F | 2002EF7F | 2002EF7F |
| 0xB4 | 5DFC0202 | 5DFC01FE | 5DFC01FE | 5DFC01F9 |
| 0xB8 | 00000210 | 00000210 | 00000210 | 00000210 |
| 0x580 | 0 | 0 | 0 | 0 |
| 0x190+12*i (i=0..7) | 0 | 0 | 0 | 0 |
| 0x614+12*i (i=0..7) | 0 | 0 | 0 | 0 |
| 0x18C, 0x610 | 0 | 0 | 0 | 0 |
| 0xDC0, 0xDC4 | 0 | 0 | 0 | 0 |

84 zero reads, 12 nonzero reads. Same B0/B4/B8/580 values as v0.7.
No assumption that zero means no training. The print_train_result
function has mode-dependent branches and separate read/write reporting.
The four-byte reads are not a complete PHY configuration or a replay recipe.

## Next control experiment
Use exactly the same v0.8 payload with original unmodified DDR:
```sh
make -f boot/v08.mk pack
sudo rkdeveloptool db out/v08/rk3588-baremetal-ram-only.bin
```
Cold reset into MaskROM between experiments. Check vendor frequency and
shell banner, then ddrphy0..3. Compare to this snapshot. No flash writes.
If zero persists at high frequency, prioritize actual initialization
write paths and register mode selection rather than extending snapshots.
A bounded DDR memory test is still required before declaring stability.
