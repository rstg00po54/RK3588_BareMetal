#!/usr/bin/env bash
# Build a separate RAM-only loader using a locally patched vendor DDR copy.
set -euo pipefail

cd "$(dirname "$0")/.."
root="$PWD"
rkbin="$root/reference/rkbin"
out="$root/out/ddr528"
original="$rkbin/bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin"
blob="$out/ddr528.bin"
mkdir -p "$out"

make -f boot/v06.mk all
cp "$original" "$blob"

# Keep unrelated parameters unchanged, use the SDK's own template.
python3 - "$rkbin/tools/ddrbin_param.txt" "$out/params.txt" <<'PY'
import pathlib, sys
src, dst = map(pathlib.Path, sys.argv[1:])
changes = {"lp4_freq": "528", "lp4x_freq": "528",
           "boot_fsp": "0", "dfs_disable": "1"}
seen = set()
lines = []
for line in src.read_text().splitlines():
    key = line.split("=", 1)[0].strip()
    if "=" in line and key in changes:
        if key in seen:
            raise SystemExit("duplicate key: " + key)
        line = key + "=" + changes[key]
        seen.add(key)
    lines.append(line)
if seen != changes.keys():
    raise SystemExit("missing keys: " + str(changes.keys() - seen))
dst.write_text("\n".join(lines) + "\n")
PY

cd "$rkbin"
./tools/ddrbin_tool rk3588 -g "$out/original-params.txt" "$original"
./tools/ddrbin_tool rk3588 "$out/params.txt" "$blob"
./tools/ddrbin_tool rk3588 -g "$out/patched-params.txt" "$blob"

# Verify actual readback, not just the input parameters.
python3 - "$out/patched-params.txt" <<'PY'
import pathlib, sys
values = {}
for line in pathlib.Path(sys.argv[1]).read_text().splitlines():
    if "=" in line and not line.lstrip().startswith(("#", ";")):
        k, v = line.split("=", 1)
        values[k.strip()] = v.strip()
expected = {"lp4_freq": 528, "lp4x_freq": 528,
            "boot_fsp": 0, "dfs_disable": 1}
for key, want in expected.items():
    raw = values.get(key, "")
    try:
        got = int(raw, 0)
    except ValueError:
        try:
            got = int(raw, 10)
        except ValueError:
            raise SystemExit(f"Cannot verify {key}: {raw!r}; inspect patched-params.txt")
    if got != want:
        raise SystemExit(f"Unexpected {key}: {got} != {want}")
    print(f"{key}={got}")
PY

sed \
    -e 's@^Path1=bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin.*@Path1=../../out/ddr528/ddr528.bin@' \
    -e 's@^Path1=bin/rk35/rk3588_usbplug_v1.11.bin.*@Path1=../../out/v06/usb472.bin@' \
    -e 's@^FlashData=.*@FlashData=../../out/ddr528/ddr528.bin@' \
    -e 's@^FlashBoot=.*@FlashBoot=../../out/v06/usb472.bin@' \
    -e 's@^PATH=.*@PATH=../../out/ddr528/rk3588-baremetal-ram-only.bin@' \
    RKBOOT/RK3588MINIALL.ini > "$out/ram-only.ini"

rm -f "$out/rk3588-baremetal-ram-only.bin"
./tools/boot_merger ../../out/ddr528/ram-only.ini
test -s "$out/rk3588-baremetal-ram-only.bin"
echo "RAM ONLY: sudo rkdeveloptool db out/ddr528/rk3588-baremetal-ram-only.bin"
