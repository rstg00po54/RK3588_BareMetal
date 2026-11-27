CROSS_COMPILE ?= aarch64-linux-gnu-
OUT := out
RKBIN := reference/rkbin
DDR := bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin
PAYLOAD := $(OUT)/usb472.bin
LOADER := $(OUT)/rk3588-baremetal-ram-only.bin
.PHONY: all pack clean
all: $(PAYLOAD)
$(OUT):
	mkdir -p $@
$(OUT)/usb472.o: boot/usb472.S | $(OUT)
	$(CROSS_COMPILE)gcc -c -ffreestanding -o $@ $<
$(OUT)/usb472.elf: $(OUT)/usb472.o
	$(CROSS_COMPILE)ld -Ttext=0 --entry=_start -o $@ $<
$(PAYLOAD): $(OUT)/usb472.elf
	$(CROSS_COMPILE)objcopy -O binary -j .text $< $@
pack: $(LOADER)
$(LOADER): $(PAYLOAD) $(RKBIN)/RKBOOT/RK3588MINIALL.ini $(RKBIN)/$(DDR)
	# Reuse vendor format/flags; both CODE472 and unused FlashBoot point to our payload.
	sed -e 's@^Path1=bin/rk35/rk3588_usbplug_v1.11.bin.*@Path1=../../out/usb472.bin@' -e 's@^FlashBoot=.*@FlashBoot=../../out/usb472.bin@' -e 's@^PATH=.*@PATH=../../out/rk3588-baremetal-ram-only.bin@' $(RKBIN)/RKBOOT/RK3588MINIALL.ini > $(OUT)/ram-only.ini
	cd $(RKBIN) && ./tools/boot_merger ../../out/ram-only.ini
	test -s $@
	@echo 'RAM ONLY: use rkdeveloptool db; NEVER use this image with ul or flash commands.'
clean:
	rm -rf $(OUT)
