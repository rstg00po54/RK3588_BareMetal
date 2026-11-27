/* SRAM phase 1: experimental HS/FS EP0-only device. Test VID/PID 1209:0001 is NOT unique
 * and must not be used on distributed products. EP0 staged SRAM image upload; no storage or bulk protocol.
 * Inherit OTG0 board clocks/PHY from BootROM, reset device core on command.
 */
#define USB 0xfc000000UL
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
/* Once we touch the inherited device, require a confirmed halt before a jump. */
static unsigned usb_owned;
/* Development-only USB control on a trusted physical connection.
 * Only the inactive 64 KiB SRAM slot is writable. Explicit RUN required. */
static unsigned upload_arm,upload_state,upload_size,upload_done;
static uint32_t upload_crc;
static unsigned upload_pending,upload_offset,upload_len;
static unsigned config_reported,usb_trace,remote_enabled=1,remote_run;
#define SRAM_USB_API 2U
#include "app_abi.h"
static unsigned app_state,app_result,app_pending;
static int app_header_ok(void){
    if(loaded!=alternate() || !loaded || upload_state!=2 ||
       upload_size!=upload_done || upload_size<20 || upload_size>SRAM_APP_MAX)return 0;
    volatile const uint32_t *h=(volatile const uint32_t *)loaded;
    return h[0]==SRAM_APP_MAGIC && h[1]==SRAM_APP_ABI &&
           h[2]==SRAM_APP_ENTRY && h[3]==0;
}
static uint32_t crc_byte(uint32_t crc,unsigned c);
static void upload_abort4(void){
    if(upload_arm || upload_state==1 || upload_pending==0x54){
        loaded=0;
        upload_state=0;
    }
    upload_arm=0;upload_pending=0;remote_run=0;app_pending=0;
    if(app_state==1)app_state=3;
}
static int upload_arm4(void){
    if(!live4 || !config4 || !alternate() || (SYS(SCTLR_EL3)&5)){
        out("[UPLOAD] USB not configured or unsafe slot\n");return 0;
    }
    loaded=0;upload_state=0;upload_size=upload_done=0;upload_crc=0;
    upload_pending=0;upload_arm=1;app_state=app_result=app_pending=0;
    out("[UPLOAD] armed; host may stage one image\n");return 1;
}
static void upload_status4(void){
    val("[UPLOAD] armed = ",upload_arm);
    val("[UPLOAD] state = ",upload_state);
    val("[UPLOAD] size = ",upload_size);
    val("[UPLOAD] received = ",upload_done);
    val("[UPLOAD] verified entry = ",loaded);
}
static int upload_apply4(void){
    /* called only after the OUT data stage completes */
    unsigned op=upload_pending;
    upload_pending=0;
    if(!upload_arm || !alternate() || (SYS(SCTLR_EL3)&5))return 0;
    if(op==0x52){
        uint32_t size=0,crc=0;
        for(unsigned i=0;i<4;++i){
            size|=(uint32_t)data4[i]<<(8*i);
            crc|=(uint32_t)data4[i+4]<<(8*i);
        }
        if(upload_state!=0 || size<4 || size>SLOT_SIZE || (size&3))return 0;
        loaded=0;upload_size=size;upload_crc=crc;upload_done=0;upload_state=1;
        return 1;
    }
    if(op==0x53){
        if(upload_state!=1 || upload_offset!=upload_done ||
           upload_len>upload_size-upload_done)return 0;
        volatile unsigned char *dst=(volatile unsigned char *)alternate();
        for(unsigned i=0;i<upload_len;++i)dst[upload_done+i]=data4[i];
        upload_done+=upload_len;
        return 1;
    }
    return 0;
}
static int upload_verify4(void){
    if(!upload_arm || upload_state!=1 || upload_done!=upload_size ||
       !alternate() || (SYS(SCTLR_EL3)&5))return 0;
    volatile const unsigned char *src=(volatile const unsigned char *)alternate();
    uint32_t crc=0xffffffffU;
    __asm__ volatile("dsb sy" ::: "memory");
    for(unsigned i=0;i<upload_size;++i)crc=crc_byte(crc,src[i]);
    crc^=0xffffffffU;
    upload_arm=0;
    if(crc!=upload_crc){
        upload_state=3;loaded=0;
        out("[UPLOAD] SRAM CRC32 FAIL\n");return 0;
    }
    upload_state=2; /* loaded becomes valid only after status stage */
    return 1;
}
static void usb_put(char c){if(c=='\n')tx('\r');tx(c);}
static void usb_hex(uint64_t v,unsigned digits){
    const char *d="0123456789ABCDEF";
    out("0x");
    while(digits){--digits;tx(d[(v>>(4*digits))&15]);}
}
static void usb_val(const char *name,uint64_t v,unsigned digits){
    out(name);out(" = ");usb_hex(v,digits);out("\n");
}
static uint32_t rd4(unsigned o) {
    return *(volatile uint32_t *)(USB + o);
}

static void wr4(unsigned o, uint32_t v) {
    *(volatile uint32_t *)(USB + o) = v;
    __asm__ volatile("dsb sy"::: "memory");
}

static uint64_t ticks4(void) {
    return SYS(CNTPCT_EL0);
}

/* The counter was verified on target only after explicit timerinit.
 * Also bound loops by iterations, in case it stops advancing. */
static int delay4(unsigned ms) {
    uint64_t t=ticks4(), limit=(SYS(CNTFRQ_EL0)/1000)*ms;
    for(unsigned spin=0;spin<20000000U;++spin)
        if(ticks4()-t>=limit)return 1;
    out("[USB4] timer delay stalled\n");
    return 0;
}
static int wait4(unsigned o,uint32_t mask,uint32_t expected,unsigned ms){
    uint64_t t=ticks4(), limit=(SYS(CNTFRQ_EL0)/1000)*ms;
    for(unsigned spin=0;spin<20000000U;++spin){
        if((rd4(o)&mask)==expected)return 1;
        if(ticks4()-t>=limit)break;
    }
    usb_val("[USB4] timeout offset",o,4);
    usb_val("value",rd4(o),8);
    return 0;
}

static int cmd4(unsigned ep, unsigned cmd, uint32_t p0, uint32_t p1, uint32_t p2) {
    unsigned b = 0xc800 + ep * 16;
    if (!wait4(b + 12, 1 << 10, 0, 50))
        return 0;
    wr4(b, p2);
    wr4(b + 4, p1);
    wr4(b + 8, p0);
    wr4(b + 12, cmd | (1 << 10));
    if (!wait4(b + 12, 1 << 10, 0, 50))
        return 0;
    if (rd4(b + 12) & 0xf000) {
        usb_val("[USB4] endpoint command error", rd4(b + 12), 8);
        return 0;
    }
    return 1;
}

static int xfer4(unsigned ep, volatile unsigned char *buf, unsigned len, unsigned type) {
    uint64_t a = (uintptr_t)buf, t = (uintptr_t)trb4;
    trb4[0] = (uint32_t)a;
    trb4[1] = (uint32_t)(a >> 32);
    trb4[2] = len;
    trb4[3] = (type << 4) | (1 << 11) | (1 << 10) | 3;
    __asm__ volatile("dsb sy"::: "memory");
    return cmd4(ep, 6, (uint32_t)(t >> 32), (uint32_t)t, 0);
}

static int arm4(void) {
    phase4 = 1;
    return xfer4(0, setup4, 8, 2);
}

static int status4(unsigned ep) {
    phase4 = 4;
    return xfer4(ep, setup4, 0, three4 ? 4 : 3);
}

static int reject4(void) {
    return cmd4(0, 4, 0, 0, 0) && arm4();
}

static const unsigned char dev4[] = {18, 1, 0, 2, 0, 0, 0, 64, 0x09, 0x12, 1, 0, 0, 1, 0, 0, 0, 1 };
static const unsigned char cfg4[] = {9, 2, 18, 0, 1, 1, 0, 0x80, 50, 9, 4, 0, 0, 0, 0xff, 0, 0, 0 };
static const unsigned char qual4[] = {10, 6, 0, 2, 0, 0, 0, 64, 1, 0 };
static int reply4(const unsigned char *p, unsigned n, unsigned len, unsigned other) {
    if (n > len)
        n = len;
    for (unsigned i = 0; i < n; ++i)
        data4[i] = p[i];
    if (other && n > 1)
        data4[1] = 7;
    phase4 = 2;
    status_ep4 = 0;
    three4 = 1;
    return xfer4(1, data4, n, 5);
}

static int handle4(void) {
    unsigned bm = setup4[0], req = setup4[1], v = setup4[2] | (setup4[3] << 8),
             idx = setup4[4] | (setup4[5] << 8), len = setup4[6] | (setup4[7] << 8);
    ++setups4;
    /* API/version, active slot, alternate slot and remote-enable flag. */
    if(bm==0xc0 && req==0x58 && !v && !idx && len && config4){
        unsigned char info[16];
        uint32_t words[4]={SRAM_USB_API,(uint32_t)(uintptr_t)_start,(uint32_t)alternate(),remote_enabled};
        for(unsigned j=0;j<4;++j)for(unsigned i=0;i<4;++i)
            info[j*4+i]=(unsigned char)(words[j]>>(8*i));
        return reply4(info,sizeof(info),len,0);
    }
    if(bm==0xc0 && req==0x5a && !v && !idx && len && config4){
        unsigned char b[16];
        uint32_t words[4]={SRAM_APP_ABI,app_state,app_result,(uint32_t)(uintptr_t)_start};
        for(unsigned j=0;j<4;++j)for(unsigned i=0;i<4;++i)
            b[4*j+i]=(unsigned char)(words[j]>>(8*i));
        return reply4(b,sizeof b,len,0);
    }
    if(bm==0x40 && config4 && remote_enabled && !v && !idx && !len){
        if(req==0x59 && app_state==0 && app_header_ok() && !(SYS(SCTLR_EL3)&5)){
            upload_pending=0x59;phase4=3;status_ep4=1;three4=0;return 1;
        }
        if(req==0x56 && upload_arm4()){
            phase4=3;status_ep4=1;three4=0;return 1;
        }
        if(req==0x57 && loaded && loaded==alternate() && upload_state==2 &&
           upload_size==upload_done && !(SYS(SCTLR_EL3)&5) &&
           *(volatile const uint32_t *)loaded!=SRAM_APP_MAGIC){
            upload_pending=0x57;phase4=3;status_ep4=1;three4=0;return 1;
        }
    }
    /* SRAM upload requests: 0x52 header, 0x53 sequential <=64B,
     * 0x54 verify, 0x55 read-only status. USB wire data uses little endian.
     * No host-chosen pointer, no writes to running slot, no remote run. */
    if(bm==0xc0 && req==0x55 && !v && !idx && len && config4){
        unsigned char status[16];
        uint32_t words[4]={upload_state,upload_size,upload_done,upload_crc};
        for(unsigned j=0;j<4;++j)
            for(unsigned i=0;i<4;++i)status[j*4+i]=(unsigned char)(words[j]>>(8*i));
        return reply4(status,sizeof(status),len,0);
    }
    if(bm==0x40 && config4 && upload_arm && !(SYS(SCTLR_EL3)&5)){
        if((req==0x52 && !v && !idx && len==8 && upload_state==0) ||
           (req==0x53 && !idx && len && len<=64 && upload_state==1 &&
            v==upload_done && len<=upload_size-upload_done)){
            upload_pending=req;upload_offset=v;upload_len=len;
            phase4=5;status_ep4=1;three4=1;
            /* DWC3 OUT data TRB must have maxpacket-rounded length. */
            return xfer4(0,data4,64,5);
        }
        if(req==0x54 && !v && !idx && !len && upload_verify4()){
            upload_pending=0x54;phase4=3;status_ep4=1;three4=0;
            return 1;
        }
    }
    /* Read-only vendor ping; no arbitrary address, DMA or image writes. */
    if(bm==0xc0 && req==0x51 && !v && !idx && len && config4){
        static const unsigned char pong[]={ 'S','R','A','M','U','S','B','1' };
        return reply4(pong,sizeof(pong),len,0);
    }
    /* Optional trace; keep normal enumeration quiet. */
    if(usb_trace){
    out("[SETUP] ");
    usb_hex(bm, 2);
    usb_put(' ');
    usb_hex(req, 2);
    usb_put(' ');
    usb_hex(v, 4);
    usb_put(' ');
    usb_hex(idx, 4);
    usb_put(' ');
    usb_hex(len, 4);
    usb_put('\n');
    }
    if (bm == 0x80 && req == 6 && (v & 255) == 0 && idx == 0 && len) {
        switch (v >> 8) {
        case 1: return reply4(dev4, sizeof(dev4), len, 0);
        case 2: return reply4(cfg4, sizeof(cfg4), len, 0);
        case 6: return reply4(qual4, sizeof(qual4), len, 0);
        case 7: return reply4(cfg4, sizeof(cfg4), len, 1);
        }
    }
    if (bm == 0 && req == 5 && v <= 127 && !idx && !len && !config4) {
        wr4(U_DCFG, (rd4(U_DCFG) & ~(127U << 3)) | (v << 3));
        phase4 = 3;
        status_ep4 = 1;
        three4 = 0;
        return 1;
    }
    if (bm == 0 && req == 9 && v <= 1 && !idx && !len) {
        if(!v)upload_abort4();
        config4 = v;config_reported=0;
        phase4 = 3;
        status_ep4 = 1;
        three4 = 0;
        return 1;
    }
    if (bm == 0x80 && req == 8 && !v && !idx && len == 1) {
        unsigned char b = (unsigned char)config4;
        return reply4(&b, 1, len, 0);
    }
    if ((bm == 0x80 || bm == 0x81 || bm == 0x82) && req == 0 && !v && len == 2 &&
        (idx == 0 || (bm == 0x82 && idx == 0x80))) {
        unsigned char b[2] = {0, 0 };
        return reply4(b, 2, len, 0);
    }
    if (bm == 0x81 && req == 10 && !v && !idx && len == 1 && config4) {
        unsigned char b = 0;
        return reply4(&b, 1, len, 0);
    }
    if (bm == 1 && req == 11 && !v && !idx && !len && config4) {
        phase4 = 3;
        status_ep4 = 1;
        three4 = 0;
        return 1;
    }
    out("[USB4] unsupported request; STALL\n");
    return reject4();
}

static void poll4(void) {
    if (!live4)
        return;
    unsigned count = rd4(U_CNT) & 0xfffc;
    if (count > sizeof(ring4)) {
        out("[USB4] event count overflow; run usbstart again\n");
        upload_abort4();live4 = 0;
        return;
    }
    for (unsigned i = 0; i < count / 4; ++i) {
        __asm__ volatile("dsb sy"::: "memory");
        uint32_t e = ring4[cursor4];
        cursor4 = (cursor4 + 1) & 63;
        wr4(U_CNT, 4);
        int ok = 1;
        if (e & 1) {
            if ((e & 0xfe) != 0) {
                out("[USB4] unsupported non-device event\n");
                upload_abort4();live4 = 0;
                return;
            }
            unsigned type = (e >> 8) & 15;
            if (type == 1) {
                ++resets4;
                upload_abort4();config_reported=0;
                config4 = 0;
                wr4(U_DCFG, rd4(U_DCFG) & ~(127U << 3));
                out("[USB4] bus reset\n");
                if (phase4 != 1) {
                    out("[USB4] interrupted EP0 phase; explicit usbstart required\n");
                    ok = 0;
                }
            } else if (type == 2) {
                ++connects4;
                usb_val("[USB4] connect speed", rd4(U_DSTS) & 7, 1);
            } else if (type == 0) {
                upload_abort4();config_reported=0;
                config4 = 0;
                out("[USB4] disconnect\n");
                if (phase4 != 1)
                    ok = 0;
            }
            else if (type == 9 || type == 11)
                ok = 0;
        } else {
            unsigned ep = (e >> 1) & 31, type = (e >> 6) & 15, s = (e >> 12) & 15;
            if (ep > 1)
                continue;
            if (type == 1) {
                if ((s & 1) || (trb4[3] & 1))
                    ok = 0;
                else if (phase4 == 1 && ep == 0) {
                    if ((trb4[2] & 0xffffff) != 0)
                        ok = 0;
                    else
                        ok = handle4();
                } else if(phase4==5 && ep==0){
                    unsigned remain=trb4[2]&0xffffff;
                    if(remain>64 || 64-remain!=upload_len || !upload_apply4()){
                        upload_abort4();ok=reject4();
                    }else phase4=3;
                } else if (phase4 == 2 && ep == 1) {
                    phase4 = 3;
                } else if (phase4 == 4 && ep == status_ep4) {
                    if(upload_pending==0x54){
                        upload_pending=0;
                        loaded=alternate();
                        out("[UPLOAD] SRAM CRC32 PASS; run manually\n");
                    }
                    if(config4 && !config_reported){
                        config_reported=1;out("[USB4] configured\n");
                    }
                    if(upload_pending==0x57){
                        upload_pending=0;remote_run=1;
                        phase4=0;return; /* status ACK completed; no new DMA */
                    }
                    if(upload_pending==0x59){
                        upload_pending=0;app_state=1;
                        if(!arm4()){app_state=3;ok=0;}
                        else{app_pending=1;return;}
                    }else ok = arm4();
                }
                else
                    ok = 0;
            } else if (type == 3 && (s & 3) == 2 && phase4 == 3 && ep == status_ep4)ok = status4(ep);
        }
        if (!ok) {
            upload_abort4();
            usb_val("[USB4] transfer/state failure event", e, 8);
            live4 = 0;
            out("Run usbstart again to reset USB\n");
            return;
        }
    }
}

static void usb_handoff_regs4(void){
    usb_val("[USB4] DCTL",rd4(U_DCTL),8);
    usb_val("[USB4] DSTS",rd4(U_DSTS),8);
    usb_val("[USB4] GEVNTCOUNT",rd4(U_CNT),8);
    usb_val("[USB4] DALEPENA",rd4(0xc720),8);
    usb_val("[USB4] EP0 OUT DEPCMD",rd4(0xc80c),8);
    usb_val("[USB4] EP0 IN DEPCMD",rd4(0xc81c),8);
}
static int usb_halt4(void){
    upload_abort4();live4=0;
    if(!usb_owned)return 1;
    wr4(U_DCTL,rd4(U_DCTL)&~(U_RUN|U_RESET|(1U<<19)));
    uint64_t t=ticks4(),limit=(SYS(CNTFRQ_EL0)/1000)*500;
    for(unsigned spin=0;spin<20000000U;++spin){
        if(rd4(U_DSTS)&U_HALT){usb_owned=0;out("[USB4] halted\n");return 1;}
        unsigned n=rd4(U_CNT)&0xfffc;
        if(n && n<=256)wr4(U_CNT,n);
        if(ticks4()-t>=limit)break;
    }
    out("[USB4] halt timeout; old transfer state may be pending\n");
    usb_handoff_regs4();
    return 0;
}
static void usbstart4(void) {
    if(!timer_ok || SYS(CNTFRQ_EL0)!=24000000 || !counter_moves()){
        out("[USB4] run timerinit first\n");return;
    }
    live4 = 0;
    uint32_t id;
    if (!probe32(USB + 0xc120, &id) || id != 0x5533300a) {
        out("[USB4] unexpected core ID\n");
        return;
    }
    if (SYS(SCTLR_EL3) & 5) {
        out("[USB4] requires MMU and D-cache off\n");
        return;
    }
    uintptr_t base=(uintptr_t)_start,end=(uintptr_t)__image_end;
    if((base!=SLOT_A && base!=SLOT_B)||end>base+SLOT_SIZE||
       (uintptr_t)ring4<base || (uintptr_t)ring4+sizeof(ring4)>end||
       (uintptr_t)trb4<base || (uintptr_t)trb4+sizeof(trb4)>end||
       (uintptr_t)setup4<base || (uintptr_t)setup4+sizeof(setup4)>end||
       (uintptr_t)data4<base || (uintptr_t)data4+sizeof(data4)>end){
        out("[USB4] unsafe DMA buffer layout\n");return;
    }
    usb_val("[USB4] SRAM event ring",(uintptr_t)ring4,16);
    usb_val("[USB4] SRAM TRB",(uintptr_t)trb4,16);
    out("[USB4] stop old device; no PHY reset\n");
    usb_owned=1;
    int halted=usb_halt4();
    usb_owned=1; /* lock out image jumps even after a failed reset */
    if(halted){
        if(!delay4(300))return;
    }else{
        /* Cold ROM EP0 software state is unavailable. Linux core reset
         * asserts CSFTRST with RUN_STOP cleared before event-ring setup.
         * Do not adopt any old transfer index or overwrite old buffers.
         * Require reset completion AND DEVCTRLHLT before installing DMA. */
        out("[USB4] graceful halt failed; try device-only reset\n");
    }
    out("[USB4] device soft reset (RUN_STOP=0)\n");
    wr4(U_DCTL,U_RESET);
    if(!wait4(U_DCTL,U_RESET,0,500)){
        usb_handoff_regs4();return;
    }
    out("[USB4] CSFTRST cleared\n");
    if(!wait4(U_DSTS,U_HALT,U_HALT,500)){
        out("[USB4] reset complete but controller not halted; refuse DMA setup\n");
        usb_handoff_regs4();return;
    }
    out("[USB4] reset and halt confirmed\n");
    usb_handoff_regs4();
    if(!delay4(halted?50:300))return;
    wr4(0xc110, (rd4(0xc110) & ~((3U << 12) | (1U << 11))) | (2U << 12));
    /* Keep board PHY routing; don't gate USB2 PHY during endpoint commands. */
    wr4(0xc200, rd4(0xc200) & ~((1U << 6) | (1U << 31)));
    wr4(U_DCFG, rd4(U_DCFG) & ~((127U << 3) | 7U | (1U << 22)));
    wr4(0xc720, 0);
    unsigned n = rd4(U_CNT) & 0xfffc;
    if (n)
        wr4(U_CNT, n);
    for (unsigned i = 0; i < 64; ++i)
        ring4[i] = 0;
    cursor4 = 0;
    config4 = 0;config_reported=0;
    setups4 = resets4 = connects4 = 0;
    uint64_t a = (uintptr_t)ring4;
    wr4(0xc400, (uint32_t)a);
    wr4(0xc404, (uint32_t)(a >> 32));
    wr4(0xc408, 0x80000100);
    wr4(0xc708, 7);
    if (!cmd4(0, 9, 0, 0, 0))
        return;
    for (unsigned ep = 0; ep < 2; ++ep) {
        if (!cmd4(ep, 1, 64 << 3, (ep << 25) | (1 << 8) | (1 << 10), 0) ||
           !cmd4(ep, 2, 1, 0, 0))return;
    }
    wr4(0xc720, 3);
    if (!arm4())
        return;
    live4 = 1;
    wr4(U_DCTL, rd4(U_DCTL) | U_RUN);
    out("[USB4] RUN; host should enumerate 1209:0001 (private test PID)\n");
}

static void usbstat4(void) {
    usb_val("owned",usb_owned,1);
    usb_val("live", live4, 1);
    usb_val("phase", phase4, 1);
    usb_val("configuration", config4, 1);
    usb_val("setups", setups4, 8);
    usb_val("resets", resets4, 8);
    usb_val("connects", connects4, 8);
    usb_val("event cursor", cursor4, 2);
    if(usb_owned)usb_handoff_regs4();
    upload_status4();
    usb_val("remote enabled",remote_enabled,1);
}

/* Snapshot: no register writes and no dereference of inherited DMA pointers. */
static void usb_snapshot4(void){
    uint32_t v;
    out("[USB4] read-only snapshot; no reset\n");
    if(!probe32(USB+0xc120,&v)){out("[USB4] GSNPSID abort\n");return;}
    usb_val("GSNPSID",v,8);
    if(v!=0x5533300a){out("[USB4] unexpected core ID\n");return;}
    static const unsigned offs[]={0xc110,0xc118,0xc200,0xc2c0,0xc400,0xc404,0xc408,0xc40c,0xc700,0xc704,0xc708,0xc70c,0xc720};
    for(unsigned i=0;i<sizeof(offs)/sizeof(offs[0]);++i){
        usb_hex(USB+offs[i],8);out(" = ");
        if(!probe32(USB+offs[i],&v)){out("ABORT\n");return;}
        usb_hex(v,8);out("\n");
    }
}

static int usb_stop_for_jump4(void){
    if(!usb_owned)return 1;
    if(usb_halt4())return 1;
    out("[USB4] stop fallback: device reset\n");
    wr4(U_DCTL,U_RESET);
    if(!wait4(U_DCTL,U_RESET,0,500)||!wait4(U_DSTS,U_HALT,U_HALT,500)){
        usb_handoff_regs4();return 0;
    }
    usb_owned=0;config4=0;
    out("[USB4] reset and halt confirmed\n");
    return 1;
}
