#ifndef DDR_PROBE528_H
#define DDR_PROBE528_H
#include "first528.h"
#include "train528.h"
#include "train528_mode8.h"
#include "lp4x_geometry.h"
enum probe528_stage { P528_NONE, P528_FIRST, P528_CA, P528_MR14_CHECK,
    P528_MR14_WRITE, P528_MODE8, P528_PULSE, P528_MR8, P528_RANK_COMPARE,
    P528_RANK_MR8, P528_RANK_MR0, P528_GEOMETRY, P528_DONE };
struct probe528_channel {
    struct cold_ca_result ca;
    struct cold_mode8_result mode8;
    struct lp4x_probe mr;
    struct lp4x_geo geometry;
    unsigned compared;
};
struct probe528_result {
    struct first528_result first;
    enum probe528_stage stage;
    unsigned ch, candidate_mask;
    int rc;
    struct probe528_channel channel[4];
};
/* Host-only first cold initialization plus geometry candidates. No memory
 * access or second pass. A nonzero return is terminal until cold reset. */
int cold_probe528(const struct cold_io *,unsigned,struct probe528_result *);
#endif
