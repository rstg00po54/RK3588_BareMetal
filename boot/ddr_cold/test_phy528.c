#include "cold_steps.h"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
int cold_phy528_electrical(const struct cold_io *,unsigned,unsigned);
int cold_phy528_timing(const struct cold_io *,unsigned);
struct rec {uintptr_t a;uint32_t v;};
struct mock {struct rec r[32];unsigned n,trace,fail,commits,writes,delay;uintptr_t b;};
static uint32_t rd(void *p,uintptr_t a){struct mock *m=p;uint32_t v=0;unsigned found=0;
    if(a==m->b+0x10c84){v=m->fail==m->commits?0:1;found=1;}
    else for(unsigned i=0;i<m->n;i++)if(m->r[i].a==a){v=m->r[i].v;found=1;break;}
    assert(found);if(m->trace)printf("R %08" PRIxPTR " %08" PRIx32 "\n",a,v);return v;}
static void wr(void *p,uintptr_t a,uint32_t v){struct mock *m=p;unsigned j=0;while(j<m->n&&m->r[j].a!=a)j++;
    if(j==m->n){assert(m->n<32);m->n++;}m->r[j]=(struct rec){a,v};m->writes++;
    if(a==m->b+0x10c80&&v==1)m->commits++;
    if(m->trace)printf("W %08" PRIxPTR " %08" PRIx32 "\n",a,v);}
static void delay(void *p,unsigned us){((struct mock*)p)->delay+=us;}
static void init(struct mock *m,unsigned ch){*m=(struct mock){.b=0xf7000000UL+ch*0x1000000UL};
    m->r[m->n++]=(struct rec){m->b+0x500,0x00140009};m->r[m->n++]=(struct rec){m->b+0x504,0x00300000};
    m->r[m->n++]=(struct rec){m->b+0x508,0};m->r[m->n++]=(struct rec){m->b+0x50c,0};}
int main(void){struct mock m;struct cold_io io={&m,rd,wr,delay};unsigned ch,n;uint32_t off,v;
    while(scanf("%u %u",&ch,&n)==2){assert(ch<4&&n==21);init(&m,ch);for(unsigned j=0;j<n;j++){
       assert(scanf("%" SCNx32 " %" SCNx32,&off,&v)==2);m.r[m.n++]=(struct rec){0xfe0c0000UL+ch*0x10000UL+off,v};}
       m.trace=1;assert(cold_phy528_electrical(&io,ch,4)==0);assert(cold_phy528_timing(&io,ch)==0);assert(m.writes==38&&m.commits==2);}
    for(unsigned f=1;f<=2;f++){init(&m,0);for(unsigned j=0;j<0x400;j+=4)if(j==0x3c8||j==0x3d4||j==0x3dc||j==0x3e0||j==0xc||j==0x3ec)m.r[m.n++]=(struct rec){0xfe0c0000UL+j,0};m.fail=f;
       assert(cold_phy528_electrical(&io,0,4)==COLD_TIMEOUT);assert(m.commits==f&&m.delay==3&&m.writes==(f==1?11:16));}
    init(&m,0);assert(cold_phy528_electrical(&io,4,4)==COLD_BAD_ARG&&m.writes==0);
    assert(cold_phy528_timing(&io,4)==COLD_BAD_ARG&&m.writes==0);return 0;}
