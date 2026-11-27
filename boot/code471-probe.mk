CROSS_COMPILE ?= aarch64-linux-gnu-
OUT := out/code471-probe
RKBIN := reference/rkbin
PROBE := $(OUT)/code471.bin
LOADER := $(OUT)/rk3588-code471-probe.bin
.PHONY: all pack clean
all: $(PROBE)
$(OUT):
	mkdir -p $@
$(OUT)/probe.o: boot/ddr_cold/code471_probe.S | $(OUT)
	$(CROSS_COMPILE)gcc -c -ffreestanding -o $@ $<
$(OUT)/probe.elf: $(OUT)/probe.o
	$(CROSS_COMPILE)ld -Ttext=0 --entry=_start -o $@ $<
$(PROBE): $(OUT)/probe.elf
	$(CROSS_COMPILE)objcopy -O binary -j .text $< $@
	test -s $@
	test "$$(wc -c < $@)" -le 4096
pack: $(PROBE)
	# The probe deliberately never returns. Original CODE472/FlashBoot are
	# placeholders required by the vendor format, never intended to execute.
	sed -e 's@^Path1=bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin.*@Path1=../../out/code471-probe/code471.bin@' -e 's@^FlashData=.*@FlashData=../../out/code471-probe/code471.bin@' -e 's@^PATH=.*@PATH=../../out/code471-probe/rk3588-code471-probe.bin@' $(RKBIN)/RKBOOT/RK3588MINIALL.ini > $(OUT)/probe.ini
	grep -Fx 'Path1=../../out/code471-probe/code471.bin' $(OUT)/probe.ini
	grep -Fx 'FlashData=../../out/code471-probe/code471.bin' $(OUT)/probe.ini
	rm -f $(LOADER)
	cd $(RKBIN) && ./tools/boot_merger ../../out/code471-probe/probe.ini
	test -s $(LOADER)
	@echo 'RAM ONLY. db may hang by design. Never flash.'
clean:
	rm -rf $(OUT)
