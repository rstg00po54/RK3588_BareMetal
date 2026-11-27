# Host-only experiment. No target compiler or device connection.
CC ?= cc
PKG_CONFIG ?= pkg-config
OUT := out/host/ddr-v118-oracle
.PHONY: all clean
all: $(OUT)
$(OUT): tools/ddr-v118-oracle.c
	@mkdir -p $(@D)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 $$($(PKG_CONFIG) --cflags unicorn) $< -o $@ $$($(PKG_CONFIG) --libs unicorn)
clean:
	rm -f $(OUT)
