#include "cold528_guard.h"
int cold528_snapshot(const struct cold_io *io,struct cold528_snapshot *s){
    if(!io||!io->read||!s)return COLD_BAD_ARG;
    *s=(struct cold528_snapshot){0};
    for(unsigned j=0;j<4;j++)if(io->read(io->ctx,0xfd58a208UL+4*j))s->faults|=G528_OS;
    if(io->read(io->ctx,0xf7010014UL)&7)s->faults|=G528_STAT;
    if(io->read(io->ctx,0xfd8d8110UL))s->faults|=G528_IDLE;
    s->mode=io->read(io->ctx,0xfd7c0280UL);s->sel=io->read(io->ctx,0xfd7c0304UL);
    for(unsigned ch=0;ch<4;ch++){
        uintptr_t p=0xfd800000UL+ch*0x4000UL;
        uint32_t a0=io->read(io->ctx,p),a1=io->read(io->ctx,p+4),b0=io->read(io->ctx,p+0x20),b1=io->read(io->ctx,p+0x24);
        if(a0!=0x8073||a1!=1||b0!=0x8073||b1!=1)s->faults|=G528_PLL;
        s->reset[ch]=io->read(io->ctx,0xfd7c0000UL+4*(0x294+ch+ch/2));
        s->phy_reset[ch]=io->read(io->ctx,p+0xa00);
        s->grf[ch]=io->read(io->ctx,(ch<2?0xfd59c000UL:0xfd59d000UL)+0x130+(ch&1)*4);
        unsigned mask=0x1180U>>(4*(ch&1));
        if((s->reset[ch]&mask)==mask && (s->phy_reset[ch]&0x16)==2 && (s->grf[ch]&0x408)==0x400)s->faults|=G528_PARTIAL;
    }
    return s->faults?COLD_BAD_ARG:COLD_OK;
}
