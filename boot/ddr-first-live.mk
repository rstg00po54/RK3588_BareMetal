# Standalone experimental first pass; locked by default. Not an ABI app.
# No reviewed DDR528_COLD_SIGNATURE is shipped. Do not arm on partial DDR.
CROSS_COMPILE ?= aarch64-linux-gnu-
AARCH64_CC ?= $(CROSS_COMPILE)gcc
AARCH64_LD ?= $(CROSS_COMPILE)ld
AARCH64_OBJCOPY ?= $(CROSS_COMPILE)objcopy
O := out/sram-ddr-live
D := boot/ddr_cold
CFLAGS := -Os -std=c11 -Wall -Wextra -Werror -ffreestanding -fpie -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mgeneral-regs-only -mstrict-align -fstack-usage
NAMES := first528 ctl528 phy528 dfi_fragments freq528 cold528_guard
OBJS := $(addprefix $(O)/,$(addsuffix .o,$(NAMES))) $(O)/entry.o $(O)/live.o $(O)/memops.o
.PHONY: all clean FORCE
all: $(O)/ddr528-first-live.bin
$(O):
	mkdir -p $@
$(O)/%.o: $(D)/%.c $(wildcard $(D)/*.h) | $(O)
	$(AARCH64_CC) $(CFLAGS) -c $< -o $@
$(O)/entry.o: boot/sram_app/ddr528_live_entry.S | $(O)
	$(AARCH64_CC) -c $< -o $@
$(O)/live.o: boot/sram_app/ddr528_live.c FORCE | $(O)
	$(AARCH64_CC) $(CFLAGS) $(LIVE_CPPFLAGS) -c $< -o $@
$(O)/memops.o: boot/sram_app/ddr_memops.c | $(O)
	$(AARCH64_CC) $(CFLAGS) -c $< -o $@
$(O)/ddr528-first-live.elf: $(OBJS) boot/sram/link.ld
	$(AARCH64_LD) --no-undefined -T boot/sram/link.ld -o $@ $(OBJS)
$(O)/ddr528-first-live.bin: $(O)/ddr528-first-live.elf
	$(AARCH64_OBJCOPY) -O binary $< $@
	python3 -c 'import pathlib; p=pathlib.Path("$@"); b=p.read_bytes(); p.write_bytes(b+bytes((-len(b))%4))'
	test "$$(wc -c < $@)" -le 65536
clean:
	rm -rf $(O)
