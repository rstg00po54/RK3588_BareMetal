#include <stdint.h>
#include <assert.h>
#include <stdio.h>
int cold_ca_select(unsigned,uint32_t,unsigned *,unsigned *);
#define BIT(x) (UINT32_C(1)<<(x))
static void test(unsigned b,uint32_t pass,unsigned expected,unsigned t)
{
    unsigned s=99,d=99;
    assert(cold_ca_select(b,pass,&s,&d)==0&&s==expected&&d==t);
}
int main(void){
    test(10,BIT(6)|BIT(8)|BIT(10)|BIT(12)|BIT(14),8,15);
    test(10,0x55554,4,11);
    test(10,BIT(8),8,15);
    test(10,BIT(8)|BIT(10)|BIT(12)|BIT(14),10,17);
    test(9,BIT(1)|BIT(3)|BIT(5),3,9);
    unsigned s=99,d=99;
    assert(cold_ca_select(10,0,&s,&d)==-2&&s==99&&d==99);
    assert(cold_ca_select(2,BIT(2),&s,&d)==-1);
    /* Vendor uses extrema, not longest contiguous eye. Gaps are not
       rejected by this pure reference. A physical driver must recheck. */
    test(10,BIT(2)|BIT(18),4,11);
    for(unsigned b=9;b<=10;b++)for(unsigned mask=0;mask<512;mask++){
        uint32_t pass=0;unsigned lo=99,hi=0;
        for(unsigned k=0;k<9;k++)if(mask&(1U<<k)){
            unsigned v=(b&1?1:2)+2*k;pass|=BIT(v);if(v<lo)lo=v;hi=v;
        }
        int rc=cold_ca_select(b,pass,&s,&d);
        if(!mask)assert(rc==-2);else {unsigned e=lo==hi?lo:lo+2;assert(rc==0&&s==e&&d==b-3+e);}
    }
    puts("CA scan selection: explicit vectors + 1024 window masks PASS");
}
