HOSTCC ?= cc
PKG_CONFIG ?= pkg-config
HOST_CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror
OUT := out/host/sram-usb
.PHONY: all test clean
all: $(OUT)
$(OUT): tools/sram-usb.c tools/sram-usb.mk
	@$(PKG_CONFIG) --exists libusb-1.0 || { echo 'Install libusb-1.0-0-dev and pkg-config'; exit 1; }
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) $$($(PKG_CONFIG) --cflags libusb-1.0) $< -o $@ $$($(PKG_CONFIG) --libs libusb-1.0)
test: $(OUT)
	$(OUT) selftest
clean:
	rm -f $(OUT)
