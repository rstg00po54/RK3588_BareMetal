#ifndef DDR_LP4X_GEOMETRY_H
#define DDR_LP4X_GEOMETRY_H
#include <stdint.h>
struct lp4x_probe {
    uint8_t mr8_rank0, mr8_rank1, mr0_rank1;
    uint8_t mr_rank0[3], mr_rank1[3]; /* MR12, MR5, MR14 */
};
struct lp4x_geo { uint32_t g[0x6c/4]; };
int lp4x_geometry528(const struct lp4x_probe *,struct lp4x_geo *);
#endif
