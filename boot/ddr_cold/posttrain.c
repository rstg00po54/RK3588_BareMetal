/* Fixed LPDDR4/4X path of v1.18 0xb98c. Pure calculation; no MMIO.
 * Caller selects the active timing bank using DDRCTL MSTR2[1:0]. */
#include <stdint.h>
int lp4x_posttrain_grf(uint32_t tmg2, uint32_t dfitmg0, uint32_t *out)
{
    if (!out) return -1;
    unsigned a=(tmg2>>16)&127, b=(tmg2>>24)&127;
    unsigned c=(dfitmg0>>16)&63;
    if (a<7 || b<2) return -1; /* reject underflow, outside tested use */
    unsigned q=b/2, v=c/2, u=(b-2)/2;
    if (u>v) u=v;
    unsigned e1=q<3?3-q:0, e2=u<3?3-u:0;
    unsigned total=(e1>e2?e1:e2)+v, sub=(a-7)/2+4;
    *out=total>sub?total-sub:0;
    return 0;
}
