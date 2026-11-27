#include <stdint.h>
#define UART 0xfeb50000UL
#define USB 0xfc000000UL
extern int probe32(uintptr_t addr, uint32_t *value);
extern __attribute__((visibility("hidden"))) char _start[], __stack_top[];
static volatile uint32_t bss_test;
static uint32_t ur(unsigned off) {
    return *(volatile uint32_t *)(UART + off);
}

static void uw(unsigned off, uint32_t v) {
    *(volatile uint32_t *)(UART + off) = v;
}

static void tx(char c) {
    for (unsigned n = 0; n < 1000000; ++n)
        if (ur(0x14) & 0x20) {
            uw(0, (unsigned char)c);
            return;
        }
}

static void put(char c) {
    if (c == '\n') tx('\r');
    tx(c);
}

static void puts0(const char *s) {
    while (*s) put(*s++);
}

static void hex(uint64_t x, unsigned n) {
    const char *d = "0123456789ABCDEF";
    puts0("0x");
    while (n) {
        --n;
        put(d[(x >> (4 * n)) & 15]);
    }
}

static void val(const char *name, uint64_t x, unsigned n) {
    puts0(name);
    puts0(" = ");
    hex(x, n);
    put('\n');
}
#define SYS(reg) ({ uint64_t x; __asm__ volatile("mrs %0, " #reg : "=r"(x)); x; })
static void cpu(void) {
    val("CurrentEL", SYS(CurrentEL) >> 2, 1);
    val("MIDR_EL1", SYS(MIDR_EL1), 16);
    val("MPIDR_EL1", SYS(MPIDR_EL1), 16);
    uint64_t s = SYS(SCTLR_EL3);
    val("SCTLR_EL3", s, 16);
    val("MMU", s & 1, 1);
    val("D-cache", (s >> 2) & 1, 1);
    val("I-cache", (s >> 12) & 1, 1);
    val("VBAR_EL3", SYS(VBAR_EL3), 16);
    val("CNTPCT_EL0", SYS(CNTPCT_EL0), 16);
    val("CNTFRQ_EL0", SYS(CNTFRQ_EL0), 8);
    val("image", (uintptr_t)_start, 16);
    val("stack top", (uintptr_t)__stack_top, 16);
}

void boot_fatal(uint64_t esr, uint64_t elr, uint64_t far) {
    puts0("\n[FATAL] EL3 exception\n");
    val("ESR", esr, 16);
    val("ELR", elr, 16);
    val("FAR", far, 16);
    for (;;) __asm__ volatile("wfe");
}

static int readreg(const char *name, unsigned off, uint32_t *v) {
    puts0(name);
    puts0(" @ ");
    hex(USB + off, 8);
    puts0(" = ");
    if (!probe32(USB + off, v)) {
        puts0("ABORT\n");
        return 0;
    }
    hex(*v, 8);
    put('\n');
    return 1;
}

static void usb(void) {
    uint32_t v;
    puts0("[USB] read-only OTG0 snapshot; no reset or event acknowledgement\n");
    if (!readreg("GSNPSID", 0xc120, &v)) return;
    if ((v >> 16) != 0x5533 && (v >> 16) != 0x3331) {
        puts0("Unknown core ID; stopping\n");
        return;
    }
    if (readreg("GCTL", 0xc110, &v)) val("  port capability", (v >> 12) & 3, 1);
    readreg("GSTS", 0xc118, &v);
    readreg("GHWPARAMS0", 0xc140, &v);
    readreg("GHWPARAMS1", 0xc144, &v);
    readreg("GHWPARAMS3", 0xc14c, &v);
    readreg("GUSB2PHYCFG0", 0xc200, &v);
    readreg("GUSB3PIPECTL0", 0xc2c0, &v);
    readreg("GEVNTADRLO0", 0xc400, &v);
    readreg("GEVNTADRHI0", 0xc404, &v);
    readreg("GEVNTSIZ0", 0xc408, &v);
    readreg("GEVNTCOUNT0", 0xc40c, &v);
    if (readreg("DCFG", 0xc700, &v)) val("  device address", (v >> 3) & 127, 2);
    if (readreg("DCTL", 0xc704, &v)) val("  run/stop", v >> 31, 1);
    readreg("DEVTEN", 0xc708, &v);
    if (readreg("DSTS", 0xc70c, &v)) {
        val("  speed code", v & 7, 1);
        val("  link state", (v >> 18) & 15, 1);
        val("  halted", (v >> 22) & 1, 1);
    }
    readreg("DALEPENA", 0xc720, &v);
    puts0("[USB] done; event buffer not dereferenced\n");
}

static int eq(const char *a, const char *b) {
    while (*a && *a == * b) {
        ++a;
        ++b;
    }
    return *a == * b;
}

static void command(const char *s) {
    if (eq(s, "help")) puts0("help cpu usb uart echo\n");
    else if (eq(s, "cpu")) cpu();
    else if (eq(s, "usb")) usb();
    else if (eq(s, "uart")) {
        val("LCR", ur(0x0c), 8);
        val("LSR", ur(0x14), 8);
    }
    else if (eq(s, "echo")) puts0("UART RX OK\n");
    else if (*s) puts0("Unknown command\n");
}

void boot_main(void) {
    char line[64];
    unsigned n = 0;
    int cr = 0;
    puts0("\n[RK3588] OpenBoot v0.2 C ENTRY OK\n");
    puts0(bss_test == 0 ? "[BOOT] BSS zero PASS\n": "[BOOT] BSS zero FAIL\n");
    cpu();
    puts0("[UART] type help; USB remains untouched until usb command\ncc@RK3588:~$ ");
    for (;;) {
        if (!(ur(0x14) & 1)) continue;
        unsigned char c = (unsigned char)ur(0);
        if (c == '\n' && cr) {
            cr = 0;
            continue;
        }
        cr = (c == '\r');
        if (c == '\r' || c == '\n') {
            put('\n');
            line[n] = 0;
            command(line);
            n = 0;
            puts0("cc@RK3588:~$ ");
        }
        else if (c == 8 || c == 127) {
            if (n) {
                --n;
                puts0("\b \b");
            }
        }
        else if (c >= 32 && c < 127 && n < sizeof(line) - 1) {
            line[n++] = (char)c;
            put((char)c);
        }
    }
}
