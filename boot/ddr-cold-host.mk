# Native host regression tests. No device access or target build.
CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O2
D := boot/ddr_cold
O := out/host/ddr-cold
WARN := -std=c11 -Wall -Wextra -Werror -UNDEBUG
TESTS := $(addprefix $(O)/,dfi switch ca geometry posttrain ctl528 phy528)
.PHONY: all test clean
all: $(TESTS)
$(O):
	mkdir -p $@
$(O)/dfi: $(D)/dfi_fragments.c $(D)/test_dfi_fragments.c $(D)/cold_steps.h | $(O)
	$(CC) $(CFLAGS) $(WARN) $(filter %.c,$^) -o $@
$(O)/switch: $(D)/freq528.c $(D)/test_freq528.c $(D)/cold_steps.h | $(O)
	$(CC) $(CFLAGS) $(WARN) $(filter %.c,$^) -o $@
$(O)/ca: $(D)/ca_scan.c $(D)/test_ca_scan.c | $(O)
	$(CC) $(CFLAGS) $(WARN) $^ -o $@
$(O)/geometry: $(D)/test_lp4x_geometry.c $(D)/lp4x_geometry.c | $(O)
	$(CC) $(CFLAGS) $(WARN) $< -o $@
$(O)/posttrain: $(D)/posttrain.c $(D)/test_posttrain.c | $(O)
	$(CC) $(CFLAGS) $(WARN) $^ -o $@
$(O)/ctl528: $(D)/ctl528.c $(D)/test_ctl528.c $(D)/cold_steps.h | $(O)
	$(CC) $(CFLAGS) $(WARN) $(filter %.c,$^) -o $@
$(O)/phy528: $(D)/phy528.c $(D)/test_phy528.c $(D)/cold_steps.h | $(O)
	$(CC) $(CFLAGS) $(WARN) $(filter %.c,$^) -o $@
test: all
	$(O)/dfi
	$(O)/switch
	$(O)/ca
	$(O)/geometry
	$(O)/posttrain > $(O)/posttrain-values.txt
	printf '%s  %s\n' 755794118fc2590b4258b92904b2ab3349a2ec0430df6b808976806a98b1d1b7 $(O)/posttrain-values.txt | sha256sum -c -
	$(PYTHON) $(D)/test_ctl528_vectors.py $(O)/ctl528
	$(PYTHON) $(D)/test_phy528_vectors.py $(O)/phy528
clean:
	rm -rf $(O)
