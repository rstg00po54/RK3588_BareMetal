#include "train528_mode8.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
struct reg {uintptr_t a;uint32_t v;};
struct mock {struct reg r[128];unsigned n,trace,fail,delay,pulses;uintptr_t p,c;unsigned period;uint32_t raw[10];};
static uint32_t get(struct mock *m,uintptr_t a){for(unsigned j=0;j<m->n;j++)if(m->r[j].a==a)return m->r[j].v;return 0;}
static void set(struct mock *m,uintptr_t a,uint32_t v){unsigned j=0;while(j<m->n&&m->r[j].a!=a)j++;if(j==m->n){assert(m->n<128);m->n++;}m->r[j]=(struct reg){a,v};}
static uint32_t rd(void *ctx,uintptr_t a){struct mock *m=ctx;uint32_t v=get(m,a);
    if(a==m->p+0xa24&&(v&1)&&m->fail!=1)v|=2;
    if(a==m->p+0xe8&&m->fail==2)v=1;
    if(a==m->p+0xec&&m->fail==3)v=2;
    if(a==m->p+0xb4)v=m->period;
    for(unsigned j=0;j<10;j++)if(a==m->p+0x1f0+(j==9?0x64:12*j))v=m->raw[j];
    if(m->trace)printf("R %08" PRIxPTR " %08" PRIx32 "\n",a,v);
    return v;
}
static void wr(void *ctx,uintptr_t a,uint32_t v){struct mock *m=ctx;set(m,a,v);if(a==m->p+0x50&&(v&0x80000))m->pulses++;if(m->trace)printf("W %08" PRIxPTR " %08" PRIx32 "\n",a,v);}
static void delay(void *ctx,unsigned us){((struct mock*)ctx)->delay+=us;}
static void init(struct mock *m,unsigned ch,unsigned fail,unsigned period){*m=(struct mock){.p=0xfe0c0000UL+ch*0x10000UL,.c=0xf7010000UL+ch*0x1000000UL,.fail=fail,.period=period};
    set(m,m->c+0x280,0x20000000);set(m,m->p+4,0x30000000);set(m,m->p+8,0x800000);set(m,m->p+0xb0,0x2002ef7f);set(m,m->p+0xb8,0x2210);
    for(unsigned j=0;j<9;j++)m->raw[j]=0x01000080;
}
int main(int argc,char **argv){(void)argv;struct mock m;struct cold_io i={&m,rd,wr,delay};struct cold_mode8_result r;
    if(argc>1){unsigned fail;assert(scanf("%u",&fail)==1);init(&m,0,fail,256);m.trace=1;printf("RESULT %d\n",cold_train528_mode8(&i,0,4,&r));return 0;}
    unsigned cases=0;
    for(unsigned ch=0;ch<4;ch++)for(unsigned period=160;period<=2047;period+=19)for(unsigned parity=0;parity<2;parity++){
        init(&m,ch,0,period);if(!parity)set(&m,m.p+0xb0,0x2002ef7e);
        unsigned L=parity?period:period/2;
        for(unsigned j=0;j<10;j++){unsigned a=(j*71+period)%((7*L-L/2)<1024?(7*L-L/2):1024),b=(j*33+period/2)%((7*L-L/2)<1024?(7*L-L/2):1024);m.raw[j]=a|(b<<16);}
        assert(cold_train528_mode8(&i,ch,4,&r)==0&&r.period==L&&m.pulses==3&&m.delay==3);
        for(unsigned j=0;j<10;j++){unsigned a=(m.raw[j]&1023)+L/2,b=((m.raw[j]>>16)&1023)+L/2;assert(r.raw[j]==m.raw[j]&&r.fine[j]==((a%L)|((b%L)<<16)));assert(((r.coarse_lo>>(3*j))&7)==a/L&&((r.coarse_hi>>(3*j))&7)==b/L);assert(!get(&m,m.p+0x8b0+4*j)&&!get(&m,m.p+0x910+4*j)&&!get(&m,m.p+0x880+4*j)&&!get(&m,m.p+0x8e0+4*j));}
        assert(get(&m,m.c+0x280)==0x20000000&&!get(&m,m.c+0x208));cases++;
    }
    for(unsigned f=1;f<=3;f++){init(&m,0,f,256);r.period=999;assert(cold_train528_mode8(&i,0,4,&r)==(f==1?COLD_TIMEOUT:-4)&&r.period==999);assert(m.pulses==(f==1?0:2));}
    init(&m,0,0,0);r.period=999;assert(cold_train528_mode8(&i,0,4,&r)==-3&&r.period==999&&!m.pulses);
    init(&m,0,0,1);assert(cold_train528_mode8(&i,0,4,&r)==-3&&r.period==999&&!m.pulses);
    init(&m,0,0,256);assert(cold_train528_mode8(&i,4,4,&r)==COLD_BAD_ARG&&!m.pulses);
    printf("mode8: %u successful vectors, 5 injected failures, invalid argument PASS\n",cases);
}
