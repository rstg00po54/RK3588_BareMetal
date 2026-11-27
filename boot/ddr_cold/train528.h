#ifndef DDR_TRAIN528_H
#define DDR_TRAIN528_H
#include "cold_steps.h"
struct cold_ca_result { uint32_t passing; unsigned base, selected, dfi; };
/* Fixed type8, 528MHz, FSP0, rank selector 1, first probe only.
 * Failure leaves partial state; caller must stop and cold restart. */
int cold_train528_ca(const struct cold_io *,unsigned ch,unsigned polls,struct cold_ca_result *);
#endif
