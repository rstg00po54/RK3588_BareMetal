#ifndef DDR_TRAIN528_MODE8_H
#define DDR_TRAIN528_MODE8_H
#include "cold_steps.h"
struct cold_mode8_result {
    uint32_t raw[10], fine[10], coarse_lo, coarse_hi;
    unsigned period;
};
/* First type8 528 MHz training, bank0, rank index0. Failure is terminal. */
int cold_train528_mode8(const struct cold_io *,unsigned,unsigned,struct cold_mode8_result *);
#endif
