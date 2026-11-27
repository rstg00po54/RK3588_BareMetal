CROSS_COMPILE ?= aarch64-linux-gnu-
OUT := out/sram-app
APP ?= boot/sram_app/hello.c
CFLAGS := -Os -ffreestanding -fpie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mgeneral-regs-only
.PHONY: all clean FORCE
all: $(OUT)/app.bin
$(OUT):
	mkdir -p $@
$(OUT)/entry.o: boot/sram_app/entry.S | $(OUT)
	$(CROSS_COMPILE)gcc -c -o $@ $<
$(OUT)/app.o: $(APP) boot/sram/app_abi.h FORCE | $(OUT)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c -o $@ $(APP)
$(OUT)/app.elf: $(OUT)/entry.o $(OUT)/app.o boot/sram_app/link.ld
	$(CROSS_COMPILE)ld -T boot/sram_app/link.ld -o $@ $(OUT)/entry.o $(OUT)/app.o
$(OUT)/app.bin: $(OUT)/app.elf
	$(CROSS_COMPILE)objcopy -O binary $< $@
	test "$$(wc -c < $@)" -le 61440
clean:
	rm -rf $(OUT)
