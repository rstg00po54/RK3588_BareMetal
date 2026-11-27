# First fixed 528 MHz four-channel 89dc integration

New host-only entry cold_first528() in boot/ddr_cold/first528.c.
Fixed LPDDR4X type8, first probe geometry (rank candidate 2,
equal rows 17), bank A, FSP0, four active channels. No physical
MMIO backend, no Monitor change. This does not yet invoke main's
subsequent training functions or perform actual DRAM accesses.

## Rechecked source details

Decoded original 175c, bad8, 17bc 24 MHz path, row4 map at 11530
from actual repository binary words with clang/llvm-objdump.
Reused local disassembly of 89dc from currentddr.o, next.o,
postctl.o, dfi.o and frequency switch from pllnext.o.

175c reset register address:
    FD7C0000 + 4*(0x294 + ch + floor(ch/2))
mask = 0x1180 >> (4*(ch&1)).
This yields A50/A54/A5C/A60, masks 1180/118/1180/118.
8a84..8ac8 uses bit 40 or 4, 8b60..8bc4 uses bit 8 or 1.
17bc 24 MHz request selects bypass A via PHY_CRU+0=80008000.
bad8 saves CRU+280/+304, selects temporary clocks with
03000000/0fc009c0. Restore ORs both saved words with ffff0000
at 9118. This deliberately matches the original and does not
restore clocks after each channel as stage1 did.
Initial address-map row4 from 11530 consists of ten words,
first to B+30004, rest to B+3000c..3002c. 91d4 adds the row18
disable field. This is only the initial two-rank probe mapping.

The new sequence includes the initial PHY+bf4=9 / PHY+000 RMW,
post-5e38 D0C, ZQCTL and FSP selection, 32a0, initial map,
controller reset release, configuration commit and 6214.
DFI starts are all completed before any DFI complete operation.
Per-channel normal-state wait, MR11/12/22, ZQ and PHY request,
528 MHz switch follow. Global clock restore is done only after
all four switches, followed by all four PHY+b4 bit18 waits.

## Actual host regression

Compiled and ran in /tmp/rk-regression:
make -f boot/ddr-cold-host.mk clean test
All ten native executables and prior trace fixtures passed.
New integrated result:
    first528: 4-channel ordered mock success, 760 writes, 462 reads
    first528: 40 injected stage/channel failures + invalid argument PASS

This test links the actual native controller, electrical/timing,
DFI/MRW/PHY ack and frequency-switch fragments. It scripts
DFI/PLL/MR/ZQ responses; no physical hardware state machine.
Assertions check all-channel ordering, initial map, clock
restore writes, selected MR words, PHY prefix and final PHY
frequency field and reset register addresses. Failure injection
covers ten stages in each channel (including PLL lock timeout).
No continuation after a failure; partial state is NOT rolled back.
Status reports exact stage/channel/return code. Repeated failure
requires cold restart. MMIO transaction hangs are still outside
the scope of finite polling.

The integrated sequence is derived from disassembly and has
mock ordering tests, NOT a full original-instruction differential
trace of 89dc. Previously tested individual profiles retain
their existing differential coverage. Register reset values,
acknowledgement behavior and clock-domain accessibility remain
unverified on physical hardware.

Local tested source SHA256:
first528.h e2451763947b318b7c6e2654321aaf42c3a296581c2e9f58b6493e9a1f495ded
first528.c e611b0a7b04d4ef9425a4b6e4537321afa9f917d568ffbfa2f30167cfefb271a
test_first528.c 9ef5c044353c538513a0f4900d14de066e54befb2c521145fec415fb7dac8518

## Next gates

Main 9e48..a218 must integrate the existing first and second
native training functions, CH0-only MR14 rejection check,
MR14 programming, PHY pulse at 9ff0 and geometry probing.
Do not interpret F528_DONE as trained memory. The state machine
does not include PMU cold-entry checks, a target MMIO backend,
second-pass geometry-dependent timing, final mapping or memory test.
Do not run on the currently partial-prefix board. No user board
operation requested.
