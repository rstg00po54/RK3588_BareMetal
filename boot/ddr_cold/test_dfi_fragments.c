#include "cold_steps.h"
#include <assert.h>
#include <stdio.h>
int cold_dfi_start_fragment(const struct cold_io *,unsigned,unsigned);
int cold_dfi_complete_fragment(const struct cold_io *,unsigned,unsigned);
int cold_mrw_fragment(const struct cold_io *,unsigned,unsigned,unsigned,unsigned,unsigned);
int cold_phy_ack_fragment(const struct cold_io *,unsigned,unsigned);
int cold_mrr_fragment(const struct cold_io *,unsigned,unsigned,unsigned,unsigned,uint8_t *);
struct rec { uintptr_t a; uint32_t v; };
struct mock { struct rec w[16]; unsigned n, delay, fail, mrr; };
static uint32_t read_mock(void *ctx,uintptr_t a)
{
    struct mock *m=ctx;
    unsigned off=a&0xfff;
    if(m->mrr){
        if(off==0x90)return m->n<4?(m->fail==7?1:0):(m->fail==9?0:0x10000);
        if(off==0x80 && m->n>=3)return m->n>=4 && m->fail==8?0x80000011U:0x11;
        if(off==0x94)return 0x1234565c;
    }
    if(off==0xc84)return m->fail==1?0:1;
    if(off==0x514)return m->fail==2?0:1;
    if(off==0x90)return m->fail==3 || (m->fail==5 && m->n>=3)?1:0;
    if(off==0x80 && m->n>=3)return m->fail==4?0x80000030U:0x30;
    if(off==0x3cc)return m->fail==6?0:1;
    for(unsigned i=m->n;i;i--)if(m->w[i-1].a==a)return m->w[i-1].v;
    if(off==0xb0)return 0x2000017e;
    if(off==0x510)return 4;
    if(off==0x3d0)return 0x12345678;
    if(off==0x3c8)return 0xabcdef01;
    return 0;
}
static void write_mock(void *ctx,uintptr_t a,uint32_t v)
{
    struct mock *m=ctx;assert(m->n<16);m->w[m->n++]=(struct rec){a,v};
}
static void delay_mock(void *ctx,unsigned us){((struct mock*)ctx)->delay+=us;}
static void eq(struct mock *m,unsigned i,uintptr_t a,uint32_t v)
{
    assert(i<m->n && m->w[i].a==a && m->w[i].v==v);
}
int main(void)
{
    struct mock m={0};struct cold_io io={&m,read_mock,write_mock,delay_mock};
    assert(cold_dfi_start_fragment(&io,0,4)==COLD_OK && m.n==4);
    eq(&m,0,0xfe0c00b0UL,0x2000015e);eq(&m,1,0xf7010c80UL,0);
    eq(&m,2,0xf7010510UL,0x24);eq(&m,3,0xf7010c80UL,1);
    assert(cold_dfi_complete_fragment(&io,0,4)==COLD_OK && m.n==8);
    eq(&m,5,0xf7010510UL,4);eq(&m,6,0xf7010510UL,5);
    for(unsigned f=1;f<=2;f++){
        m=(struct mock){.fail=f};assert(cold_dfi_start_fragment(&io,0,4)==COLD_TIMEOUT);
        assert(m.n==4 && m.delay==3);
    }
    m=(struct mock){0};assert(cold_mrw_fragment(&io,2,3,11,0x5c,4)==COLD_OK && m.n==3);
    eq(&m,0,0xf9010080UL,0x30);eq(&m,1,0xf9010084UL,0xb5c);eq(&m,2,0xf9010080UL,0x80000030);
    for(unsigned f=3;f<=5;f++){
        m=(struct mock){.fail=f};assert(cold_mrw_fragment(&io,0,3,11,0,4)==COLD_TIMEOUT);
        assert(m.n==(f==3?0:3) && m.delay==3);
    }
    m=(struct mock){0};assert(cold_phy_ack_fragment(&io,0,4)==COLD_OK && m.n==4);
    eq(&m,0,0xfe0c03d0UL,0x12340007);eq(&m,1,0xfe0c03c8UL,(0xabcdef01U&0xfffbfff3U)|0x40004U);
    eq(&m,3,0xfe0c03c8UL,((0xabcdef01U&0xfffbfff3U)|4U)&0xfffbfffdU);
    m=(struct mock){.fail=6};assert(cold_phy_ack_fragment(&io,0,4)==COLD_TIMEOUT && m.n==3 && m.delay==3);
    m=(struct mock){0};assert(cold_dfi_start_fragment(&io,4,4)==COLD_BAD_ARG && m.n==0);
    assert(cold_mrw_fragment(&io,0,0,11,0,4)==COLD_BAD_ARG && m.n==0);
    uint8_t data=0;
    m=(struct mock){.mrr=1};assert(cold_mrr_fragment(&io,0,1,8,4,&data)==COLD_OK && m.n==4 && data==0x5c);
    eq(&m,0,0xf7010080UL,0x01000030);eq(&m,1,0xf7010080UL,0x11);
    eq(&m,2,0xf7010084UL,0x800);eq(&m,3,0xf7010080UL,0x80000011);
    for(unsigned f=7;f<=9;f++){
        data=0xa5;m=(struct mock){.mrr=1,.fail=f};
        assert(cold_mrr_fragment(&io,0,1,8,4,&data)==COLD_TIMEOUT);
        assert(m.n==(f==7?1:4) && m.delay==3 && data==0xa5);
    }
    puts("DFI/MRW/MRR/PHY fragments: mock success, timeout and argument tests PASS");
}
