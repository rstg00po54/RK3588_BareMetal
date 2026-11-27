/* v1.18 a080..a0f4, a120..a1f0, a734..a84c.
 * Pure data decoder. No MMIO, no DRAM access. Inputs must come from
 * successful MRR transactions. These are vendor structure fields. */
#include <stdint.h>
#include <stddef.h>
struct lp4x_probe {
    uint8_t mr8_rank0, mr8_rank1, mr0_rank1;
    uint8_t mr_rank0[3], mr_rank1[3]; /* MR12, MR5, MR14 */
};
struct lp4x_geo { uint32_t g[0x6c/4]; };
static unsigned rows(uint8_t mr8){return 14+((((mr8>>2)&15)+1)>>1);}
int lp4x_geometry528(const struct lp4x_probe *p,struct lp4x_geo *out){
    if(!p||!out)return -1;
    struct lp4x_geo r={0};
    unsigned w0=p->mr8_rank0>>6,w1=p->mr8_rank1>>6;
    r.g[0]=1;r.g[1]=10;r.g[2]=3;r.g[3]=1;
    r.g[4]=w0==1?0:1;r.g[5]=(p->mr8_rank0>>2)&1;
    r.g[6]=rows(p->mr8_rank0)+(w0==1);r.g[10]=r.g[6];
    for(unsigned i=0;i<3;i++)if(p->mr_rank0[i]!=p->mr_rank1[i]){*out=r;return 0;}
    /* Matching MR bytes are only vendor's rank candidate, not proof. */
    if((w0==1)!=(w1==1) && !(p->mr0_rank1&2))return -2;
    r.g[0]=2;r.g[7]=rows(p->mr8_rank1)+(w1==1);
    if(w1==1)r.g[4]=0;
    r.g[11]=r.g[7];*out=r;return 0;
}
