"""Native differential fixture from the restricted v1.18 interpreter.
No vendor code execution or hardware access required for this test.
"""
import hashlib
import random
import subprocess
import sys
offsets=[0x3c8,0x3d4,0x3dc,0x3e0,0xc,0x3ec,0xa2c,0xa30,0xa34,0xa80,0xa84,0xa38,0,0xa1c,0xb8,0xb0,4,0x14,0x18,0x10,0x400]
rng=random.Random(3588)
lines=[]
for trial in range(64):
    values=[0 if trial==0 else 0xffffffff if trial==1 else rng.getrandbits(32) for _ in offsets]
    for ch in range(4):
        lines.append(f"{ch} {len(offsets)}\n")
        for off,v in zip(offsets,values):
            lines.append(f"{off:x} {v:08x}\n")
fixture="".join(lines).encode("ascii")
assert hashlib.sha256(fixture).hexdigest()=="5e71e7b24bcaa25b98ab0a0756f318c1b1f42e67643b03fb1d23042ab222a044"
actual=subprocess.check_output([sys.argv[1]],input=fixture)
assert len(actual.splitlines())==19200
assert hashlib.sha256(actual).hexdigest()=="90a32da59b04ad63675070d1c780db3192de3741224a173f367bac1fbf54725e"
print("PHY528: 256 cases, 19200 ordered MMIO accesses match oracle fixture; 2 commit timeouts PASS")
