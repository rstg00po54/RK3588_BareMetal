# Read-only SRAM ABI1 preflight. Default GNU cross-toolchain.
CROSS_COMPILE ?= aarch64-linux-gnu-
AARCH64_CC ?= $(CROSS_COMPILE)gcc
AARCH64_LD ?= $(CROSS_COMPILE)ld
AARCH64_OBJCOPY ?= $(CROSS_COMPILE)objcopy
O := out/sram-ddr-preflight
CFLAGS := -Os -std=c11 -Wall -Wextra -Werror -ffreestanding -fpie -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mgeneral-regs-only -mstrict-align -fstack-usage
.PHONY: all clean
all: $(O)/ddr528-preflight.bin
$(O):
	mkdir -p $@
$(O)/entry.o: boot/sram_app/entry.S | $(O)
	$(AARCH64_CC) -c $< -o $@
$(O)/preflight.o: boot/sram_app/ddr528_preflight.c boot/sram/app_abi.h boot/ddr_cold/cold528_guard.h | $(O)
	$(AARCH64_CC) $(CFLAGS) -c $< -o $@
$(O)/guard.o: boot/ddr_cold/cold528_guard.c boot/ddr_cold/cold528_guard.h boot/ddr_cold/cold_steps.h | $(O)
	$(AARCH64_CC) $(CFLAGS) -c $< -o $@
$(O)/memops.o: boot/sram_app/ddr_memops.c | $(O)
	$(AARCH64_CC) $(CFLAGS) -c $< -o $@
$(O)/ddr528-preflight.elf: $(O)/entry.o $(O)/preflight.o $(O)/guard.o $(O)/memops.o boot/sram_app/link.ld
	$(AARCH64_LD) --no-undefined -T boot/sram_app/link.ld -o $@ $(filter %.o,$^)
$(O)/ddr528-preflight.bin: $(O)/ddr528-preflight.elf
	$(AARCH64_OBJCOPY) -O binary $< $@
	test "$$(wc -c < $@)" -le 61440
clean:
	rm -rf $(O)
