#ifndef DDR_FIRST528_H
#define DDR_FIRST528_H
#include "cold_steps.h"
enum first528_stage { F528_NONE, F528_CLOCKS, F528_RESET, F528_CTL,
    F528_ELECTRICAL, F528_MAP, F528_RELEASE, F528_TIMING, F528_DFI_START,
    F528_DFI_COMPLETE, F528_NORMAL, F528_MR, F528_ZQ, F528_PHY_ACK,
    F528_SWITCH, F528_RESTORE, F528_PHY_READY, F528_DONE };
struct first528_result { enum first528_stage stage; unsigned ch; int rc; };
/* Fixed four-channel type8 first-pass 89dc. Host-only; no physical backend.
 * Does not include main's earlier PMU preflight, training or DRAM access. */
int cold_first528(const struct cold_io *,unsigned,struct first528_result *);
#endif
