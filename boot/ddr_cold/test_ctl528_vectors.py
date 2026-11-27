"""Reproduce native differential fixture (no board, no vendor execution).
Expected digest obtained by interpreting original v1.18 5e38.
Usage: python3 test_ctl528_vectors.py /path/to/test_ctl528
"""
import hashlib
import random
import subprocess
import sys
OFFSETS = [int(s, 16) for s in '''0 18 600 604 b80 588 5a4 5a8 b04 b08 b00 d04
4 8 d08 c 10 14 30 34 38 64 608 500 504 508 50c 580 800
10280 10000 10200 10288 10508 10c94 10d00 10d04 10380 10394
10c90 10184 10500 1018c 10510 10104 10108 10118 10518
804 100804 200804 300804'''.split()]
rng = random.Random(3588)
lines = []
for mode in range(2):
    for trial in range(64):
        values = [0 if trial == 0 else 0xffffffff if trial == 1 else rng.getrandbits(32) for _ in OFFSETS]
        for ch in range(4):
            lines.append(f'{mode} {ch} {len(OFFSETS)}\n')
            for off, value in zip(OFFSETS, values):
                lines.append(f'{0xf7000000+ch*0x1000000+off:08x} {value:08x}\n')
fixture = ''.join(lines).encode('ascii')
assert hashlib.sha256(fixture).hexdigest() == 'a7791ad6b8014167e96c9ce1175dca3c97496c71c134bfdcd263c843558cb7cf'
actual = subprocess.check_output([sys.argv[1]], input=fixture)
assert len(actual.splitlines()) == 34304
assert hashlib.sha256(actual).hexdigest() == 'd852fcb72da8820e8389c1ecbc4bba3a3cb2eac14d122bd1dbfdace5d6e7efd7'
print('CTL528: 512 cases, 34304 ordered MMIO accesses match oracle fixture PASS')
