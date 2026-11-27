#include "train528.h"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
struct reg {uintptr_t a;uint32_t v;};
struct mock {struct reg r[32];unsigned n, trace, ch, commits, mr, fail, delay;uint32_t mask;uintptr_t b,c,p;};
static uint32_t get(struct mock *m,uintptr_t a){for(unsigned j=0;j<m->n;j++)if(m->r[j].a==a)return m->r[j].v;return 0;}
static void set(struct mock *m,uintptr_t a,uint32_t v){unsigned j=0;while(j<m->n&&m->r[j].a!=a)j++;if(j==m->n){assert(m->n<32);m->n++;}m->r[j]=(struct reg){a,v};}
static uint32_t rd(void *ctx,uintptr_t a){struct mock *m=ctx;uint32_t v=get(m,a);
    if(a==m->c+0xc84)v=m->fail==1?0:1;
    if(a==m->c+0x80)v&=m->fail==3?0xffffffff:0x7fffffff;
    if(a==m->c+0x90)v=m->fail==2?1:m->fail==4?0:0x10000;
    if(a==m->c+0x94){assert(get(m,m->c+0x84)==0xc00);unsigned d=(get(m,m->p+0xa2c)>>6)&31;v=(m->mask&(1U<<d))?0x5c:0;if(m->fail==5&&m->mr==10)v=0;}
    if(m->trace)printf("R %08" PRIxPTR " %08" PRIx32 "\n",a,v);
    return v;}
static void wr(void *ctx,uintptr_t a,uint32_t v){struct mock *m=ctx;set(m,a,v);if(a==m->c+0xc80&&v==1)m->commits++;if(a==m->c+0x80&&(v>>31))m->mr++;if(m->trace)printf("W %08" PRIxPTR " %08" PRIx32 "\n",a,v);}
static void delay(void *ctx,unsigned us){((struct mock*)ctx)->delay+=us;}
static void init(struct mock *m,unsigned ch,unsigned base,uint32_t mask,unsigned fail){*m=(struct mock){.ch=ch,.b=0xf7000000UL+ch*0x1000000UL,.c=0xf7010000UL+ch*0x1000000UL,.p=0xfe0c0000UL+ch*0x10000UL,.mask=mask,.fail=fail};
    set(m,m->c+0x280,0x20000000);set(m,m->p+4,0x30000000);set(m,m->p+0xb0,0x2002ef7f);set(m,m->p,0x40005000|base);set(m,m->p+0xa2c,0x20146);set(m,m->b+0x508,0x5c);set(m,m->b+0x580,0x07190204);}
int main(int argc,char **argv){(void)argv;struct mock m;struct cold_io i={&m,rd,wr,delay};struct cold_ca_result r;
    if(argc>1){unsigned mask;assert(scanf("%x",&mask)==1);init(&m,0,10,mask,0);m.trace=1;int rc=cold_train528_ca(&i,0,4,&r);printf("RESULT %d\n",rc);return 0;}
    unsigned pass=0,reject=0;
    for(unsigned ch=0;ch<4;ch++)for(unsigned base=9;base<=10;base++)for(unsigned k=0;k<512;k++){
        uint32_t mask=0;unsigned lo=99,hi=0;for(unsigned j=0;j<9;j++)if(k&(1U<<j)){unsigned d=(base&1?1:2)+2*j;mask|=1U<<d;if(d<lo)lo=d;hi=d;}
        unsigned pick=lo==hi?lo:lo+2;init(&m,ch,base,mask,0);r=(struct cold_ca_result){.base=99};
        int rc=cold_train528_ca(&i,ch,4,&r);
        if(!mask||!(mask&(1U<<pick))){assert(rc==(!mask?-3:-4)&&r.base==99&&m.mr==9&&m.commits==9);reject++;continue;}
        assert(rc==0&&r.passing==mask&&r.selected==pick&&r.dfi==base-3+pick);assert(m.mr==10&&m.commits==10&&m.delay==1);
        assert(get(&m,m.c+0x280)==0x20000000&&get(&m,m.c+0x208)==0&&get(&m,m.p+0xb0)==0x2002ef7f);pass++;
    }
    for(unsigned f=1;f<=5;f++){init(&m,0,10,0x5500,f);r=(struct cold_ca_result){.base=99};assert(cold_train528_ca(&i,0,4,&r)==(f==5?-4:COLD_TIMEOUT));assert(r.base==99);assert(m.commits==(f==5?10:1));}
    init(&m,0,10,0,0);assert(cold_train528_ca(&i,4,4,&r)==COLD_BAD_ARG&&m.commits==0);
    printf("CA training: %u success, %u rejected windows, 5 injected failure paths PASS\n",pass,reject);
}
