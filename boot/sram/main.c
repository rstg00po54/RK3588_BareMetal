#include <stdint.h>
#define UART 0xfeb50000UL
#define SLOT_A 0xff001000UL
#define SLOT_B 0xff011000UL
#define SLOT_SIZE 0x10000U
#define CHUNK 128U
extern int probe32(uintptr_t addr,uint32_t *value);
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
static void uw(unsigned o,uint32_t v){
    *(volatile uint32_t *)(UART+o)=v;
    __asm__ volatile("dsb sy" ::: "memory");
}
static void uart_diag(void){
    val("[UART] LCR = ",ur(0x0c));
    val("[UART] MCR = ",ur(0x10));
    val("[UART] LSR = ",ur(0x14));
    val("[UART] USR = ",ur(0x7c));
    val("[UART] RFL = ",ur(0x84));
}
static int rx_gpio_available;
static unsigned gpio_high,gpio_low,gpio_changes,gpio_last;
static unsigned rx_bytes,rx_errors,rx_windows,rx_monitor;
static void rx_pad_snapshot(void){
    val("[RX] PMU2 mux = ",*(volatile uint32_t *)0xfd5f4000UL);
    val("[RX] BUS mux = ",*(volatile uint32_t *)0xfd5f800cUL);
    val("[RX] GPIO0B pull = ",*(volatile uint32_t *)0xfd5f4028UL);
    val("[RX] GPIO0B pdis = ",*(volatile uint32_t *)0xfd5f404cUL);
    uint32_t v;
    out("[RX] probe GPIO0 EXT_PORT (read only)\n");
    if(probe32(0xfd8a0070UL,&v)){
        rx_gpio_available=1;
        gpio_last=(v>>14)&1;
        val("[RX] GPIO0 EXT_PORT = ",v);
    }else out("[RX] GPIO read abort; UART monitoring only\n");
}
static void rx_sample(void){
    if(rx_gpio_available){
        unsigned b=(*(volatile uint32_t *)0xfd8a0070UL>>14)&1;
        if(b)++gpio_high;else ++gpio_low;
        if(b!=gpio_last)++gpio_changes;
        gpio_last=b;
    }
}
static void rx_report(void){
    out("\n[RX] monitor; hold a key in terminal\n");
    val("uart bytes = ",rx_bytes);
    val("uart errors = ",rx_errors);
    if(rx_gpio_available){
        val("GPIO0_B6 high samples = ",gpio_high);
        val("GPIO0_B6 low samples = ",gpio_low);
        val("GPIO0_B6 changes = ",gpio_changes);
    }
    gpio_high=gpio_low=gpio_changes=rx_bytes=rx_errors=0;
    uart_diag();
}
static void uart_rx_init(void){
    /* SDK ioc_rk3588.h: PMU2 GPIO0B IE at +0x34.
     * GPIO0_B6 is UART2_RX_M0. Use the per-pin bit-6 hiword mask. */
    volatile uint32_t *ie=(volatile uint32_t *)0xfd5f4034UL;
    val("[UART] GPIO0B IE before = ",*ie);
    *ie=0x00400040U;
    __asm__ volatile("dsb sy" ::: "memory");
    val("[UART] GPIO0B IE after = ",*ie);
    /* ns16550 U-Boot init: DTR/RTS, no auto flow control. */
    for(unsigned i=0;i<1000000;++i)if(ur(0x14)&0x40)break;
    uw(0x0c,3);
    uw(4,0);
    uw(0x10,3);
    uw(8,7);
    uart_diag();
    /* Test local UART TX->RX with MCR loopback. Restore before logging. */
    for(unsigned i=0;i<1000000;++i)if(ur(0x14)&0x40)break;
    uw(0x10,0x13);
    uw(8,7);
    int received=-1;
    if(ur(0x14)&0x20){
        uw(0,0x55);
        for(unsigned i=0;i<1000000;++i){
            if(ur(0x14)&1){received=ur(0)&255;break;}
        }
    }
    for(unsigned i=0;i<1000000;++i)if(ur(0x14)&0x40)break;
    uw(0x10,3);
    uw(8,7);
    out(received==0x55?"[UART] INTERNAL LOOPBACK PASS\n":"[UART] INTERNAL LOOPBACK FAIL\n");
    if(received>=0)val("[UART] loopback byte = ",(unsigned)received);
    out("[UART] RX ready: type help\n");
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
#include "usb_sram.h"
#include "ddr_sram.h"

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
    if(usb_owned){out("ERR USB ACTIVE; usb stop first (usbstop)\n");return;}
    upload_abort4();upload_state=0;upload_size=upload_done=0;upload_crc=0;
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
    if(!loaded || loaded!=alternate() || (SYS(SCTLR_EL3)&5)){
        out("ERR NO VERIFIED IMAGE OR UNSAFE SLOT\n");return;
    }
    if(!usb_stop_for_jump4())return;
    if(upload_state==2){
        uint32_t crc=0xffffffffU;
        volatile const unsigned char *src=(volatile const unsigned char *)loaded;
        for(unsigned i=0;i<upload_size;++i)crc=crc_byte(crc,src[i]);
        if((crc^0xffffffffU)!=upload_crc){loaded=0;upload_state=3;out("ERR FINAL CRC\n");return;}
    }
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
    if(eq(s,"help"))out("help info test uart rxmon timer timerinit usb usbstart usbstat usbstop usbload update usbremote usbtrace loadstat load run\nddrpre ddrhandshake ddr ddr0..3 ddrclk0..3 ddrphy0..3\n");
    else if(eq(s,"ddrpre"))ddr_preflight();
    else if(eq(s,"ddrhandshake"))ddr_handshake();
    else if(eq(s,"ddr"))ddr_info();
    else if(eq(s,"ddr0"))ddr_channel(0);
    else if(eq(s,"ddr1"))ddr_channel(1);
    else if(eq(s,"ddr2"))ddr_channel(2);
    else if(eq(s,"ddr3"))ddr_channel(3);
    else if(eq(s,"ddrclk0"))ddr_clock(0);
    else if(eq(s,"ddrclk1"))ddr_clock(1);
    else if(eq(s,"ddrclk2"))ddr_clock(2);
    else if(eq(s,"ddrclk3"))ddr_clock(3);
    else if(eq(s,"ddrphy0"))ddr_phy(0);
    else if(eq(s,"ddrphy1"))ddr_phy(1);
    else if(eq(s,"ddrphy2"))ddr_phy(2);
    else if(eq(s,"ddrphy3"))ddr_phy(3);
    else if(eq(s,"info"))info();
    else if(eq(s,"test"))test();
    else if(eq(s,"uart"))uart_diag();
    else if(eq(s,"timer"))timer_info();
    else if(eq(s,"timerinit")){
        if(usb_owned)out("ERR USB ACTIVE; usbstop first\n");
        else timer_init();
    }
    else if(eq(s,"rxmon")){
        rx_monitor=!rx_monitor;
        rx_windows=0;
        out(rx_monitor?"[RX] monitor ON\n":"[RX] monitor OFF\n");
    }
    else if(eq(s,"usb"))usb_snapshot4();
    else if(eq(s,"usbstart"))usbstart4();
    else if(eq(s,"usbstat"))usbstat4();
    else if(eq(s,"usbstop"))(void)usb_stop_for_jump4();
    else if(eq(s,"usbload")||eq(s,"update"))(void)upload_arm4();
    else if(eq(s,"usbremote")){
        remote_enabled=!remote_enabled;
        if(!remote_enabled)upload_abort4();
        out(remote_enabled?"[USB] remote update ON\n":"[USB] remote update OFF\n");
    }
    else if(eq(s,"usbtrace")){
        usb_trace=!usb_trace;
        out(usb_trace?"[USB] setup trace ON\n":"[USB] setup trace OFF\n");
    }
    else if(eq(s,"loadstat"))upload_status4();
    else if(eq(s,"load"))load();
    else if(eq(s,"run"))run();
    else if(*s)out("Unknown command\n");
}
void boot_main(void){
    char line[64],history[4][64];
    unsigned n=0,last_cr=0,esc=0,hcount=0,hnext=0,hpos=0;
    out("\n[SRAM] OpenBoot SRAM v0.5.2\n");
    out(bss_test?"[SRAM] BSS FAIL\n":"[SRAM] BSS PASS\n");
    info();test();uart_rx_init();out("[SRAM] READY\ncc@SRAM:~$ ");
    rx_pad_snapshot();
    out("[RX] sampling retained; reports OFF (rxmon toggles)\n");
    /* Only auto-start on a verified running counter (warm A/B jump).
     * Cold timer MMIO was previously intermittent; keep UART recovery. */
    if(SYS(CNTFRQ_EL0)==24000000 && counter_moves()){
        timer_ok=1;
        out("[SRAM] warm counter; auto USB start\n");
        usbstart4();
    }else out("[SRAM] cold start: timerinit then usbstart\n");
    out("cc@SRAM:~$ ");
    unsigned idle=0;
    for(;;){
        rx_sample();
        if(live4)poll4();
        if(remote_run){remote_run=0;out("\n[UPLOAD] remote RUN\n");run();out("cc@SRAM:~$ ");}
        if(++idle==500000U){
            idle=0;
            if(rx_windows<12 || rx_bytes || rx_errors || gpio_changes){
                ++rx_windows;
                if(rx_monitor){
                    rx_report();
                    out("cc@SRAM:~$ ");
                }else{
                    /* Preserve sampled accesses from the verified baseline,
                     * but do not print unsolicited reports. */
                    gpio_high=gpio_low=gpio_changes=rx_bytes=rx_errors=0;
                    (void)ur(0x0c);(void)ur(0x10);(void)ur(0x14);
                    (void)ur(0x7c);(void)ur(0x84);
                }
            }
        }
        unsigned lsr=ur(0x14);
        if(lsr&0x1e)++rx_errors;
        if(!(lsr&1)){
            if(lsr&0x1e)val("\n[UART] RX error LSR = ",lsr);
            continue;
        }
        ++rx_bytes;
        unsigned c=ur(0)&255;
        if(lsr&0x1e)val("\n[UART] RX error LSR = ",lsr);
        if(c==3){
            n=0;esc=0;last_cr=0;hpos=0;out("^C\ncc@SRAM:~$ ");continue;
        }
        if(c==27){esc=1;continue;}
        if(esc==1){esc=(c=='['||c=='O')?2:0;continue;}
        if(esc==2){
            esc=0;
            if(c=='A'||c=='B'){
                if(c=='A' && hpos<hcount)++hpos;
                else if(c=='B' && hpos)--hpos;
                while(n){out("\b \b");--n;}
                if(hpos){
                    const char *p=history[(hnext+4-hpos)%4];
                    while(*p && n<sizeof(line)-1){line[n++]=*p;tx(*p++);}
                }
            }
            continue;
        }
        if(c<32 && c!='\r' && c!='\n' && c!=8)continue;
        /* CR and LF are both valid Enter keys. Suppress only the LF
         * immediately following CR so a CRLF terminal gets one prompt. */
        if(c=='\n' && last_cr){last_cr=0;continue;}
        last_cr=(c=='\r');
        if(c=='\r'||c=='\n'){
            out("\n");
            if(n){
                line[n]=0;
                for(unsigned i=0;i<=n;++i)history[hnext][i]=line[i];
                hnext=(hnext+1)%4;if(hcount<4)++hcount;
                cmd(line);n=0;
            }
            hpos=0;
            out("cc@SRAM:~$ ");
        }else if(c==8||c==127){if(n){--n;out("\b \b");}}
        else if(c>=32&&c<127&&n<sizeof(line)-1){line[n++]=(char)c;tx((char)c);}
    }
}
