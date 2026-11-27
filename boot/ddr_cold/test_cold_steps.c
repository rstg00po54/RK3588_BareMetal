#include "cold_steps.h"
#include <assert.h>
#include <stdio.h>
struct write_rec { uintptr_t a; uint32_t v; };
struct mock { struct write_rec w[16]; unsigned n, reads, delay, fail; };
static void write_mock(void *ctx, uintptr_t a,uint32_t v) {
    struct mock *m=ctx;
    assert(m->n<16);
    m->w[m->n++]=(struct write_rec){a,v};
}
static uint32_t read_mock(void *ctx, uintptr_t a) {
    struct mock *m=ctx;
    ++m->reads;
    if(m->fail) return 0;
    if(a==0xfd8d8118UL||a==0xfd8d8120UL) return 0x10000000U;
    if(a==0xfd8d8184UL) return m->n==3?1:0;
    return 0x8000U;
}
static void delay_mock(void *ctx,unsigned us) { ((struct mock*)ctx)->delay+=us; }
int main(void) {
    struct mock m={0};
    struct cold_io io={&m,read_mock,write_mock,delay_mock};
    assert(cold_global_handshake(&io,4)==COLD_OK);
    assert(m.n==6 && m.reads==4);
    const uintptr_t a[]={0xfd8d8110UL,0xfd8d8160UL,0xfd8d8154UL,
                         0xfd8d8154UL,0xfd8d8160UL,0xfd8d8110UL};
    const uint32_t v[]={0xf000f000U,0x30003U,0x30003U,0x30000U,0x30000U,0xf0000000U};
    for(unsigned i=0;i<6;i++){assert(m.w[i].a==a[i]);assert(m.w[i].v==v[i]);}
    m=(struct mock){.fail=1};
    assert(cold_global_handshake(&io,4)==COLD_TIMEOUT && m.n==1 && m.reads==4);
    m=(struct mock){0};
    assert(cold_pll_integer_fragment(&io,2,0,88,1,2,4)==COLD_OK);
    assert(m.n==7 && m.delay==1 && m.reads==1);
    assert(m.w[2].a==0xfd808000UL && m.w[2].v==0x03ff0058U);
    assert(m.w[3].a==0xfd808004UL && m.w[3].v==0x01ff0081U);
    assert(m.w[6].v==0x80000000U);
    m=(struct mock){.fail=1};
    assert(cold_pll_integer_fragment(&io,0,1,130,1,1,4)==COLD_TIMEOUT);
    assert(m.n==6 && m.reads==4 && m.w[0].a==0xfd800020UL);
    assert(cold_pll_integer_fragment(&io,4,0,88,1,2,4)==COLD_BAD_ARG);
    puts("DDR cold steps mock PASS");
    return 0;
}
