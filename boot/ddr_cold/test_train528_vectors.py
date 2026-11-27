"""Compare native CA scan against saved restricted vendor trace hashes.
The original interpreter stubbed 16c0. The native driver additionally
rechecks MR12, so remove only those 14 known extra accesses.
"""
import hashlib
import subprocess
import sys

cases = [
    (range(8,15,2), "6737e7bd00376388d490fb21a616c07d07d2408ae9a6708b5ad2b13430b8ee70"),
    (range(6,15,2), "b0a498f8f6b2713597d8de42fe6ddb2b7eb8e6f083f023f4cffc6fc6f5039347"),
    (range(2,19,2), "8a7159ae54cda3789decd509698b155eecccd202a82fc5d11b83d86a4df8b9ea"),
    ([8], "a38b23019e08ca37cedf1e7be25f0b7a407de0d75ce5e45893b5ebbbcfad33b3"),
    ([], "fd08ccb50bbf83eb34086cf72cda7457812adceab271b1e9a90cfe9e4df59d6f"),
]
total = 0
for ds, expected in cases:
    mask = sum(1 << d for d in ds)
    lines = subprocess.check_output([sys.argv[1], "trace"],
                                    input=f"{mask:x}\n".encode()).decode().splitlines()
    result = lines.pop()
    if mask:
        assert result == "RESULT 0" and len(lines) == 202
        assert lines[178].startswith("R f7000508 ")
        assert lines[179].startswith("W f7010080 ")
        assert lines[187] == "R f7010094 0000005c"
        assert lines[192:196] == ["R fe0c0050 00000000",
                                  "W fe0c0050 00080000",
                                  "R fe0c0050 00080000",
                                  "W fe0c0050 00000000"]
        del lines[192:196]
        del lines[178:188]
        assert len(lines) == 188
    else:
        assert result == "RESULT -3" and len(lines) == 169
    total += len(lines)
    assert hashlib.sha256(("\n".join(lines)+"\n").encode()).hexdigest() == expected
print(f"CA training: 5 vendor trace fixtures, {total} comparable accesses PASS")
