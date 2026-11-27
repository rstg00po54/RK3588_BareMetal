# SRAM-only DDR528 dry run

This app is a step toward target integration, not a DDR driver that
writes hardware. Existing stable SRAM Monitor ABI1 remains unchanged.
All addresses are keys in a mock array; no physical MMIO callback,
no DRAM read/write, no training or real clock change.

## Build

    make -f boot/ddr-cold-sram.mk
    # out/sram-ddr-dry/ddr528-dry.bin

Defaults to clang --target=aarch64-none-elf, ld.lld, llvm-objcopy.
A GNU cross-toolchain may be selected via AARCH64_CC, AARCH64_LD,
AARCH64_OBJCOPY; that toolchain is not tested this round.
The app uses existing ABI1 entry.S, link.ld and app_abi.h.
Native test_probe528.c now also exposes app_main with
PROBE528_EMBEDDED. Both modes share the same mock callbacks.
The 16456-byte mock array and 956-byte result are explicitly in
loaded .data rather than stack/BSS. Mock assertion failures latch
an error, and array overflow is guarded. A small freestanding
memset/memcpy implementation is linked.

## Actual build results

Local reconstructed /tmp/rk-regression tree, clang AArch64:
first link failed because probe528.o was missing and the compiler
placed the assertion latch in BSS. Both fixed.
Final binary 35060 bytes; ELF AArch64 entry 0; ABI header words
31504153,1,10,0 (hex); no ELF relocation entries or undefined
symbols, no BSS; .data 17416 bytes. Max individual reported
static stack frame 304 bytes; sum of all per-function compiler
static stack reports 1920 bytes, not a measured stack high-water
mark and not including monitor callbacks. Slot limit 61440 bytes,
4096 bytes reserved stack. PC-relative code uses page-aligned
slots. Actual AArch64 execution on the board is still untested.

Binary SHA256 (local Clang build):
4fa9d9cf6debd40a1a4a0bba7a75a744db2a4caec2350b161caef2a3d042597d

Native app_main harness ran twice, three scripted four-channel
scenarios each, returning 528d0000 on both invocations. Null API
correctly refused. All 11 native regression executables and
saved-trace fixtures passed again after changes. Repository blob
for modified test_probe528.c matches locally tested source
b5e9817d1ca03b0fa13534c94a520ce4066e724b. Local copies of other
sources are from previous rounds, not a fresh clone.

## Optional target test (existing monitor)

    sudo sram-usb exec out/sram-ddr-dry/ddr528-dry.bin

Expected serial:
    [DDR528 DRY] SRAM-only mock; NO MMIO
    [DDR528 DRY] four-channel scenario PASS
    [DDR528 DRY] four-channel scenario PASS
    [DDR528 DRY] four-channel scenario PASS
    [DDR528 DRY] PASS; real DDR unchanged

Expected app return 528d0000 and host APP RETURN PASS. This test
may be performed on the current partial-prefix board, since it
does not access physical DDR registers; no MaskROM/power cycle
is needed for this specific dry run. It is not proof that DDR
initialization works. If monitor hangs, stop and collect serial
output rather than retrying repeatedly.

Remaining before first physical DDR probe: cold-entry validation,
USB/clock ownership, bounded and logged physical MMIO backend,
review full 89dc ordering against original disassembly. Real
geometry-dependent second pass and final address map, final
training and DRAM integrity remain separate milestones.
