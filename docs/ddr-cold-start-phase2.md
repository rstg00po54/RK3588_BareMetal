# DDR cold-start reverse engineering: phase 2

## v0.9 target verification, 528 MHz

- FD8D8118 = 0023FFFF
- FD8D8120 = 0023FFFF
- FD8D8184 = 00000000
- MEMTEST scratch 0x44C0, 4096 bytes, 38 patterns, PASS.
- No full-capacity or retention validation yet.

## DDRPHY PLL programming

Original v1.18 blob, file offsets, direct instruction decoding.
Function 0x17bc called at 0x8a44. Arguments are descriptor, channel,
PLL selector, target frequency-like value; exact input unit and fractional
branch semantics are not yet fully traced. The base is read from channel
descriptor +0x10 (0x19f8, 0x1a98, 0x1ae0). A/B register banks are separated
by 0x20. Do not confuse this with CPU APLL.

Candidate integer A path (0x1a90..0x1abc then 0x1a2c..0x1a5c):
- CON0 <- 80008000 (0x1aa0)
- CON1 <- 20002000 (0x1aa8)
- CON0 <- 03ff0000 | M (0x18c4, 0x1ab0)
- CON1 <- 01ff0000 | P | (S << 6) (0x18c8/0x18cc/0x1a94/0x1ab4)
- CON2 <- ffff0000 (0x1ab8)
- CON1 <- 20000000 (0x1a38)
- wait 1 us via 0x105fc (0x1a40)
- read CON6 (0x1a48), test bit15 with TBNZ (0x1a4c);
  up to 1000 reads; original exits loop on timeout without reporting it.
- CON0 <- 80000000 (0x1a5c).

B bank analogous offsets 0x20,24,28,38 at 0x1ad0..0x1b4c.
Some branches also write CON3/4 fractional configuration; not implemented.
Original function temporarily writes global CRU +0x594 = 00c00040 at
0x18f0 and restores selected bits at 0x1a60..0x1a70. This external
clock-routing prerequisite is NOT included in the isolated helper.
0x8a78 writes channel PHY_CRU+0xa00 = 00160016, waits 10 us, followed by
00100000 at 0x8a9c, 00040000 at 0x8ad4, 00020000 at 0x8b04 and
00020002 at 0x8b44; intervening global CRU writes and calls remain to
be decoded. These are partial observations, not a usable reset recipe.

528 MHz observed M=88,P=1,S=2. CON6 bit15 set in all four snapshots.
Frequency readback matches 528 MHz configuration. No direct clock measurement.

## Isolated implementation

boot/ddr_cold/cold_steps.h and cold_steps.c provide two standalone
steps with dependency-injected MMIO and delay. No caller in OpenBoot,
no CODE471 entry or real MMIO backend. The PLL function is deliberately
named pll_integer_fragment: it lacks surrounding mux/reset setup.
Timeouts return an error rather than continuing, and failure leaves
partial hardware state (cold reset required before retry).
The host mock checks access sequence, masks and timeouts. It does not
model hardware or establish safe live use. No existing loader changed.
Run: cc -std=c11 -Wall -Wextra -Werror boot/ddr_cold/cold_steps.c boot/ddr_cold/test_cold_steps.c -o /tmp/ddr-cold-test && /tmp/ddr-cold-test
