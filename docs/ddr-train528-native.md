# Native first LPDDR4X 528 MHz training scan

Implemented cold_train528_ca in boot/ddr_cold/train528.c, for the
first 41b8 operation mask 0x10, type8, 528 MHz, FSP0, rank selector 1.
See ddr-training-41b8-first-scan.md for original offsets.
No board or Monitor changes; callbacks are mock IO only.

The driver performs 3114 preparation, delay scan, controller
commits, actual MRR primitive calls through injected IO, selection,
final DFI timing programming and 3154 restoration. It also
implements the 16c0 PHY+50 bit19 pulse, with 1 us delay.
Intentional departures: bounded polling, reject selected gaps in
passing mask, re-read MR12 at the chosen setting, return -3 for
no passing samples and -4 for failed selection/recheck.
Output result is assigned only after success. A failure can leave
partial state, even for an invalid observed base value. Never retry
or advance to memory accesses; require cold restart. Poll bounds
cannot prevent a hung physical MMIO bus transaction.

Native tests: four channel address strides, two initial base
parities, all 512 masks each, 4096 total. 2112 successes and
1984 correctly rejected windows. Five injected failures:
commit ack, MR busy, command trigger, data valid, final recheck.
All passed. The initial compile warning in the test (misleading
indentation) was fixed; clean build with -Wall -Wextra -Werror.

Compared native CH0 access traces against saved actual restricted
instruction runs (/tmp/train-{match,window,all,single,none}.txt).
Four success cases have 188 comparable operations each; all-fail
has 169. All 921 match byte-for-byte after excluding the intentional
10 additional final MR12 verification accesses and four 16c0
pulse accesses from each successful native trace. The original
interpreter stubbed 16c0; pulse itself was separately decoded and
is explicitly asserted by the fixture. The trace fixture script
test_train528_vectors.py stores hashes of original traces, not
a live re-execution of the vendor binary.

Rebuilt the local reconstructed source tree with
make -f boot/ddr-cold-host.mk clean test; all eight native programs
passed. Then added and executed the saved-trace fixture, which also
passed. This is host regression, not a hardware training test.

Remaining: independent native operation-mask-8 training, complete
clock/reset/address-map sequencing, geometry-driven second pass,
physical MMIO integration, real DFI and training responses, and
memory tests. The current board must not run these fragments in
isolation. No user board action required.
