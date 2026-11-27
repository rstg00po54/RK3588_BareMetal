# Native fixed-528 MHz first-probe PHY configuration

This round converts the previously interpreted 32a0 and 6aac/6214
paths into independent C. Original binary v1.18 SHA1
8d5e55ebf63d94618b448b79757b1f07733fd076.
No physical IO backend, board operation or Monitor modification.

## Implementation

boot/ddr_cold/phy528.c:
- cold_phy528_electrical: 32a0 fixed type8, 528 MHz, FSP0, first probe
  geometry, original electrical input. 16 writes, 16 reads on
  immediate commit acknowledgement. Both commits have bounded waits.
- cold_phy528_timing: 6aac wrapper mode0, 6214 fixed type8 / FSP0
  timing path. 22 writes, 21 reads. MR field packing uses live
  injected reads of the configured controller timing bank, not
  a copied trained-register snapshot.

The code retains ordered RMW and separate operations to the same
register, including redundant reads and writes. PHY timing values
are specialized to the known P/T/H and initial geometry. This is
not a general electrical table generator, and second-pass geometry
and other frequency banks are not supported.

## Native differential verification

Local /tmp/phy_diff.py reused the restricted instruction interpreter
(/tmp/emu_regen.py source) to execute 6dc4 and 5e38, then start
32a0 with 64 different synthetic PHY initial banks (zero, all ones,
62 deterministic random banks). 6aac followed 32a0 preserving
the simulated register state. Each case produced 75 access records.
Controller bank values are held fixed at first-probe profile;
commit acknowledgements are scripted. The original interpreter
executed CH0; expected addresses for CH1..3 were translated using
the established controller and PHY strides.

Native C compared byte-for-byte with these expected traces:
256 cases, 19200 ordered reads/writes, cmp PASS.
Separate tests cover both commit timeout branches and invalid
channel argument. No hardware side effects or timing are verified.
Fixture SHA256:
input 5e71e7b24bcaa25b98ab0a0756f318c1b1f42e67643b03fb1d23042ab222a044
output 90a32da59b04ad63675070d1c780db3192de3741224a173f367bac1fbf54725e

The reproducible fixture test is test_phy528_vectors.py; it compares
the C trace against the saved oracle digest. The original partial
instruction interpreter was executed locally; the full-blob oracle
wrapper remains integration-untested.

## Regression

In local reconstructed source tree:
  make -f boot/ddr-cold-host.mk clean test

All seven native tests compiled with -Wall -Wextra -Werror.
Previous DFI, switch, CA selection, geometry, posttrain and CTL528
tests still pass. New output:
  PHY528: 256 cases, 19200 ordered MMIO accesses match oracle fixture; 2 commit timeouts PASS

## Integration boundary

The electrical and timing functions are intentionally separate:
the original sequence places address-map setup, reset release
and other controller operations between 32a0 and 6aac/6214.
The differential fixture tests these functions in isolation, NOT
complete 89dc. No complete target sequence is authorized by this
result. Next focus: native training sweep, rank probing,
all-channel clock/reset sequence, second-pass and physical validation.
