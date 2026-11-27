#include <stdint.h>
#define UART 0xfeb50000UL
#define SLOT_A 0xff001000UL
#define SLOT_B 0xff011000UL
#define SLOT_SIZE 0x10000U
#define CHUNK 128U
extern __attribute__((visibility("hidden"))) char _start[], __stack_top[], __image_end[];
static uintptr_t loaded;
static int timer_ok;
static volatile uint32_t bss_test;
static volatile uint32_t scratch[256];
#define SYS(reg) ({ uint64_t x; __asm__ volatile("mrs %0, " #reg : "=r"(x)); x; })
static uint32_t ur(unsigned o){return *(volatile uint32_t *)(UART+o);}
static void tx(char c) {
    for(unsigned n=0;n<1000000;++n) if(ur(0x14)&0x20){
        *(volatile uint32_t *)UART=(unsigned char)c; return;
    }
}
static void out(const char *s){while(*s){if(*s=='\n')tx('\r');tx(*s++);}}
static void hex(uint64_t x){
    const char *d="0123456789ABCDEF";out("0x");
    for(int i=60;i>=0;i-=4)tx(d[(x>>i)&15]);
}
static void val(const char *s,uint64_t x){out(s);hex(x);out("\n");}
static int eq(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return *a==*b;}
static uintptr_t alternate(void) {
    uintptr_t a=(uintptr_t)_start;
    return a==SLOT_A?SLOT_B:a==SLOT_B?SLOT_A:0;
}
static void timer_write(unsigned off,uint32_t v){
    val("[TIMER] write address = ",0xfd8c8000UL+off);
    val("[TIMER] value = ",v);
    *(volatile uint32_t *)(0xfd8c8000UL+off)=v;
    __asm__ volatile("dsb sy" ::: "memory");
    out("[TIMER] write complete\n");
}
static int counter_moves(void){
    uint64_t first=SYS(CNTPCT_EL0);
    for(unsigned i=0;i<10000;++i)
        if(SYS(CNTPCT_EL0)!=first)return 1;
    return 0;
}
static void timer_init(void){
    /* DDR v1.18 0x105f0 / 0x10618: 24 MHz and system timer setup.
     * Do not reset an already running counter during a hot update. */
    out("[TIMER] explicit init: set CNTFRQ_EL0\n");
    uint64_t hz=24000000;
    __asm__ volatile("msr cntfrq_el0, %0; isb"::"r"(hz):"memory");
    out("[TIMER] check counter\n");
    if(!counter_moves()){
        timer_write(0x04,0);
        __asm__ volatile("dsb sy" ::: "memory");
        timer_write(0x14,0xffffffffU);
        timer_write(0x18,0xffffffffU);
        timer_write(0x08,0);
        __asm__ volatile("dsb sy" ::: "memory");
        timer_write(0x04,1);
        __asm__ volatile("dsb sy; isb" ::: "memory");
    }
    timer_ok=counter_moves();
    out(timer_ok?"[SRAM] TIMER PASS\n":"[SRAM] TIMER STALLED; bounded polling fallback\n");
}
static void timer_info(void){
    val("CNTFRQ_EL0 = ",SYS(CNTFRQ_EL0));
    val("CNTPCT_EL0 = ",SYS(CNTPCT_EL0));
    out(counter_moves()?"[SRAM] COUNTER RUNNING\n":"[SRAM] COUNTER STALLED\n");
}
static int rx(void){
    uint64_t start=timer_ok?SYS(CNTPCT_EL0):0;
    /* Absolute loop bound even if the counter stops. */
    for(unsigned spins=0;spins<100000000U;++spins){
        uint32_t l=ur(0x14);
        if(l&0x1e){if(l&1)(void)ur(0);return -1;}
        if(l&1)return ur(0)&255;
        if(timer_ok && SYS(CNTPCT_EL0)-start>=120000000ULL)break;
    }
    return -1;
}
static int get32(uint32_t *v){
    *v=0;
    for(unsigned i=0;i<4;++i){int c=rx();if(c<0)return 0;*v|=(uint32_t)c<<(8*i);}
    return 1;
}
static uint32_t crc_byte(uint32_t crc,unsigned c){
    crc^=c;
    for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320U & (0U-(crc&1)));
    return crc;
}
static void load(void){
    uint32_t size,want,crc=0xffffffffU;
    uintptr_t a=alternate();
    loaded=0;
    if(!a || (SYS(SCTLR_EL3)&5)){
        out("ERR SLOT OR MMU/DCACHE\n");return;
    }
    out("READY\n");
    if(!get32(&size)||!get32(&want)||size<4||size>SLOT_SIZE||(size&3)){
        out("ERR HEADER\n");return;
    }
    out("GO\n");
    volatile uint8_t *dst=(volatile uint8_t *)a;
    for(unsigned i=0;i<size;++i){
        int c=rx();if(c<0){out("ERR RX\n");return;}
        dst[i]=(uint8_t)c;crc=crc_byte(crc,(unsigned)c);
        if((i+1)%CHUNK==0 || i+1==size)out("ACK\n");
    }
    __asm__ volatile("dsb sy" ::: "memory");
    if((crc^0xffffffffU)!=want){out("ERR CRC\n");return;}
    crc=0xffffffffU;
    for(unsigned i=0;i<size;++i)crc=crc_byte(crc,dst[i]);
    if((crc^0xffffffffU)!=want){out("ERR SRAM CRC\n");return;}
    loaded=a;
    out("OK\n");
}
static void run(void){
    if(!loaded){out("ERR NO IMAGE\n");return;}
    uintptr_t a=loaded;loaded=0;
    out("JUMP\n");
    for(unsigned i=0;i<1000000;++i)if(ur(0x14)&0x40)break;
    __asm__ volatile("dsb sy; ic iallu; dsb sy; isb" ::: "memory");
    ((void (*)(void))a)();
    for(;;)__asm__ volatile("wfe");
}
static void info(void){
    val("image = ",(uintptr_t)_start);val("stack top = ",(uintptr_t)__stack_top);
    val("image end = ",(uintptr_t)__image_end);
    val("CurrentEL = ",SYS(CurrentEL)>>2);val("SCTLR_EL3 = ",SYS(SCTLR_EL3));
    val("CNTFRQ_EL0 = ",SYS(CNTFRQ_EL0));val("alternate = ",alternate());
}
static void test(void){
    for(unsigned p=0;p<2;++p){
        for(unsigned i=0;i<256;++i)scratch[i]=i^(p?0xffffffffU:0);
        __asm__ volatile("dsb sy" ::: "memory");
        for(unsigned i=0;i<256;++i)if(scratch[i]!=(i^(p?0xffffffffU:0))){
            out("[SRAM] scratch FAIL\n");return;
        }
    }
    out("[SRAM] 1024-byte scratch PASS\n");
}
void boot_fatal(uint64_t esr,uint64_t elr,uint64_t far){
    out("\n[SRAM] FATAL\n");val("ESR=",esr);val("ELR=",elr);val("FAR=",far);
    for(;;)__asm__ volatile("wfe");
}
static void cmd(const char *s){
    if(eq(s,"help"))out("help info test timer timerinit load run\n");
    else if(eq(s,"info"))info();
    else if(eq(s,"test"))test();
    else if(eq(s,"timer"))timer_info();
    else if(eq(s,"timerinit"))timer_init();
    else if(eq(s,"load"))load();
    else if(eq(s,"run"))run();
    else if(*s)out("Unknown command\n");
}
void boot_main(void){
    char line[64];unsigned n=0;
    out("\n[SRAM] OpenBoot SRAM v0.2.1\n");
    out(bss_test?"[SRAM] BSS FAIL\n":"[SRAM] BSS PASS\n");
    info();test();out("[SRAM] READY\ncc@SRAM:~$ ");
    for(;;){
        if(!(ur(0x14)&1))continue;
        unsigned c=ur(0)&255;
        if(c=='\r'||c=='\n'){
            if(!n)continue;
            out("\n");line[n]=0;cmd(line);n=0;out("cc@SRAM:~$ ");
        }else if(c==8||c==127){if(n){--n;out("\b \b");}}
        else if(c>=32&&c<127&&n<sizeof(line)-1){line[n++]=(char)c;tx((char)c);}
    }
}
