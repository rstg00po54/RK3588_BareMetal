CROSS_COMPILE ?= aarch64-linux-gnu-
OUT := out/sram-shell
RKBIN := reference/rkbin
CFLAGS := -Os -ffreestanding -fpie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mgeneral-regs-only
.PHONY: all pack clean
all: $(OUT)/sram.bin
$(OUT):
	mkdir -p $@
$(OUT)/entry.o: boot/sram/entry.S | $(OUT)
	$(CROSS_COMPILE)gcc -c -ffreestanding -o $@ $<
$(OUT)/main.o: boot/sram/main.c | $(OUT)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c -o $@ $<
$(OUT)/sram.elf: $(OUT)/entry.o $(OUT)/main.o boot/sram/link.ld
	$(CROSS_COMPILE)ld -T boot/sram/link.ld -o $@ $(OUT)/entry.o $(OUT)/main.o
$(OUT)/sram.bin: $(OUT)/sram.elf
	$(CROSS_COMPILE)objcopy -O binary $< $@
	# Word-pad the image for serial protocol.
	python3 -c 'import pathlib; p=pathlib.Path("$@"); b=p.read_bytes(); p.write_bytes(b+bytes((-len(b))%4))'
	test -s $@
	test "$$(wc -c < $@)" -le 65536
pack: $(OUT)/sram.bin
	sed -e 's@^Path1=bin/rk35/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin.*@Path1=../../out/sram-shell/sram.bin@' -e 's@^FlashData=.*@FlashData=../../out/sram-shell/sram.bin@' -e 's@^PATH=.*@PATH=../../out/sram-shell/rk3588-sram-shell.bin@' $(RKBIN)/RKBOOT/RK3588MINIALL.ini > $(OUT)/probe.ini
	rm -f $(OUT)/rk3588-sram-shell.bin
	cd $(RKBIN) && ./tools/boot_merger ../../out/sram-shell/probe.ini
	test -s $(OUT)/rk3588-sram-shell.bin
clean:
	rm -rf $(OUT)
