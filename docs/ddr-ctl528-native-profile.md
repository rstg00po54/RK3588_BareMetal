# Native fixed-528 MHz controller profile: differential verification

This step converts the known v1.18 5e38/53e4 LPDDR4X path into
host-testable independent C, rather than adding another untested
physical-register script. No Monitor change or physical MMIO.

## New verification

Re-ran local /tmp/emu_regen.py match. Both training scenarios and
geometry in this replay are synthetic, not hardware measurements.
Second 6dc4 returned 0 at step 4258. Isolated 5e38 mode0 returned
at step 4755. TMG14=001600cd and RFSHSET1TMG1=000000c9 still
differ from vendor post-init 528 values.

For the hypothetical single-rank, width-category-1, 528 MHz second
pass, checked 5e38 mode1 directly. 5eac..5eb4 skips the C+280
bit29 write when R+10!=0. MSTR0 becomes 01080002 instead of
03080002. 6118..613c tail branches to 5e30; the actual original
blob bytes at 5e30 are c0035fd6 (RET). No additional operations
are hidden there. All other writes match this fixed profile.

## Native implementation and differential test

ctl528.c uses 53 ordered operations for mode0, 52 for mode1.
15 or 14 reads, respectively. Values are for the exact fixed
LPDDR4X 528 MHz configuration already derived; not general timing
generation. In particular it uses the first-pass timings, not the
vendor's final trained snapshot. All 15 RMW operations preserve
the correct bits. Mode1 is limited to the hypothetical geometry
above. The caller will eventually need to guard actual geometry.

Executed the original instructions with 64 synthetic register
initial states (zero, all-ones, 62 deterministic random patterns)
for each of two modes, CH0. Independently compared native C traces,
including four channel address translations, against those oracle
results. 512 native cases, 34304 individual read/write records,
byte-for-byte cmp PASS. The original interpreter ran CH0; the
other channel addresses in the expected traces are translated
by the known controller stride, not independent channel simulations.
No hardware state machine or register side effects modeled.

SHA256 oracle output:
d852fcb72da8820e8389c1ecbc4bba3a3cb2eac14d122bd1dbfdace5d6e7efd7
Test fixture generator uses Python random.Random(3588), and verifies
both input and expected trace digests. Original local interpreter
harness is not part of this fixture; the full-blob oracle still
requires integration testing.

## Consolidated regression

make -f boot/ddr-cold-host.mk test

Ran successfully in a local reconstructed source tree from the
previously tested source copies and new files:
- DFI/MRW/MRR/PHY success and timeout tests PASS
- 528 MHz switch: four channel addresses, six timeout paths PASS
- CA selection: 1024 masks PASS
- Geometry: 65536 raw MR8 pairs PASS
- Posttrain: 17600 output values match original oracle digest
- CTL528: 512 cases, 34304 ordered MMIO accesses match oracle fixture PASS

All six native executables compiled with -Wall -Wextra -Werror.
This make target is host-only, needs cc, Python 3 and sha256sum.
No requirement for the user to run it now.

## Next integration gates

The fixed controller profile is independently executable C but
not wired to SRAM Monitor. The physical sequencing still needs
all-channel reset/clock setup, initial address-map generation,
32a0/6214 native implementation, actual training and error checks,
and full second-pass 89dc integration. The two timing differences
are not resolved. No statement of DDR readiness is warranted until
actual DFI/MR/PHY acknowledgements and DRAM read/write tests pass.
