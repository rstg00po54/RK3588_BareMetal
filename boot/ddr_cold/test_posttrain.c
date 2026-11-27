#include <stdint.h>
#include <assert.h>
#include <stdio.h>
int lp4x_posttrain_grf(uint32_t,uint32_t,uint32_t *);
int main(void)
{
    uint32_t v=0;
    assert(lp4x_posttrain_grf(0x060a1817,0x07190204,&v)==0 && v==8);
    assert(lp4x_posttrain_grf(0x060a1817,0x07110204,&v)==0 && v==4);
    assert(lp4x_posttrain_grf(0,0,&v)==-1);
    assert(lp4x_posttrain_grf(0x060a1817,0,0)==-1);
    /* Compare this output against independent instruction interpreter. */
    for(unsigned a=7;a<32;a++)for(unsigned b=2;b<24;b++)for(unsigned d=0;d<32;d++){
        assert(lp4x_posttrain_grf((a<<16)|(b<<24),d<<16,&v)==0);
        printf("%u\n",v);
    }
    return 0;
}
