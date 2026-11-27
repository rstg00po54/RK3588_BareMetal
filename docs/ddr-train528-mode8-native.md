# Independent native first-probe mode8 training

Implemented cold_train528_mode8 in boot/ddr_cold/train528_mode8.c.
Scope is original v1.18 41b8 operation mask8, type8, 528 MHz,
FSP0, rank index0, first probe. Not the full main training schedule.
No physical MMIO backend, no new firmware, no board action.

Reviewed 44f8..468c and 4dc0..5044 using actual AArch64 disassembly
in local /tmp/train.o and /tmp/traintail.o. The tenth raw sample
is PHY+254, not +25c or +264; previous nine use +1f0+12*j.
The earlier interpreter mock supplied 01000080 only at those nine
addresses; the tenth sample was zero. This is reflected in fixtures.

Native implementation includes preparation, pattern writes, bounded
a24.bit1 wait, ten raw sample reads, measured period selection,
coarse/fine field programming, three 16c0 pulses on success, cleanup,
e8/ec status checks, controller restoration. Explicitly refuses
period zero or coarse quotient >7 before writing delay banks.
Only assigns diagnostic result on success; failures can leave
partial state. No automatic retry or DRAM access. Existing MR14
and training ordering must be verified in the integration layer.

## Actual host validation

Native C tests compiled and ran in /tmp/rk-regression:
800 successful vectors (four channels, 100 periods, both b0.bit0
values), five injected failures (request timeout, e8, ec, zero
period, coarse overflow), invalid channel argument.
Two earlier original-instruction traces, second-match.txt and
second-fail.txt, compared with native reads/writes. 240 success
and 229 failure records match byte-for-byte after excluding only
the known 16c0 pulse sequences (12 success, 8 failure accesses).
The prior interpreter stubbed 16c0. This comparison is not an
independent model of physical hardware state. Fixture script
checks hashes and exact pulse sequence/count.

make -f boot/ddr-cold-host.mk clean test
actually executed in the reconstructed local source tree.
All nine native test executables and all saved-trace fixtures passed.
No hardware success implied.

## Remaining integration

Complete first cold 89dc reset/clock/address-map sequence, all
active channels and DFI ordering, MR14 and rank-probe state
machine, second-pass geometry-dependent configuration, final
training, mapping and actual DRAM validation. Both native training
functions now exist but must not be executed on the board in
isolation. Previous partial-prefix state unchanged.
