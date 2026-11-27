# SRAM ABI1 dry-run only: injected mock registers; no physical MMIO.
# Use the same GNU cross-toolchain as boot/sram-app.mk.
CROSS_COMPILE ?= aarch64-linux-gnu-
AARCH64_CC ?= $(CROSS_COMPILE)gcc
AARCH64_LD ?= $(CROSS_COMPILE)ld
AARCH64_OBJCOPY ?= $(CROSS_COMPILE)objcopy
D := boot/ddr_cold
O := out/sram-ddr-dry
CFLAGS := -Os -std=c11 -Wall -Wextra -Werror -ffreestanding -fpie -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mgeneral-regs-only -mstrict-align -fstack-usage
NAMES := probe528 first528 ctl528 phy528 dfi_fragments freq528 train528 ca_scan train528_mode8 lp4x_geometry test_probe528
OBJS := $(addprefix $(O)/,$(addsuffix .o,$(NAMES))) $(O)/entry.o $(O)/ddr_memops.o
.PHONY: all clean
all: $(O)/ddr528-dry.bin
$(O):
	mkdir -p $@
$(O)/%.o: $(D)/%.c $(wildcard $(D)/*.h) boot/sram/app_abi.h | $(O)
	$(AARCH64_CC) $(CFLAGS) -DPROBE528_EMBEDDED -c $< -o $@
$(O)/ddr_memops.o: boot/sram_app/ddr_memops.c | $(O)
	$(AARCH64_CC) $(CFLAGS) -c $< -o $@
$(O)/entry.o: boot/sram_app/entry.S | $(O)
	$(AARCH64_CC) -c $< -o $@
$(O)/ddr528-dry.elf: $(OBJS) boot/sram_app/link.ld
	$(AARCH64_LD) -T boot/sram_app/link.ld --no-undefined -Map=$(O)/ddr528-dry.map -o $@ $(OBJS)
$(O)/ddr528-dry.bin: $(O)/ddr528-dry.elf
	$(AARCH64_OBJCOPY) -O binary $< $@
	test "$$(wc -c < $@)" -le 61440
clean:
	rm -rf $(O)
