#!/usr/bin/env bash
# Build controls without modifying the current firmware or writing board storage.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
mode="${1:-baseline}"
case "$mode" in
  baseline|probe12k) ;;
  *) echo "usage: bash $0 baseline|probe12k" >&2; exit 2 ;;
esac
out="out/sram-control-$mode"
mkdir -p "$out"
if [[ "$mode" == baseline ]]; then
    # Exact source revision that printed ASM ENTRY through SRAM READY on target.
    rev=bf12aeed7e504bfe0ef629afc8571bcc5dfdb4a3
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    git archive "$rev" boot/sram boot/sram.mk | tar -x -C "$tmp"
    make -C "$tmp" -f boot/sram.mk all
    cp "$tmp/out/sram-shell/sram.bin" "$out/code471.bin"
else
    # Known-good assembly probe, padded to same CODE471 payload length as v0.2.1.
    make -f boot/code471-probe.mk all
    python3 - "$out/code471.bin" <<'PY'
import pathlib, sys
b = pathlib.Path("out/code471-probe/code471.bin").read_bytes()
assert 0 < len(b) <= 12288
pathlib.Path(sys.argv[1]).write_bytes(b + bytes(12288-len(b)))
PY
fi
sed -e "s@^Path1=bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin.*@Path1=../../$out/code471.bin@" \
    -e "s@^FlashData=.*@FlashData=../../$out/code471.bin@" \
    -e "s@^PATH=.*@PATH=../../$out/loader.bin@" \
    reference/rkbin/RKBOOT/RK3588MINIALL.ini > "$out/control.ini"
grep -Fx "Path1=../../$out/code471.bin" "$out/control.ini"
grep -Fx "FlashData=../../$out/code471.bin" "$out/control.ini"
rm -f "$out/loader.bin"
(cd reference/rkbin && ./tools/boot_merger "../../$out/control.ini")
test -s "$out/loader.bin"
wc -c "$out/code471.bin" "$out/loader.bin"
sha256sum "$out/code471.bin" "$out/loader.bin"
echo "RAM ONLY. Use fresh MaskROM; observe UART. No storage writes."
echo "sudo timeout -k 2s 15s rkdeveloptool db $out/loader.bin"
