#ifndef SRAM_APP_ABI_H
#define SRAM_APP_ABI_H
#include <stdint.h>
#define SRAM_APP_MAGIC 0x31504153U /* SAP1, little endian */
#define SRAM_APP_ABI 1U
#define SRAM_APP_ENTRY 16U
#define SRAM_APP_MAX 0xf000U /* final 4 KiB of inactive slot is app stack */
struct sram_app_api {
    uint32_t abi;
    uint32_t size;
    void (*puts)(const char *);
    uint64_t (*ticks)(void);
};
#endif
