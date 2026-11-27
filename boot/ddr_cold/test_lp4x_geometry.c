#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "lp4x_geometry.c"
int main(void){
    struct lp4x_probe p={.mr8_rank0=0x10,.mr8_rank1=0x10,.mr_rank0={0x5c,1,0x5c},.mr_rank1={0,1,0x5c}};
    struct lp4x_geo g;
    assert(lp4x_geometry528(&p,&g)==0 && g.g[0]==1 && g.g[6]==16 && g.g[7]==0 && g.g[4]==1);
    p.mr_rank1[0]=0x5c;assert(lp4x_geometry528(&p,&g)==0 && g.g[0]==2 && g.g[7]==16);
    p.mr8_rank1=0x50;assert(lp4x_geometry528(&p,&g)==-2);
    p.mr0_rank1=2;assert(lp4x_geometry528(&p,&g)==0 && g.g[4]==0 && g.g[7]==17);
    /* All raw MR8 bytes. These are formula tests, not JEDEC validity checks. */
    for(unsigned a=0;a<256;a++)for(unsigned b=0;b<256;b++){
        p.mr8_rank0=a;p.mr8_rank1=b;p.mr0_rank1=2;
        assert(lp4x_geometry528(&p,&g)==0);
        assert(g.g[0]==2 && g.g[1]==10 && g.g[2]==3 && g.g[3]==1);
        assert(g.g[6]==14+(((a&60)>>2)+1)/2+(a>>6==1));
        assert(g.g[7]==14+(((b&60)>>2)+1)/2+(b>>6==1));
        assert(g.g[4]==((a>>6!=1)&&(b>>6!=1)) && g.g[5]==((a>>2)&1));
        assert(g.g[10]==g.g[6]&&g.g[11]==g.g[7]);
        p.mr0_rank1=0;int mismatch=(a>>6==1)!=(b>>6==1);
        assert(lp4x_geometry528(&p,&g)==(mismatch?-2:0));
    }
    puts("LPDDR4X geometry: 65536 MR8 pairs, width consistency and rank candidate tests PASS");
}
