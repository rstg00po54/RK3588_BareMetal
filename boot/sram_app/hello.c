#include "../sram/app_abi.h"
uint32_t app_main(const struct sram_app_api *api)
{
    if (!api || api->abi != SRAM_APP_ABI || api->size < sizeof(*api))
        return 1;
    api->puts("[APP] Hello from independent SRAM application!\n");
    return 0x12345678U;
}
