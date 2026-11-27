# Independent CODE471 SRAM entry probe (stage 0)

Prior host test of cold_steps.c: DDR cold steps mock PASS (user Ubuntu).
This probe does NOT call cold_steps.c. Its purpose is to prove ROM delivery
and execution of an independent first-stage image before DDR initialization.

Original DDR v1.18 config at file offset 0x115d0 has selector byte
0x115da=13. The selected word at 0x11604=0x2016e360 encodes UART2,
mux0, baud 1500000. Pinmux path 0x650..0x674 writes FD5F4000=0x0ff00880
and FD5F800C=0x0ff00aa0. UART path 0xfff0..0x1006c programs LCR=0x83,
DLL=1, LCR=3, offset 0x98=1. Probe explicitly clears DLM while DLAB set.
UART clock is inherited, not independently configured. If silent,
that does not establish that ROM failed to enter CODE471.

Probe is position independent, stackless, under 4 KiB, uses only UART
pinmux/UART MMIO and a 256-byte image-owned scratch. It prints actual
entry address and EL. It intentionally halts forever, even on PASS.
No memory controller writes, PLL writes, storage writes, or return.
No flash commands. Original CODE472 and FlashBoot remain in package as
format placeholders, and cannot run if the probe reaches intentional halt.

Build:
```sh
make -f boot/code471-probe.mk pack
```
Fully power off, enter MaskROM, open UART 1500000 8N1, then:
```sh
sudo timeout -k 2s 15s rkdeveloptool db out/code471-probe/rk3588-code471-probe.bin
```
Timeout is expected if ROM cannot continue because probe never returns.
Observe UART and actual image address. Reset power afterward.
If silent, record host result and UART output; do not assume execution
failure. The probe is not a functional DDR loader. Packaging compatibility
and target execution are not verified at commit.
