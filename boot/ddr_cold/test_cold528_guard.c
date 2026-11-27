#include "cold528_guard.h"
#include <assert.h>
#include <stdio.h>
struct mock {unsigned fault,ch,reads;};
static uint32_t rd(void *v,uintptr_t a){struct mock *m=v;m->reads++;
    if(a>=0xfd58a208UL&&a<=0xfd58a214UL)return (m->fault&G528_OS)&&a==0xfd58a208UL+4*m->ch;
    if(a==0xf7010014UL)return (m->fault&G528_STAT)?1:0;
    if(a==0xfd8d8110UL)return (m->fault&G528_IDLE)?1:0;
    if(a==0xfd7c0280UL)return 0x104;
    if(a==0xfd7c0304UL)return 0x165;
    for(unsigned ch=0;ch<4;ch++){
        uintptr_t p=0xfd800000UL+ch*0x4000UL;
        if(a==p||a==p+0x20)return (m->fault&G528_PLL)&&ch==m->ch?0x58:0x8073;
        if(a==p+4||a==p+0x24)return 1;
        if(a==p+0xa00)return (m->fault&G528_PARTIAL)&&ch==m->ch?2:0;
        if(a==0xfd7c0000UL+4*(0x294+ch+ch/2))return (m->fault&G528_PARTIAL)&&ch==m->ch?(0x1180U>>(4*(ch&1))):0;
        if(a==(ch<2?0xfd59c000UL:0xfd59d000UL)+0x130+(ch&1)*4)return 0x400;
    }assert(0);return 0;}
int main(void){struct mock m={0};struct cold_io io={&m,rd,0,0};struct cold528_snapshot s;
    assert(cold528_snapshot(&io,&s)==0&&s.faults==0&&m.reads==36);
    for(unsigned ch=0;ch<4;ch++)for(unsigned f=1;f<32;f++){m=(struct mock){f,ch,0};assert(cold528_snapshot(&io,&s)==COLD_BAD_ARG&&s.faults==f&&m.reads==36);}
    assert(cold528_snapshot(0,&s)==COLD_BAD_ARG&&cold528_snapshot(&io,0)==COLD_BAD_ARG);
    puts("cold528 guard: clean candidate + 124 fault combinations PASS");}
