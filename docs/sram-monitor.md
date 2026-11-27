# SRAM Monitor v0.6.0: returning applications

This is a first hardware experiment, not an isolated OS/process model.
Existing full monitor update (sram-usb run) remains API 2 and still
disconnects/re-enumerates. New exec uses the same verified upload
protocol, then request 0x59 to call the image and 0x5a to read result.
No USB reset or new USB handle is intended on exec.

Memory layout: current monitor stays in its original 64 KiB slot
(FF001000 or FF011000). The other slot is reused for one app, with
first 60 KiB maximum image and last 4 KiB stack. No additional SRAM
banks, DDR, or storage are used. The app overwrites any staged
alternate firmware. It is not a persistent installed application.

ABI 1: little-endian header words SAP1 (31504153), 1, 16, 0.
Entry at offset 16, position independent AArch64 AAPCS64 code.
x0 points to struct sram_app_api; return unsigned 32-bit result in w0.
Use only trusted, short returning apps. Monitor puts/ticks callbacks
are available. No automatic BSS initialization, scheduler, privilege
isolation, interrupt-driven USB servicing, or exception recovery.
Do not change MMU/cache/EL/interrupt masks, USB, stack beyond its
reserved range, or monitor memory. Long or faulty apps may stall USB.
An ordinary return code is not itself proof of app-specific success.

Build and test:
    make -f boot/sram.mk
    make -f tools/sram-usb.mk
    sudo install -m 755 out/host/sram-usb /usr/local/bin/sram-usb
    sudo sram-usb run out/sram-shell/sram.bin
    make -f boot/sram-app.mk
    sudo sram-usb exec out/sram-app/app.bin
    sudo sram-usb exec out/sram-app/app.bin
    sudo sram-usb ping

Expected UART: [APP] call; USB remains running, hello, returned 12345678.
Expected host: APP RETURN PASS; result=0x12345678; monitor slot unchanged.
Measure exec time and verify no USB disconnect in dmesg.
Do not resend after an uncertain execute or failed return.
Initial v0.6.0 hello upload verified but app did not return. See v0.6.1 results below.


## v0.6.1 hardware verification

Three incremental assembly probes all compiled and ran on the board,
with monitor in Slot A (FF001000), app entry FF011010 and app stack
top FF021000. Runtime ABI callback addresses were inside the monitor:
puts FF002058, ticks FF002700. User's actual test:

| Image | Bytes | Return | Upload + execute + return |
|---|---:|---|---:|
| probe0.bin | 36 | 12345678 | 5 ms |
| probe1.bin | 80 | 12345679 | 6 ms |
| probe2.bin | 92 | 1234567A | 6 ms |

All CRC32 verifications passed; host confirmed result and ping on the
same USB handle, unchanged monitor slot. UART confirmed callback output
"[APP] Assembly callback PASS". No intentional controller restart.
The previous hello failure's exact root cause remains unproven; runtime
callback pointer stores in v0.6.1 are consistent with the suspected
absolute-address template issue. Rebuild and test the C hello next.

Repeated unsupported setup 80 06 0F00 0000 0005 is GET_DESCRIPTOR(BOS),
not an app execution error. USB bcdUSB is 0200, and no BOS descriptor
is advertised. Leave the current STALL behavior intact for now; log
rate limiting is active. Do not fake SuperSpeed or LPM capability.

Monitor slot B execution and ordinary C hello have not yet been tested
with v0.6.1. No DDR writes were performed by these probes.
