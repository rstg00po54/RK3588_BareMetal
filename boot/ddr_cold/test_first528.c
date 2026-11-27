#include "first528.h"
#include <assert.h>
#include <stdio.h>
struct reg {uintptr_t a;uint32_t v;};
struct mock {
    struct reg r[1024], writes[2048];unsigned n,nw,reads,delay,started,completed,switched;
    struct first528_result *status;
    enum first528_stage fail;unsigned failch,fail_reads;
};
static uint32_t get(struct mock *m,uintptr_t a){for(unsigned j=0;j<m->n;j++)if(m->r[j].a==a)return m->r[j].v;return 0;}
static void set(struct mock *m,uintptr_t a,uint32_t v){unsigned j=0;while(j<m->n&&m->r[j].a!=a)j++;if(j==m->n){assert(m->n<1024);m->n++;}m->r[j]=(struct reg){a,v};}
static uintptr_t b(unsigned ch){return 0xf7000000UL+ch*0x1000000UL;}
static uintptr_t p(unsigned ch){return 0xfe0c0000UL+ch*0x10000UL;}
static unsigned failure(struct mock *m,enum first528_stage s,unsigned ch){return m->fail==s&&m->failch==ch;}
static uint32_t rd(void *ctx,uintptr_t a){struct mock *m=ctx;m->reads++;uint32_t v=get(m,a);unsigned ch=m->status->ch;enum first528_stage s=m->status->stage;
    if(ch<4){uintptr_t c=b(ch)+0x10000,phy=p(ch),pc=0xfd800000UL+ch*0x4000UL;
        if(a==c+0xc84){assert(get(m,c+0xc80)==1);if(failure(m,s,ch)&&s!=F528_SWITCH){m->fail_reads++;return 0;}return 1;}
        if(a==c+0x514){if(s==F528_DFI_START){assert(m->completed==0);if(failure(m,s,ch)){m->fail_reads++;return 0;}m->started|=1U<<ch;return 1;}
            assert(s==F528_SWITCH&&m->completed==15);return !(get(m,c+0x510)&0x20);}
        if(a==c+0x14&&s==F528_NORMAL){assert(m->completed==15);if(failure(m,s,ch)){m->fail_reads++;return 0;}return 1;}
        if(a==c+0x90&&s==F528_MR){if(failure(m,s,ch)){m->fail_reads++;return 1;}return 0;}
        if(a==c+0x80&&s==F528_MR)return v&0x7fffffff;
        if(a==c+0xb90&&s==F528_ZQ){if(failure(m,s,ch)){m->fail_reads++;return 0x10000;}return 0;}
        if(a==phy+0x3cc&&s==F528_PHY_ACK){if(failure(m,s,ch)){m->fail_reads++;return 0;}return 1;}
        if(a==pc+0x18&&s==F528_SWITCH){if(failure(m,s,ch)){m->fail_reads++;return 0;}m->switched|=1U<<ch;return 0x8000;}
        if(a==phy+0xb4&&s==F528_PHY_READY){assert(m->switched==15);if(failure(m,s,ch)){m->fail_reads++;return 0;}return 0x40000;}
    }
    return v;
}
static void wr(void *ctx,uintptr_t a,uint32_t v){struct mock *m=ctx;assert(m->nw<2048);m->writes[m->nw++]=(struct reg){a,v};
    unsigned ch=m->status->ch;enum first528_stage s=m->status->stage;
    if(s==F528_DFI_COMPLETE){assert(m->started==15);if(a==b(ch)+0x10c80&&v==1)m->completed|=1U<<ch;}
    if(s==F528_RESTORE)assert(m->switched==15);
    set(m,a,v);
}
static void delay(void *ctx,unsigned us){((struct mock*)ctx)->delay+=us;}
static void init(struct mock *m,struct first528_result *r,enum first528_stage fail,unsigned ch){*m=(struct mock){.status=r,.fail=fail,.failch=ch};set(m,0xfd7c0280,0x104);set(m,0xfd7c0304,0x165);set(m,0xfd7c0594,0x80);for(unsigned j=0;j<4;j++){set(m,p(j),0x44005011);set(m,p(j)+0xa1c,0x20820);set(m,p(j)+0xb0,0x2000017e);set(m,p(j)+0xb8,0x2640);}}
int main(void){struct first528_result r;struct mock m;struct cold_io io={&m,rd,wr,delay};init(&m,&r,F528_NONE,4);
    assert(cold_first528(&io,4,&r)==0&&r.stage==F528_DONE&&r.ch==4&&r.rc==0);
    assert(m.started==15&&m.completed==15&&m.switched==15);
    assert(get(&m,0xfd7c0280)==0xffff0104&&get(&m,0xfd7c0304)==0xffff0165);
    for(unsigned ch=0;ch<4;ch++){
        assert(get(&m,b(ch)+0x30004)==0x3f3f3f07&&get(&m,b(ch)+0x3000c)==0x70707);
        assert(get(&m,b(ch)+0x3001c)==0x1f080808);
        assert(get(&m,b(ch)+0x10510)==5&&get(&m,b(ch)+0x508)==0x5c&&get(&m,b(ch)+0x50c)==0x6005c);
        assert(get(&m,p(ch)+0xbf4)==9&&(get(&m,p(ch)+0xb8)&0xfff)==528&&get(&m,p(ch)+0xb0)==0x2002ef7f);
        assert(get(&m,0xfd7c0000UL+4*(0x294+ch+ch/2))==(0x1180U>>(4*(ch&1)))<<16);
    }
    printf("first528: 4-channel ordered mock success, %u writes, %u reads\n",m.nw,m.reads);
    const enum first528_stage failures[]={F528_ELECTRICAL,F528_RELEASE,F528_DFI_START,F528_NORMAL,F528_MR,F528_ZQ,F528_PHY_ACK,F528_SWITCH,F528_PHY_READY};
    unsigned cases=0;
    for(unsigned ch=0;ch<4;ch++)for(unsigned j=0;j<sizeof failures/sizeof failures[0];j++){
        init(&m,&r,failures[j],ch);assert(cold_first528(&io,4,&r)==COLD_TIMEOUT);assert(r.stage==failures[j]&&r.ch==ch&&r.rc==COLD_TIMEOUT&&m.fail_reads==4);
        if(r.stage==F528_DFI_START)assert(!m.completed&&!m.switched);
        cases++;
    }
    /* DFI completion failure must stop before MR commands. */
    for(unsigned ch=0;ch<4;ch++){init(&m,&r,F528_DFI_COMPLETE,ch);assert(cold_first528(&io,4,&r)==COLD_TIMEOUT&&r.stage==F528_DFI_COMPLETE&&r.ch==ch&&!m.switched);cases++;}
    init(&m,&r,F528_NONE,4);assert(cold_first528(&io,0,&r)==COLD_BAD_ARG&&m.nw==0);
    printf("first528: %u injected stage/channel failures + invalid argument PASS\n",cases);
}
