# Physical DDR528 bring-up: read-only preflight and locked diagnostic

User's actual SRAM dry-run log returned 528d0000, three scenario PASS.
It proves SRAM execution of mock code, not physical DDR initialization.
The prior ddrstage1 guard refused writes; reason was not printed.
Do not repeat stage1 or infer fresh power-on from that rejection.

## First test (existing Monitor)

    make -f boot/ddr-preflight.mk -j$(nproc)
    sudo sram-usb exec out/sram-ddr-preflight/ddr528-preflight.bin

Default toolchain is GNU AArch64, like existing SRAM Monitor.
Read-only ABI1 app prints 36 actual MMIO reads: OS_REG2..5, CH0
STAT, PMU idle request, global clocks, both PLL banks per channel,
four reset / PHY reset / DDR GRF values. It checks EL3, MMU and
D-cache off, 24 MHz moving counter. Does not write DDR registers.
Result 528e0000|faults: bit0 OS, bit1 STAT, bit2 idle, bit3 PLL,
bit4 previous reset-prefix signature. 528e00ff/fe/fd mean ABI,
CPU context, timer rejection. A zero mask is only a candidate,
not proof of fresh cold power-on. A set bit4 can also be a reset
default; conservative rejection until reviewed. Other signatures
may not detect every partial state. On the current board capture
the full log rather than repeat stage1. Direct reads can still
cause a synchronous abort or hang a bus; last logged address is
the diagnostic clue. This app has no recovery for such a hang.

## Standalone physical MMIO diagnostic

boot/ddr-first-live.mk builds ddr528-first-live.bin with a real
volatile 32-bit read/write callback, bounded timer delay, region
allowlist, UART breadcrumbs before writes, exception logging,
independent EL3 stack and exception vectors. The existing Monitor
non-ABI run path stops USB before jumping, unlike ABI exec.
Do not run this new standalone image yet; no USB restart/return.
It is locked at compile time unless DDR528_COLD_SIGNATURE header
is supplied and matches the read-only cold snapshot. No real cold
signature header shipped. Fresh cold power-on is independently
required, signature alone cannot prove it. No physical writes
allowed in default build. The optional write-enabled path calls
cold_first528 only, stops at completion or error, no training
or DRAM accesses. UART logging changes timing and may stop if
a clock domain fails. The address filter is not a silicon safety
proof. Full first528 original-instruction differential replay
and hardware clock/reset ordering remain open.

## Verified locally

From /tmp/rk-regression reconstructed source tree, not fresh clone:
- Native guard: clean candidate + 124 fault combinations PASS.
- All 12 native regression executables and prior saved trace
  fixtures passed after make clean test.
- Clang AArch64 + LLD cross-link, no ELF relocations:
  read-only ABI app 1492 bytes, no BSS.
  locked standalone 19876 bytes, within 64 KiB.
  test-only signature (zero fixture, NOT hardware data) build
  20180 bytes, no BSS and no relocations. Never executed.
- Local GNU AArch64 compiler unavailable; make defaults to GCC
  for user's Ubuntu. No hardware execution of these new images.
- Host makefile Git blob 1595a0c659dc3bcb678ba591567168326dd655a2.

Read-only app SHA256 (local Clang build)
e65ede4c7d48d96a13ecaccb776e1ea06dfe45ac4d1347006124df4179224092
Locked standalone SHA256 (local Clang build)
8bb5dc4788f308416ed3d448bd87a26ddc311b77d30207b559d84e07367208d9

Next gate: collect current read-only signature, establish freshly
powered cold baseline before enabling physical writes. Do not
run a write-enabled image on the partial-prefix board.
