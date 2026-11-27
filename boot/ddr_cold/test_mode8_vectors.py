"""Compare mode8 native traces with saved v1.18 interpreter digests.
Interpreter stubbed 16c0; exclude only the exact four-access pulses.
"""
import hashlib
import subprocess
import sys
pulse = ["R fe0c0050 00000000", "W fe0c0050 00080000",
         "R fe0c0050 00080000", "W fe0c0050 00000000"]
for fail, result, count, pulses, digest in [
    (0, 0, 240, 3, "5ec8e66d63794f81efb55c52b0c10ce81056dda42eec87d4311fbf0d56c57f2b"),
    (2, -4, 229, 2, "bf7ce06da795197608effaf426173516745c7d8a99a84b5d36a130f030241a31"),
]:
    lines = subprocess.check_output([sys.argv[1], "trace"], input=f"{fail}\n".encode()).decode().splitlines()
    assert lines.pop() == f"RESULT {result}"
    clean = []
    j = n = 0
    while j < len(lines):
        if lines[j:j+4] == pulse:
            n += 1
            j += 4
        else:
            clean.append(lines[j])
            j += 1
    assert n == pulses and len(clean) == count
    assert hashlib.sha256(("\n".join(clean)+"\n").encode()).hexdigest() == digest
print("mode8: 469 comparable vendor trace accesses PASS")
