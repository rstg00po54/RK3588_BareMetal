# CODE472 RAM-only smoke test

This is a replacement CODE472 payload, not yet a USB protocol driver. Keep the
known-good RK3588 DDR v1.18 (LPDDR4 1848 MHz) as CODE471. Do not run the vendor
USBPlug, SPL, BL31 or BL32. The flash metadata entries exist for boot_merger
compatibility only. Never flash the resulting image.

Build from the repository root with GNU AArch64 tools:
```sh
make pack
sudo rkdeveloptool ld
sudo rkdeveloptool db out/rk3588-baremetal-ram-only.bin
```
Check Maskrom mode before db. Keep the UART monitor open at 1500000 8N1.
A successful test prints CODE472 AARCH64 ENTRY OK, CurrentEL, and a 128-byte
scratch read/write result. The payload uses no stack and inherits the UART
clock, pinmux and baud rate from the known-working DDR blob (UART2
0xfeb50000, confirmed in the local U-Boot LubanCat-4 configuration). The
scratch test is not a full DRAM test and may be satisfied by cache.

The payload intentionally never enumerates a new USB device. USB disappearing,
or rkdeveloptool timing out waiting for Loader mode, is expected if the UART
shows the banner. Return to MaskROM via power cycle/board button for another
test. No storage writes are performed by the payload.

References:
- https://lists.u-boot-project.org/pipermail/u-boot/2025-February/581229.html
- https://bootlin.com/blog/recovery-boot-on-rockchip-socs/
- reference/u-boot/configs/lubancat-4-rk3588s-msata_defconfig
- reference/rkbin/RKBOOT/RK3588MINIALL.ini

The hardware execution and vendor packer compatibility with the small custom
payload are still unverified. The UART state and CPU entry EL must be measured
on target. Future phases: C runtime, UART RX, memory test, then a USB device
driver and a RAM-only host protocol.
