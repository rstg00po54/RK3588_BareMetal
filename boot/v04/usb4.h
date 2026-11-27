/* Experimental HS/FS EP0-only device. Test VID/PID 1209:0001 is NOT unique
 * and must not be used on distributed products. No storage or bulk protocol.
 * Inherit OTG0 board clocks/PHY from BootROM, reset device core on command.
 */
#define U_DCTL 0xc704
#define U_DSTS 0xc70c
#define U_DCFG 0xc700
#define U_CNT  0xc40c
#define U_RUN  (1U<<31)
#define U_RESET (1U<<30)
#define U_HALT (1U<<22)
static volatile uint32_t ring4[64] __attribute__((aligned(64)));
static volatile uint32_t trb4[4] __attribute__((aligned(16)));
static volatile unsigned char setup4[64] __attribute__((aligned(64)));
static volatile unsigned char data4[64] __attribute__((aligned(64)));
static unsigned cursor4, phase4, config4, live4, status_ep4, three4;
static unsigned setups4, resets4, connects4;
static uint32_t rd4(unsigned o) { return *(volatile uint32_t *)(USB+o); }
static void wr4(unsigned o,uint32_t v) { *(volatile uint32_t *)(USB+o)=v; __asm__ volatile("dsb sy" ::: "memory"); }
static uint64_t ticks4(void) { return SYS(CNTPCT_EL0); }
static void delay4(unsigned ms) { uint64_t t=ticks4(); while(ticks4()-t < (SYS(CNTFRQ_EL0)/1000)*ms) {} }
static int wait4(unsigned o,uint32_t mask,uint32_t expected,unsigned ms) {
    uint64_t t=ticks4(), lim=(SYS(CNTFRQ_EL0)/1000)*ms;
    do { if ((rd4(o)&mask)==expected) return 1; } while(ticks4()-t<lim);
    val("[USB4] timeout offset",o,4); val("value",rd4(o),8); return 0;
}
static int cmd4(unsigned ep,unsigned cmd,uint32_t p0,uint32_t p1,uint32_t p2) {
    unsigned b=0xc800+ep*16;
    if (!wait4(b+12,1<<10,0,50)) return 0;
    wr4(b,p2); wr4(b+4,p1); wr4(b+8,p0); wr4(b+12,cmd|(1<<10));
    if (!wait4(b+12,1<<10,0,50)) return 0;
    if (rd4(b+12)&0xf000) { val("[USB4] endpoint command error",rd4(b+12),8); return 0; }
    return 1;
}
static int xfer4(unsigned ep,volatile unsigned char *buf,unsigned len,unsigned type) {
    uint64_t a=(uintptr_t)buf, t=(uintptr_t)trb4;
    trb4[0]=(uint32_t)a; trb4[1]=(uint32_t)(a>>32);
    trb4[2]=len; trb4[3]=(type<<4)|(1<<11)|(1<<10)|3;
    __asm__ volatile("dsb sy" ::: "memory");
    return cmd4(ep,6,(uint32_t)(t>>32),(uint32_t)t,0);
}
static int arm4(void) { phase4=1; return xfer4(0,setup4,8,2); }
static int status4(unsigned ep) { phase4=4; return xfer4(ep,setup4,0,three4?4:3); }
static int reject4(void) { return cmd4(0,4,0,0,0)&&arm4(); }
static const unsigned char dev4[]={18,1,0,2,0,0,0,64,0x09,0x12,1,0,0,1,0,0,0,1};
static const unsigned char cfg4[]={9,2,18,0,1,1,0,0x80,50,9,4,0,0,0,0xff,0,0,0};
static const unsigned char qual4[]={10,6,0,2,0,0,0,64,1,0};
static int reply4(const unsigned char *p,unsigned n,unsigned len,unsigned other) {
    if(n>len)n=len;
    for(unsigned i=0;i<n;++i)data4[i]=p[i];
    if(other && n>1)data4[1]=7;
    phase4=2; status_ep4=0; three4=1;
    return xfer4(1,data4,n,5);
}
static int handle4(void) {
    unsigned bm=setup4[0],req=setup4[1],v=setup4[2]|(setup4[3]<<8),
             idx=setup4[4]|(setup4[5]<<8),len=setup4[6]|(setup4[7]<<8);
    ++setups4;
    /* Print concise request trace; UART is 1.5 Mbaud. */
    puts0("[SETUP] "); hex(bm,2); put(' '); hex(req,2); put(' '); hex(v,4); put(' '); hex(idx,4); put(' '); hex(len,4); put('\n');
    if(bm==0x80 && req==6 && (v&255)==0 && idx==0 && len) {
        switch(v>>8) {
        case 1:return reply4(dev4,sizeof(dev4),len,0);
        case 2:return reply4(cfg4,sizeof(cfg4),len,0);
        case 6:return reply4(qual4,sizeof(qual4),len,0);
        case 7:return reply4(cfg4,sizeof(cfg4),len,1);
        }
    }
    if (bm==0 && req==5 && v<=127 && !idx && !len && !config4) {
        wr4(U_DCFG,(rd4(U_DCFG)&~(127U<<3))|(v<<3));
        phase4=3; status_ep4=1; three4=0; return 1;
    }
    if (bm==0 && req==9 && v<=1 && !idx && !len) {
        config4=v; phase4=3; status_ep4=1; three4=0; return 1;
    }
    if (bm==0x80 && req==8 && !v && !idx && len==1) {
        unsigned char b=(unsigned char)config4; return reply4(&b,1,len,0);
    }
    if ((bm==0x80 || bm==0x81 || bm==0x82) && req==0 && !v && len==2 &&
        (idx==0 || (bm==0x82 && idx==0x80))) {
        unsigned char b[2]={0,0}; return reply4(b,2,len,0);
    }
    if (bm==0x81 && req==10 && !v && !idx && len==1 && config4) {
        unsigned char b=0; return reply4(&b,1,len,0);
    }
    if (bm==1 && req==11 && !v && !idx && !len && config4) {
        phase4=3; status_ep4=1; three4=0; return 1;
    }
    puts0("[USB4] unsupported request; STALL\n"); return reject4();
}
static void poll4(void) {
    if(!live4)return;
    unsigned count=rd4(U_CNT)&0xfffc;
    if(count>sizeof(ring4)) { puts0("[USB4] event count overflow; run usbstart again\n"); live4=0; return; }
    for(unsigned i=0;i<count/4;++i) {
        __asm__ volatile("dsb sy" ::: "memory");
        uint32_t e=ring4[cursor4]; cursor4=(cursor4+1)&63;
        wr4(U_CNT,4);
        int ok=1;
        if(e&1) {
            if((e&0xfe)!=0) { puts0("[USB4] unsupported non-device event\n"); live4=0; return; }
            unsigned type=(e>>8)&15;
            if(type==1) {
                ++resets4; config4=0;
                wr4(U_DCFG,rd4(U_DCFG)&~(127U<<3));
                puts0("[USB4] bus reset\n");
                if(phase4!=1) { puts0("[USB4] interrupted EP0 phase; explicit usbstart required\n"); ok=0; }
            } else if(type==2) { ++connects4; val("[USB4] connect speed",rd4(U_DSTS)&7,1); }
            else if(type==0) { config4=0; puts0("[USB4] disconnect\n"); if(phase4!=1)ok=0; }
            else if(type==9||type==11)ok=0;
        } else {
            unsigned ep=(e>>1)&31,type=(e>>6)&15,s=(e>>12)&15;
            if(ep>1) continue;
            if(type==1) {
                if((s&1) || (trb4[3]&1))ok=0;
                else if(phase4==1 && ep==0) {
                    if((trb4[2]&0xffffff)!=0)ok=0; else ok=handle4();
                } else if(phase4==2 && ep==1) { phase4=3; }
                else if(phase4==4 && ep==status_ep4) { if(config4)puts0("[USB4] configured\n"); ok=arm4(); }
                else ok=0;
            } else if(type==3 && (s&3)==2 && phase4==3 && ep==status_ep4)ok=status4(ep);
        }
        if(!ok) { val("[USB4] transfer/state failure event",e,8); live4=0; puts0("Run usbstart again to reset USB\n"); return; }
    }
}
static void usbstart4(void) {
    live4=0;
    uint32_t id;
    if(!probe32(USB+0xc120,&id) || id!=0x5533300a) { puts0("[USB4] unexpected core ID\n");return; }
    if(SYS(SCTLR_EL3)&5) { puts0("[USB4] requires MMU and D-cache off\n");return; }
    puts0("[USB4] stop old device; no PHY reset\n");
    wr4(U_DCTL,rd4(U_DCTL)&~(U_RUN|U_RESET|(1U<<19)));
    /* Discard only the old event notifications while explicitly disconnecting.
     * No reads/writes of the old DMA ring itself. */
    uint64_t t=ticks4();
    while(!(rd4(U_DSTS)&U_HALT)) {
        unsigned n=rd4(U_CNT)&0xfffc;
        if(n && n<=256)wr4(U_CNT,n);
        if(ticks4()-t>(SYS(CNTFRQ_EL0)/1000)*500) {
            puts0("[USB4] stop timeout; power cycle to MaskROM\n");return;
        }
    }
    delay4(300);
    puts0("[USB4] device soft reset\n");
    wr4(U_DCTL,U_RESET);
    if(!wait4(U_DCTL,U_RESET,0,500))return;
    delay4(50);
    wr4(0xc110,(rd4(0xc110)&~((3U<<12)|(1U<<11)))|(2U<<12));
    /* Keep board PHY routing; don't gate USB2 PHY during endpoint commands. */
    wr4(0xc200,rd4(0xc200)&~((1U<<6)|(1U<<31)));
    wr4(U_DCFG,rd4(U_DCFG)&~((127U<<3)|7U|(1U<<22)));
    wr4(0xc720,0);
    unsigned n=rd4(U_CNT)&0xfffc;
    if(n)wr4(U_CNT,n);
    for(unsigned i=0;i<64;++i)ring4[i]=0;
    cursor4=0; config4=0; setups4=resets4=connects4=0;
    uint64_t a=(uintptr_t)ring4;
    wr4(0xc400,(uint32_t)a); wr4(0xc404,(uint32_t)(a>>32));
    wr4(0xc408,0x80000100);
    wr4(0xc708,7);
    if(!cmd4(0,9,0,0,0))return;
    for(unsigned ep=0;ep<2;++ep) {
        if(!cmd4(ep,1,64<<3,(ep<<25)|(1<<8)|(1<<10),0) ||
           !cmd4(ep,2,1,0,0))return;
    }
    wr4(0xc720,3);
    if(!arm4())return;
    live4=1;
    wr4(U_DCTL,rd4(U_DCTL)|U_RUN);
    puts0("[USB4] RUN; host should enumerate 1209:0001 (private test PID)\n");
}
static void usbstat4(void) {
    val("live",live4,1); val("phase",phase4,1); val("configuration",config4,1);
    val("setups",setups4,8); val("resets",resets4,8); val("connects",connects4,8);
    val("event cursor",cursor4,2);
}
static void oldtrb4(void) {
    uint32_t v;
    puts0("[TRB] raw historical SRAM words; not live pointer proof\n");
    for(unsigned i=0;i<4;++i) {
        uintptr_t a=0xff0004c0+4*i;
        if(!probe32(a,&v)) { puts0("ABORT\n");return; }
        val("word",v,8);
    }
}
