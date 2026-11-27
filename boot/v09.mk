CROSS_COMPILE ?= aarch64-linux-gnu-
OUT := out/v09
RKBIN := reference/rkbin
DDR := bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin
CFLAGS := -Os -ffreestanding -fpie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mgeneral-regs-only
PAYLOAD := $(OUT)/usb472.bin
LOADER := $(OUT)/rk3588-baremetal-ram-only.bin
.PHONY: all pack clean
all: $(PAYLOAD)
$(OUT):
	mkdir -p $@
$(OUT)/entry.o: boot/v02/entry.S | $(OUT)
	$(CROSS_COMPILE)gcc -c -ffreestanding -o $@ $<
$(OUT)/main.o: boot/v09/main.c boot/v08/ddr_phy.h boot/v09/ddr_seq.h boot/v06/ddr_clock.h boot/v05/ddr_diag.h boot/v04/usb4.h | $(OUT)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c -o $@ $<
$(OUT)/usb472.elf: $(OUT)/entry.o $(OUT)/main.o boot/v02/link.ld
	$(CROSS_COMPILE)ld -T boot/v02/link.ld -o $@ $(OUT)/entry.o $(OUT)/main.o
$(PAYLOAD): $(OUT)/usb472.elf
	$(CROSS_COMPILE)objcopy -O binary $< $@
pack: $(LOADER)
$(LOADER): $(PAYLOAD) $(RKBIN)/RKBOOT/RK3588MINIALL.ini $(RKBIN)/$(DDR)
	sed -e 's@^Path1=bin/rk35/rk3588_usbplug_v1.11.bin.*@Path1=../../out/v09/usb472.bin@' -e 's@^FlashBoot=.*@FlashBoot=../../out/v09/usb472.bin@' -e 's@^PATH=.*@PATH=../../out/v09/rk3588-baremetal-ram-only.bin@' $(RKBIN)/RKBOOT/RK3588MINIALL.ini > $(OUT)/ram-only.ini
	cd $(RKBIN) && ./tools/boot_merger ../../out/v09/ram-only.ini
	test -s $@
	@echo 'RAM ONLY: db, never flash.'
clean:
	rm -rf $(OUT)
