/* v0.9: status-only snapshot and image-owned DDR smoke test. */
static const struct ddr_reg ddr_seq_regs[] = {
    {0x118, "SEQ_STATUS_118"},
    {0x120, "SEQ_STATUS_120"},
    {0x184, "SEQ_STATUS_184"},
};
static void ddr_seq(void) {
    puts0("[DDRSEQ] FD8D8000 raw status; no writes\n");
    if (ddr_regs(0xfd8d8000UL, ddr_seq_regs,
                 sizeof(ddr_seq_regs)/sizeof(ddr_seq_regs[0])))
        puts0("[DDRSEQ] snapshot complete\n");
}

static volatile uint32_t mem_scratch[1024] __attribute__((aligned(64)));
static uint32_t mem_pattern(unsigned pass, unsigned i) {
    switch (pass) {
    case 0: return 0;
    case 1: return 0xffffffffU;
    case 2: return 0xaaaaaaaaU;
    case 3: return 0x55555555U;
    case 4: return (uint32_t)(uintptr_t)&mem_scratch[i];
    case 5: return ~(uint32_t)(uintptr_t)&mem_scratch[i];
    default: return 1U << (pass - 6);
    }
}
static void memtest(void) {
    if (SYS(SCTLR_EL3) & 5) {
        puts0("[MEMTEST] requires MMU and D-cache off\n");
        return;
    }
    val("[MEMTEST] scratch", (uintptr_t)mem_scratch, 16);
    val("[MEMTEST] bytes", sizeof(mem_scratch), 8);
    for (unsigned p=0; p<38; ++p) {
        for (unsigned i=0; i<1024; ++i)
            mem_scratch[i]=mem_pattern(p,i);
        __asm__ volatile("dsb sy" ::: "memory");
        for (unsigned i=0; i<1024; ++i) {
            uint32_t got=mem_scratch[i], want=mem_pattern(p,i);
            if (got!=want) {
                val("[MEMTEST] failing pass",p,2);
                val("address",(uintptr_t)&mem_scratch[i],16);
                val("expected",want,8);
                val("actual",got,8);
                puts0("[MEMTEST] FAIL\n");
                return;
            }
        }
    }
    puts0("[MEMTEST] PASS: 38 patterns, 4096 bytes; not full RAM test\n");
}
