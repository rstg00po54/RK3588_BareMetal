#include "cold_steps.h"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
int cold_ctl528_profile(const struct cold_io *,unsigned,unsigned);
struct rec {uintptr_t a;uint32_t v;};
struct mock {struct rec r[64];unsigned n;};
static uint32_t get(struct mock *m,uintptr_t a){for(unsigned i=0;i<m->n;i++)if(m->r[i].a==a)return m->r[i].v;assert(0);return 0;}
static uint32_t rd(void *p,uintptr_t a){uint32_t v=get(p,a);printf("R %08" PRIxPTR " %08" PRIx32 "\n",a,v);return v;}
static void wr(void *p,uintptr_t a,uint32_t v){struct mock *m=p;for(unsigned i=0;i<m->n;i++)if(m->r[i].a==a){m->r[i].v=v;printf("W %08" PRIxPTR " %08" PRIx32 "\n",a,v);return;}assert(0);}
int main(void){unsigned mode,ch,n;struct mock m;struct cold_io io={&m,rd,wr,0};
    while(scanf("%u %u %u",&mode,&ch,&n)==3){assert(n<=64);m.n=n;for(unsigned i=0;i<n;i++)assert(scanf("%" SCNxPTR " %" SCNx32,&m.r[i].a,&m.r[i].v)==2);assert(cold_ctl528_profile(&io,ch,mode)==0);}
    assert(cold_ctl528_profile(&io,4,0)==COLD_BAD_ARG);assert(cold_ctl528_profile(&io,0,2)==COLD_BAD_ARG);return 0;}
