#include "cold_steps.h"
#include <assert.h>
#include <stdio.h>
int cold_switch528_fragment(const struct cold_io *,unsigned,unsigned);
struct rec {uintptr_t a;uint32_t v;};
struct mock {struct rec w[64];unsigned n,commits,delay,fail;uint32_t dfi;uintptr_t c,p;};
static uint32_t rd(void *ctx,uintptr_t a){struct mock *m=ctx;
    if(a==m->c+0xc84)return m->fail==m->commits?0:1;
    if(a==m->c+0x514)return m->commits==1?(m->fail==4?1:0):(m->fail==6?0:1);
    if(a==m->c+0x510)return m->dfi;
    if(a==m->p+0x18)return m->fail==5?0:0x8000;
    if(a==0xfd7c0594UL)return 0x80;
    assert(0);return 0;
}
static void wr(void *ctx,uintptr_t a,uint32_t v){struct mock *m=ctx;assert(m->n<64);m->w[m->n++]=(struct rec){a,v};if(a==m->c+0xc80&&v==1)++m->commits;if(a==m->c+0x510)m->dfi=v;}
static void delay(void *ctx,unsigned n){((struct mock*)ctx)->delay+=n;}
static void eq(struct mock *m,unsigned j,uintptr_t a,uint32_t v){assert(j<m->n&&m->w[j].a==a&&m->w[j].v==v);}
static struct mock init(unsigned ch,unsigned fail){return (struct mock){.c=0xf7010000UL+ch*0x1000000UL,.p=0xfd800000UL+ch*0x4000UL,.dfi=5,.fail=fail};}
int main(void){struct mock m=init(0,0);struct cold_io i={&m,rd,wr,delay};
    for(unsigned ch=0;ch<4;ch++){
        m=init(ch,0);assert(cold_switch528_fragment(&i,ch,4)==COLD_OK);
        assert(m.n==24&&m.commits==3&&m.dfi==5&&m.delay==2);
        uintptr_t g=(ch<2?0xfd59c000UL:0xfd59d000UL)+0x130+(ch&1)*4;
        eq(&m,0,m.c+0xc80,0);eq(&m,1,m.c+0x510,4);eq(&m,2,m.c+0x510,0x24);eq(&m,3,m.c+0xc80,1);
        eq(&m,4,g,0x80000);eq(&m,5,g,0x30000);eq(&m,6,0xfd7c0594UL,0x00c00040);
        eq(&m,9,m.p,0x03ff0058);eq(&m,10,m.p+4,0x01ff0081);eq(&m,13,m.p,0x80000000);
        eq(&m,14,0xfd7c0594UL,0x00c00080);eq(&m,15,m.p+0x300,0x10000);
        eq(&m,16,m.p+0xa00,0x20000);eq(&m,17,g,0x80008);
    }
    const unsigned counts[]={0,4,21,24,4,14,21};
    for(unsigned f=1;f<=6;f++){m=init(0,f);assert(cold_switch528_fragment(&i,0,4)==COLD_TIMEOUT);assert(m.n==counts[f]);if(f==5){eq(&m,13,0xfd7c0594UL,0x00c00080);assert(m.dfi==0x24);}}
    m=init(0,0);assert(cold_switch528_fragment(&i,4,4)==COLD_BAD_ARG&&m.n==0);
    puts("528 MHz frequency switch: 4 channel address tests + 6 timeout tests PASS");
}
