# Independent SRAM Shell v0.1 and UART hot update

Independent CODE471 probe passed on target at FF001000, EL3, scratch PASS.
Host cold_steps mock passed. No DDR initialized by this shell.

Use board's UART2 M0, 1500000 8N1. Entry code uses verified UART setup,
a private stack, BSS zeroing, EL3 vector table, bounded receive and CRC32.

SDK reference/linux-6.1/rockchip/rk3588s.dtsi declares system SRAM
FF001000 + EF000. We choose two nonoverlapping 64KiB slots:
A FF001000..FF010FFF, B FF011000..FF020FFF. The second slot has not
yet been validated at cold start. No write outside these slots by loader.
No return to ROM, no storage writes, no DDR initialization.
Use only trusted binaries built by boot/sram.mk; CRC is not authentication.
Image must be position-independent, including stack/BSS and exception vectors.
Receiver refuses MMU or data cache enabled. I-cache invalidated before jump.
UART timeout depends on 24MHz architectural counter progressing.
No full SRAM capacity or reset-mode test yet.

First load:
```sh
make -f boot/sram.mk pack
sudo timeout -k 2s 15s rkdeveloptool db out/sram-shell/rk3588-sram-shell.bin
```
Enter MaskROM physically once. USB timeout expected if shell is running.
UART commands: help, info, test, load, run.
After READY, close serial terminal and run (replace port with actual device):
```sh
make -f boot/sram.mk all
python3 tools/sram-upload.py /dev/ttyUSB0 out/sram-shell/sram.bin
```
Second update should alternate from B to A. Reopen terminal to inspect info.
If upload fails, active shell remains intact unless target bus stalls;
recover with reset/MaskROM. A full power cycle loses SRAM contents.
Do not call reset-to-MaskROM by writing boot-mode tag alone: SDK U-Boot
bootrom.c says ROM itself never checks that register; early bootloader
must cooperate. USB hot-download can follow after UART route works.

Not compiled or run on target at commit. Original working loaders untouched.
