#include "cold_steps.h"
static int valid(const struct cold_io *io) {
    return io && io->read && io->write && io->delay_us;
}
static uint32_t rd(const struct cold_io *io, uintptr_t a) {
    return io->read(io->ctx,a);
}
static void wr(const struct cold_io *io, uintptr_t a, uint32_t v) {
    io->write(io->ctx,a,v);
}
static int wait_bit(const struct cold_io *io, uintptr_t a, uint32_t mask,
                    int set, unsigned polls) {
    while (polls--) {
        if ((!!(rd(io,a)&mask)) == set) return COLD_OK;
    }
    return COLD_TIMEOUT;
}
int cold_global_handshake(const struct cold_io *io, unsigned polls) {
    const uintptr_t b=0xfd8d8000UL;
    if (!valid(io)||!polls) return COLD_BAD_ARG;
    wr(io,b+0x110,0xf000f000U);
    if (wait_bit(io,b+0x118,0xf0000000U,1,polls)) return COLD_TIMEOUT;
    if (wait_bit(io,b+0x120,0xf0000000U,1,polls)) return COLD_TIMEOUT;
    wr(io,b+0x160,0x00030003U);
    wr(io,b+0x154,0x00030003U);
    if (wait_bit(io,b+0x184,3,1,polls)) return COLD_TIMEOUT;
    wr(io,b+0x154,0x00030000U);
    if (wait_bit(io,b+0x184,3,0,polls)) return COLD_TIMEOUT;
    wr(io,b+0x160,0x00030000U);
    wr(io,b+0x110,0xf0000000U);
    return COLD_OK;
}
int cold_pll_integer_fragment(const struct cold_io *io, unsigned ch,
                              unsigned bank, unsigned m, unsigned p,
                              unsigned s, unsigned polls) {
    if (!valid(io)||ch>=4||bank>=2||!m||m>0x3ff||
        !p||p>0x3f||s>7||!polls) return COLD_BAD_ARG;
    uintptr_t b=0xfd800000UL+ch*0x4000UL+bank*0x20UL;
    wr(io,b+0,0x80008000U);
    wr(io,b+4,0x20002000U);
    wr(io,b+0,0x03ff0000U|m);
    wr(io,b+4,0x01ff0000U|p|(s<<6));
    wr(io,b+8,0xffff0000U);
    wr(io,b+4,0x20000000U);
    io->delay_us(io->ctx,1);
    if (wait_bit(io,b+0x18,0x8000U,1,polls)) return COLD_TIMEOUT;
    wr(io,b+0,0x80000000U);
    return COLD_OK;
}
