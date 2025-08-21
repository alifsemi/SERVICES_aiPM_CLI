#include "soc.h"
#include "app_mem_regions.h"

#include "hostbase.h"
#include "soc_rst.h"

#if RTSS_BIN || MDM_BIN
#include <string.h>
#endif

void reset_external_system0(uint32_t vtor_address)
{
    *((volatile uint32_t *)0x1A604000) = 0;
    *((volatile uint32_t *)0x1A605010) = vtor_address;
    *((volatile uint32_t *)0x1A605014) = vtor_address;

    /* apply reset & M55 CPUWAIT */
    HOSTBASE->EXT_SYS0_RST_CTRL = 3;

    /* wait for reset acknowledge */
    while (HOSTBASE->EXT_SYS0_RST_ST != 4);

    /* release reset, but keep M55 CPUWAIT */
    HOSTBASE->EXT_SYS0_RST_CTRL = 1;
}

void reset_external_system1(uint32_t vtor_address)
{
    *((volatile uint32_t *)0x1A604010) = 0;
    *((volatile uint32_t *)0x1A60A020) = vtor_address;
    *((volatile uint32_t *)0x1A60A024) = vtor_address;

    /* apply reset & M55 CPUWAIT */
    HOSTBASE->EXT_SYS1_RST_CTRL = 3;

    /* wait for reset acknowledge */
    while (HOSTBASE->EXT_SYS1_RST_ST != 4);

    /* release reset, but keep M55 CPUWAIT */
    HOSTBASE->EXT_SYS1_RST_CTRL = 1;
}

void release_external_system0()
{
    HOSTBASE->EXT_SYS0_RST_CTRL = 0;

#if SOC_FEAT_HAS_BULK_SRAM
    /* dummy read to force clocks to start */
    (void) *((volatile uint32_t *) APP_SRAM2_BASE);
#endif
}

void release_external_system1()
{
    HOSTBASE->EXT_SYS1_RST_CTRL = 0;

    /* dummy read to force clocks to start */
    (void) *((volatile uint32_t *) APP_SRAM4_BASE);
}

#if RTSS_BIN
void sideload_external_system0() {
    //uint32_t *rtsshp_loadaddr = (uint32_t *) SRAM0_BASE;
    //uint32_t *rtsshp_loadaddr = (uint32_t *) SRAM1_BASE;
    uint32_t *rtsshp_loadaddr = (uint32_t *) SRAM2_BASE;

    /* dummy read to force clocks to start */
    (void) *((volatile uint32_t *) SRAM2_BASE);
    memcpy(rtsshp_loadaddr, rtsshp_bin, rtsshp_bin_size);
}

void sideload_external_system1() {
    //uint32_t *rtsshe_loadaddr = (uint32_t *) SRAM0_BASE;
    //uint32_t *rtsshe_loadaddr = (uint32_t *) SRAM1_BASE;
    uint32_t *rtsshe_loadaddr = (uint32_t *) SRAM4_BASE;

    /* dummy read to force clocks to start */
    (void) *((volatile uint32_t *) SRAM4_BASE);
    memcpy(rtsshe_loadaddr, rtsshe_bin, rtsshe_bin_size);
}
#endif

