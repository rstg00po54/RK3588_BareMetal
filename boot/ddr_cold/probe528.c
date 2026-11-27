/* First 9e48..a218 probe sequence after fixed 89dc. Host-injected IO only. */
#include "probe528.h"
int cold_mrr_fragment(const struct cold_io *,unsigned,unsigned,unsigned,unsigned,uint8_t *);
int cold_mrw_fragment(const struct cold_io *,unsigned,unsigned,unsigned,unsigned,unsigned);
static uint32_t rd(const struct cold_io *i,uintptr_t a){return i->read(i->ctx,a);}
static void wr(const struct cold_io *i,uintptr_t a,uint32_t v){i->write(i->ctx,a,v);}
static void rmw(const struct cold_io *i,uintptr_t a,uint32_t keep,uint32_t bits){wr(i,a,(rd(i,a)&keep)|bits);}
#define TRY(expr) do{int rc=(expr);if(rc){out->rc=rc;return rc;}}while(0)
int cold_probe528(const struct cold_io *i,unsigned polls,struct probe528_result *out){
    if(!out)return COLD_BAD_ARG;
    *out=(struct probe528_result){.stage=P528_NONE,.ch=4,.rc=COLD_BAD_ARG};
    if(!i||!i->read||!i->write||!i->delay_us||!polls)return COLD_BAD_ARG;
    out->stage=P528_FIRST;TRY(cold_first528(i,polls,&out->first));
    for(unsigned ch=0;ch<4;ch++){
        out->ch=ch;struct probe528_channel *r=&out->channel[ch];
        uintptr_t b=0xf7000000UL+ch*0x1000000UL,c=b+0x10000,p=0xfe0c0000UL+ch*0x10000UL;
        out->stage=P528_CA;TRY(cold_train528_ca(i,ch,polls,&r->ca));
        if(!ch){uint8_t v;out->stage=P528_MR14_CHECK;TRY(cold_mrr_fragment(i,ch,1,14,polls,&v));
            if(v==0x5d){out->rc=-5;return -5;}}
        out->stage=P528_MR14_WRITE;TRY(cold_mrw_fragment(i,ch,3,14,rd(i,b+0x50c)&255,polls));
        out->stage=P528_MODE8;TRY(cold_train528_mode8(i,ch,polls,&r->mode8));
        out->stage=P528_PULSE;rmw(i,p+4,~0U,0x20);rmw(i,p+0x10,~0U,0x200);
        rmw(i,p+0x50,~0U,0x80000);i->delay_us(i->ctx,1);rmw(i,p+0x50,0xfff7ffff,0);rmw(i,p+0x10,0xfffffdff,0);
        out->stage=P528_MR8;TRY(cold_mrr_fragment(i,ch,1,8,polls,&r->mr.mr8_rank0));
        uint32_t saved=rd(i,c+0x180);wr(i,c+0x180,0);
        static const unsigned mrs[]={12,5,14};unsigned same=1;
        out->stage=P528_RANK_COMPARE;
        for(unsigned j=0;j<3;j++){
            TRY(cold_mrr_fragment(i,ch,1,mrs[j],polls,&r->mr.mr_rank0[j]));
            TRY(cold_mrr_fragment(i,ch,2,mrs[j],polls,&r->mr.mr_rank1[j]));
            r->compared=j+1;
            if(r->mr.mr_rank0[j]!=r->mr.mr_rank1[j]){same=0;break;}
        }
        if(same){out->stage=P528_RANK_MR8;TRY(cold_mrr_fragment(i,ch,2,8,polls,&r->mr.mr8_rank1));
            if(((r->mr.mr8_rank0>>6)==1)!=((r->mr.mr8_rank1>>6)==1)){
                out->stage=P528_RANK_MR0;TRY(cold_mrr_fragment(i,ch,2,0,polls,&r->mr.mr0_rank1));
            }}
        out->stage=P528_GEOMETRY;
        if(lp4x_geometry528(&r->mr,&r->geometry)){out->rc=-6;return -6;}
        wr(i,c+0x180,saved);out->candidate_mask|=1U<<ch;
    }
    out->stage=P528_DONE;out->ch=4;out->rc=0;return 0;
}
