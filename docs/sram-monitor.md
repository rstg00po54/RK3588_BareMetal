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
First build and actual target verification are pending.
