#ifndef DDR_COLD_STEPS_H
#define DDR_COLD_STEPS_H
#include <stdint.h>
#include <stddef.h>
struct cold_io {
    void *ctx;
    uint32_t (*read)(void *, uintptr_t);
    void (*write)(void *, uintptr_t, uint32_t);
    void (*delay_us)(void *, unsigned);
};
enum cold_result { COLD_OK=0, COLD_BAD_ARG=-1, COLD_TIMEOUT=-2 };
/* Cold context only. No live DDR reconfiguration. */
int cold_global_handshake(const struct cold_io *io, unsigned polls);
int cold_pll_integer_fragment(const struct cold_io *io, unsigned ch,
                              unsigned bank, unsigned m, unsigned p,
                              unsigned s, unsigned polls);
#endif
