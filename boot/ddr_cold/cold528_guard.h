#ifndef DDR_COLD528_GUARD_H
#define DDR_COLD528_GUARD_H
#include "cold_steps.h"
struct cold528_snapshot { uint32_t mode, sel, reset[4], phy_reset[4], grf[4]; unsigned faults; };
enum { G528_OS=1, G528_STAT=2, G528_IDLE=4, G528_PLL=8, G528_PARTIAL=16 };
/* Reads known cold-state addresses only. No PHY controller access besides
 * CH0 STAT, no writes. Passing does NOT prove power-on state. */
int cold528_snapshot(const struct cold_io *, struct cold528_snapshot *);
#endif
