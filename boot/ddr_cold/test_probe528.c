#include "probe528.h"
#ifdef PROBE528_EMBEDDED
#include "../sram/app_abi.h"
/* An assertion in the mock is a latched failure, not an EL3 trap. */
static volatile unsigned mock_failed __attribute__((section(".data"))) = 1;
#define assert(x) do { if (!(x)) mock_failed=1; } while (0)
#else
#include <assert.h>
#include <stdio.h>
#endif
struct reg {uintptr_t a;uint32_t v;};
struct mock {struct reg r[1024];unsigned n,reads,writes,delay,started,completed,switched,checked,mr14,rank8,rank0,done;unsigned dual,width,failch;enum probe528_stage fail;struct probe528_result *s;};
static uintptr_t b(unsigned ch){return 0xf7000000UL+ch*0x1000000UL;}
static uintptr_t p(unsigned ch){return 0xfe0c0000UL+ch*0x10000UL;}
static uint32_t get(struct mock *m,uintptr_t a){for(unsigned j=0;j<m->n;j++)if(m->r[j].a==a)return m->r[j].v;return 0;}
static void set(struct mock *m,uintptr_t a,uint32_t v){unsigned j=0;while(j<m->n&&m->r[j].a!=a)j++;if(j==m->n){if(m->n>=1024){assert(0);return;}m->n++;}m->r[j]=(struct reg){a,v};}
static uint32_t rd(void *ctx,uintptr_t a){struct mock *m=ctx;m->reads++;uint32_t v=get(m,a);struct probe528_result *s=m->s;
    unsigned ch=s->stage==P528_FIRST?s->first.ch:s->ch;if(ch>=4)return v;
    uintptr_t c=b(ch)+0x10000,phy=p(ch),pc=0xfd800000UL+ch*0x4000UL;
    if(s->stage==P528_FIRST){enum first528_stage f=s->first.stage;
        if(a==c+0xc84)return 1;
        if(a==c+0x514){if(f==F528_DFI_START){assert(!m->completed);m->started|=1U<<ch;return 1;}assert(f==F528_SWITCH&&m->completed==15);return !(get(m,c+0x510)&0x20);}
        if(a==c+0x14&&f==F528_NORMAL)return 1;
        if(a==c+0x90&&f==F528_MR)return 0;
        if(a==c+0x80&&f==F528_MR)return v&0x7fffffff;
        if(a==c+0xb90&&f==F528_ZQ)return 0;
        if(a==phy+0x3cc&&f==F528_PHY_ACK)return 1;
        if(a==pc+0x18&&f==F528_SWITCH){m->switched|=1U<<ch;return 0x8000;}
        if(a==phy+0xb4&&f==F528_PHY_READY){assert(m->switched==15);return 0x40100;}
        return v;
    }
    int fail=s->stage==m->fail&&ch==m->failch;
    if(a==c+0xc84)return 1;
    if(a==c+0x80)return v&0x7fffffff;
    if(a==c+0x90)return fail&&s->stage!=P528_MODE8&&s->stage!=P528_MR14_CHECK?1:0x10000;
    if(a==c+0x94){unsigned mr=(get(m,c+0x84)>>8)&255,rank=(get(m,c+0x80)>>4)&3;
        assert(rank==1||rank==2);
        if(s->stage==P528_CA){assert(mr==12&&rank==1);unsigned d=(get(m,phy+0xa2c)>>6)&31;return d>=8&&d<=14?0x5c:0;}
        if(s->stage==P528_MR14_CHECK){assert(ch==0&&rank==1&&mr==14);m->checked++;return fail?0x5d:0x40;}
        if(mr==8)return rank==2&&m->width?0x50:0x10;
        if(mr==0)return m->width==2?2:0;
        if(rank==2&&!m->dual&&mr==12)return 0;
        return mr==5?1:0x5c;
    }
    if(a==phy+0xa24&&(v&1))return fail?v:v|2;
    if(a==phy+0xb4)return 0x40100;
    for(unsigned j=0;j<10;j++)if(a==phy+0x1f0+(j==9?0x64:12*j))return 0x01000080;
    return v;
}
static void wr(void *ctx,uintptr_t a,uint32_t v){struct mock *m=ctx;struct probe528_result *s=m->s;m->writes++;
    if(s->stage==P528_FIRST){unsigned ch=s->first.ch;if(ch<4&&s->first.stage==F528_DFI_COMPLETE){assert(m->started==15);if(a==b(ch)+0x10c80&&v==1)m->completed|=1U<<ch;}}
    else if(s->ch<4){unsigned ch=s->ch;uintptr_t c=b(ch)+0x10000;assert(m->switched==15);assert(s->candidate_mask==((1U<<ch)-1));
        if(a==c+0x80&&(v>>31)){unsigned mr=(get(m,c+0x84)>>8)&255;
            if(s->stage==P528_MR14_WRITE){assert(mr==14&&(v&0x30)==0x30&&!(v&1));if(!ch)assert(m->checked==1);m->mr14|=1U<<ch;}
            if(s->stage==P528_RANK_MR8)m->rank8|=1U<<ch;
            if(s->stage==P528_RANK_MR0)m->rank0|=1U<<ch;}
        if(s->stage==P528_GEOMETRY&&a==c+0x180)m->done|=1U<<ch;
    }set(m,a,v);}
static void delay(void *ctx,unsigned us){((struct mock*)ctx)->delay+=us;}
static void init(struct mock *m,struct probe528_result *s,unsigned dual,unsigned width){*m=(struct mock){.s=s,.dual=dual,.width=width,.failch=4};set(m,0xfd7c0280,0x104);set(m,0xfd7c0304,0x165);set(m,0xfd7c0594,0x80);
    for(unsigned j=0;j<4;j++){set(m,p(j),0x44005011);set(m,p(j)+0xa1c,0x20820);set(m,p(j)+0xb0,0x2000017e);set(m,p(j)+0xb8,0x2640);set(m,b(j)+0x10180,0x12340000+j);}}
#ifdef PROBE528_EMBEDDED
/* The ABI reserves only 4 KiB for stack: explicitly loaded .data, not BSS. */
static struct mock embedded_mock __attribute__((section(".data"))) = {.n=1};
static struct probe528_result embedded_result __attribute__((section(".data"))) = {.ch=4};
uint32_t app_main(const struct sram_app_api *api){
    if(!api||api->abi!=SRAM_APP_ABI||api->size<sizeof(*api)||!api->puts)return 0x528d00ffU;
    struct mock *m=&embedded_mock;struct probe528_result *s=&embedded_result;
    struct cold_io io={m,rd,wr,delay};
    api->puts("[DDR528 DRY] SRAM-only mock; NO MMIO\n");
    mock_failed=0;
    for(unsigned scenario=0;scenario<3;scenario++){
        unsigned dual=scenario!=0,width=scenario==2?2:0;init(m,s,dual,width);
        int rc=cold_probe528(&io,4,s);
        if(rc||mock_failed||s->stage!=P528_DONE||s->candidate_mask!=15||m->done!=15||m->mr14!=15||m->checked!=1){
            api->puts("[DDR528 DRY] FAIL\n");return 0x528d0010U+scenario;
        }
        for(unsigned ch=0;ch<4;ch++){
            struct probe528_channel *r=&s->channel[ch];
            if(r->ca.selected!=10||r->mode8.period!=256||r->geometry.g[0]!=1+dual||r->geometry.g[6]!=16||r->geometry.g[7]!=(dual?16+(width!=0):0)||get(m,b(ch)+0x10180)!=0x12340000+ch)return 0x528d0020U+scenario;
        }
        if(m->rank8!=(dual?15:0)||m->rank0!=(width?15:0))return 0x528d0030U+scenario;
        api->puts("[DDR528 DRY] four-channel scenario PASS\n");
    }
    api->puts("[DDR528 DRY] PASS; real DDR unchanged\n");return 0x528d0000U;
}
#else
int main(void){struct mock m;struct probe528_result s;struct cold_io io={&m,rd,wr,delay};
    for(unsigned scenario=0;scenario<3;scenario++){unsigned dual=scenario!=0,width=scenario==2?2:0;init(&m,&s,dual,width);
        assert(cold_probe528(&io,4,&s)==0&&s.stage==P528_DONE&&s.candidate_mask==15&&m.done==15&&m.mr14==15&&m.checked==1);
        for(unsigned ch=0;ch<4;ch++){struct probe528_channel *r=&s.channel[ch];assert(r->ca.selected==10&&r->mode8.period==256&&r->geometry.g[0]==1+dual&&r->geometry.g[6]==16&&r->geometry.g[7]==(dual?16+(width!=0):0));assert(get(&m,b(ch)+0x10180)==0x12340000+ch);}
        assert(m.rank8==(dual?15:0)&&m.rank0==(width?15:0));printf("probe528: scenario %u candidate mask %x, %u writes, %u reads PASS\n",scenario,s.candidate_mask,m.writes,m.reads);}
    unsigned n=0;const enum probe528_stage f[]={P528_CA,P528_MR14_WRITE,P528_MODE8,P528_MR8,P528_RANK_COMPARE,P528_RANK_MR8,P528_RANK_MR0};
    for(unsigned ch=0;ch<4;ch++)for(unsigned j=0;j<sizeof f/sizeof f[0];j++){init(&m,&s,1,2);m.fail=f[j];m.failch=ch;assert(cold_probe528(&io,4,&s)==COLD_TIMEOUT&&s.stage==f[j]&&s.ch==ch&&s.candidate_mask==(1U<<ch)-1);n++;}
    init(&m,&s,1,0);m.fail=P528_MR14_CHECK;m.failch=0;assert(cold_probe528(&io,4,&s)==-5&&s.stage==P528_MR14_CHECK&&!s.candidate_mask&&!m.mr14);n++;
    init(&m,&s,1,1);assert(cold_probe528(&io,4,&s)==-6&&s.stage==P528_GEOMETRY&&!s.candidate_mask);n++;
    init(&m,&s,0,0);assert(cold_probe528(&io,0,&s)==COLD_BAD_ARG&&!m.writes);printf("probe528: %u injected failures + invalid argument PASS\n",n);
}
#endif
