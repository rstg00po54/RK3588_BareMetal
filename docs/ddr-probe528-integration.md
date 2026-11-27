# Integrated first LPDDR4X 528 MHz probe

New host-only entry cold_probe528() links cold_first528(),
cold_train528_ca(), cold_train528_mode8(), MR commands and
lp4x_geometry528(). Main order rechecked against local
postmain.o/geomnext.o disassembly and repository bytes.
Fixed type8, first pass, four channels, 528 MHz. No physical
MMIO backend, no actual memory access, no firmware change.

For each channel after successful first528:
- 9e9c CA scan.
- 9f9c channel zero only: read MR14 rank mask1, reject 5d.
- 9f3c write MR14 rank mask3 from controller B+50c low byte.
- 9f6c operation-mask8 training.
- 9ff0..a018 PHY+4 bit5, PHY+10 bit9, 16c0 pulse, clear bit9.
- a06c read rank0 MR8, save and zero C+180.
- a120..a178 compare MR12, MR5, MR14 between rank masks1/2,
  stopping at first mismatch.
- If all match, read rank1 MR8, conditionally MR0 if width
  categories differ, then decode geometry.
- Restore C+180 on success. Store candidate bit and move on.

Original retry/fallback behavior is deliberately replaced by
terminal errors; a partial failure requires cold restart.
The geometry structure is the existing simplified raw decoder,
not a full vendor R object. Rank comparisons can alias and MR8
values are not independently validated. candidate_mask identifies
completed probes, NOT verified physical ranks or usable capacity.
No second-pass initialization or final mapping is attempted.

## Actual test results

Built and ran in local reconstructed /tmp/rk-regression tree:
    make -f boot/ddr-cold-host.mk clean test
All 11 native test programs and prior digest fixtures passed.
New output:
    probe528: scenario 0 candidate mask f, 1876 writes, 1363 reads PASS
    probe528: scenario 1 candidate mask f, 1956 writes, 1463 reads PASS
    probe528: scenario 2 candidate mask f, 1972 writes, 1483 reads PASS
    probe528: 30 injected failures + invalid argument PASS

Scenario 0 single-rank candidate (first MR comparison mismatch);
1 matching two-rank candidate; 2 differing width category with
MR0.bit1 allowing it. All MR responses and training samples are
scripted. Tests check ordering, channel zero MR14 precheck,
conditional rank reads, result mask and C+180 restoration.
30 failure cases: seven stages across four channels, MR14
rejection, incompatible geometry. Previous first528 test covers
40 initial-stage failures. The whole chain is mock-tested, not
a full vendor-instruction differential replay.

Verified repository Git blob hashes match local tested files:
probe528.c ce67646317b2e8337db445bb129eb1ebd95b1cf5
probe528.h c5e5c419a04282568da06645bdd8e273b8316c31
test_probe528.c ad6d225dbf08462cdd72158c075dd17469125b2a
lp4x_geometry.c 428bd42379fc04a3cd119c8e453e86b0485e99ac
lp4x_geometry.h 8e95da920b70e00b224e4b94f96058f044ec4f72
boot/ddr-cold-host.mk d010709de697e90cf441498b3201c1ecb6d654ff
The makefile's only local discrepancy was its leading comment,
now reconciled. The rest of the regression source tree is a
previously reconstructed copy, not a fresh complete Git checkout.

## Next gates

Derive and validate the geometry-dependent second-pass mapping
and timing generation; do not assume the simulated geometry
is the board geometry. Need physical cold-entry guards and a
bounded diagnostic SRAM app before hardware testing. Actual
PLL/DFI/PHY/MR behavior and DDR read/write stability remain
unverified. No board operation requested this turn.
