#!/usr/bin/env bash
# Build a separate RAM-only loader using a locally patched vendor DDR copy.
set -euo pipefail

cd "$(dirname "$0")/.."
root="$PWD"
rkbin="$root/reference/rkbin"
mode="${1:-freq-only}"
case "$mode" in
    freq-only|dfs-only) ;;
    *) echo "Usage: bash tools/build-ddr-test.sh [freq-only|dfs-only]" >&2; exit 2 ;;
esac
payload_version="${2:-v06}"
case "$payload_version" in
    v06|v07) ;;
    *) echo "Expected payload v06 or v07" >&2; exit 2 ;;
esac
name="ddr-$mode"
if [[ "$payload_version" != v06 ]]; then
    name="$name-$payload_version"
fi
out="$root/out/$name"
original="$rkbin/bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin"
blob="$out/ddr-test.bin"
mkdir -p "$out"

# The SDK includes the Python implementation, not the legacy executable.
ddrtool="$rkbin/tools/ddrbin_tool.py"
if [[ ! -f "$ddrtool" ]]; then
    echo "Missing DDR tool: $ddrtool" >&2
    exit 1
fi

make -f "boot/$payload_version.mk" all
cp "$original" "$blob"

# Keep unrelated parameters unchanged, use the SDK's own template.
python3 - "$rkbin/tools/ddrbin_param.txt" "$out/params.txt" "$mode" <<'PY'
import pathlib, sys
src, dst = map(pathlib.Path, sys.argv[1:3])
changes = {"lp4x_freq": "528"} if sys.argv[3] == "freq-only" else {"dfs_disable": "1"}
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
rm -f "$out/original-params.txt" "$out/patched-params.txt" "$out/rk3588-baremetal-ram-only.bin"
python3 "$ddrtool" rk3588 -g "$out/original-params.txt" "$original"
test -s "$out/original-params.txt"
python3 "$ddrtool" rk3588 "$out/params.txt" "$blob"
python3 "$ddrtool" rk3588 -g "$out/patched-params.txt" "$blob"
test -s "$out/patched-params.txt"

# Verify actual readback, not just the input parameters.
python3 - "$out/original-params.txt" "$out/patched-params.txt" "$mode" <<'PY'
import pathlib, sys
def read_params(path):
    values = {}
    for line in pathlib.Path(path).read_text().splitlines():
        if "=" in line and not line.lstrip().startswith(("#", ";")):
            k, v = line.split("=", 1)
            values[k.strip()] = v.strip()
    return values

before, after = map(read_params, sys.argv[1:3])
expected = {"lp4x_freq": 528} if sys.argv[3] == "freq-only" else {"dfs_disable": 1}
def number(raw):
    try:
        return int(raw, 0)
    except ValueError:
        return int(raw, 10)

for key in ("lp4_freq", "lp4x_freq", "boot_fsp", "dfs_disable"):
    if key not in before or key not in after:
        raise SystemExit("Cannot verify " + key)
    a, b = number(before[key]), number(after[key])
    if b != expected.get(key, a):
        raise SystemExit(f"Unexpected {key}: {a} -> {b}")
    print(f"{key}: {a} -> {b}")

# Check all other non-version configuration values printed by the tool.
for key in sorted(before.keys() & after.keys()):
    if key not in expected and before[key] != after[key]:
        if not any(word in key.lower() for word in ("version", "verinfo", "time")):
            raise SystemExit(f"Unexpected change: {key}: {before[key]} -> {after[key]}")

for key, want in expected.items():
    if number(before[key]) == want:
        raise SystemExit(f"{key} already equals {want}; not a useful control experiment")

PY

sed \
    -e "s@^Path1=bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin.*@Path1=../../out/$name/ddr-test.bin@" \
    -e "s@^Path1=bin/rk35/rk3588_usbplug_v1.11.bin.*@Path1=../../out/$payload_version/usb472.bin@" \
    -e "s@^FlashData=.*@FlashData=../../out/$name/ddr-test.bin@" \
    -e "s@^FlashBoot=.*@FlashBoot=../../out/$payload_version/usb472.bin@" \
    -e "s@^PATH=.*@PATH=../../out/$name/rk3588-baremetal-ram-only.bin@" \
    RKBOOT/RK3588MINIALL.ini > "$out/ram-only.ini"

rm -f "$out/rk3588-baremetal-ram-only.bin"
./tools/boot_merger "../../out/$name/ram-only.ini"
test -s "$out/rk3588-baremetal-ram-only.bin"
echo "RAM ONLY: sudo rkdeveloptool db out/$name/rk3588-baremetal-ram-only.bin"
