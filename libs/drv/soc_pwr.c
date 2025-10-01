#include "deviceID.h"
#include "soc_pwr.h"
#include "alif.h"

void enable_syst_sram(uint32_t sram_select)
{
    volatile uint32_t *reg_ptr, reg_data, devID = DeviceID();

    reg_ptr = (uint32_t *) 0x1A602014;  /* CGU CLK_ENA register */
    reg_data = *reg_ptr;

    if (devID == 1) {
        return; /* not applicable for Spark */
    }
    else if (devID == 2) {
        /* SRAM0 clock enable is bit 27 */
        if (sram_select & SYST_SRAM0) {
            reg_data |= (1UL << 27);
        } else {
            reg_data &= ~(1UL << 27);
        }
    }
    else {
        /* SRAM0 clock enable is bit 24 */
        if (sram_select & SYST_SRAM0) {
            reg_data |= (1UL << 24);
        } else {
            reg_data &= ~(1UL << 24);
        }
    }

    /* SRAM1 clock enable is bit 28 */
    if (sram_select & SYST_SRAM1) {
        reg_data |= (1UL << 28);
    } else {
        reg_data &= ~(1UL << 28);
    }

    *reg_ptr = reg_data;

    reg_ptr = (uint32_t *) 0x1A60A004;  /* VBAT PWR_CTRL register */
    reg_data = *reg_ptr;

    /* SRAM0 power mask is bit 8 */
    if (sram_select & SYST_SRAM0) {
        reg_data &= ~(3UL << 8);
    } else {
        reg_data |= (3UL << 8);
    }

    /* SRAM1 power mask is bit 12 */
    if (sram_select & SYST_SRAM1) {
        reg_data &= ~(3UL << 12);
    } else {
        reg_data |= (3UL << 12);
    }

    *reg_ptr = reg_data;
}

void enable_pd1_aon(uint32_t retention_select)
{
    if (DeviceID() != 0) {
        return; /* only applicable for Bolt */
    }

    /* Enable PD1 */
    *((volatile uint32_t *)0x1A60A004) |= 1U;

    /* Enable Main SRAM Retention LDO
    * shared between SE, HE, PD4 SRAMs */
    *((volatile uint32_t *)0x1A60A038) |= (1U << 10);

    /* Enable PD4 SRAM Retention */
    retention_select = ~retention_select;
    *((volatile uint32_t *)0x1A60B000)  = retention_select & 0x3333;

    /* Disconnect PPU from M55-M[4] and M55-G[8]
     * Allows PD4 to turn on and off via functions:
     * enable_pd4_sram() / disable_pd4_sram() */
    *((volatile uint32_t *)0x1A60B008)  = 0x110U;

    /* PD4 PPU HWSTAT Value */
    while((*((volatile uint32_t *)0x1A605058) & 0x7FFUL) == 0);
}

void disable_pd1_aon()
{
    if (DeviceID() != 0) {
        return; /* only applicable for Bolt */
    }

    /* Disable PD1 */
    *((volatile uint32_t *)0x1A60A004) &= ~1U;

    /* Disable PD4 SRAM Retention */
    *((volatile uint32_t *)0x1A60B000)  = 0x3333;

    /* PD4 PPU HWSTAT Value */
    while((*((volatile uint32_t *)0x1A605058) & 0x7FFUL) != 0);
}

void enable_pd4_sram(uint32_t pll_sel)
{
    if (DeviceID() != 0) {
        return; /* only applicable for Bolt */
    }

    /* Switch PD4 between HFXO and PLL-160M clock */
    *((volatile uint32_t *)0x1A605040) = pll_sel ? 3 : 0;
    *((volatile uint32_t *)0x1A60504C) = pll_sel ? 1 : 0;

    /* Enable PD4 */
    *((volatile uint32_t *)0x1A605048) |=  (1U << 12);

    /* PD4 PPU HWSTAT Value */
    while((*((volatile uint32_t *)0x1A605058) & 0x7FFUL) != 0x100);
}

void disable_pd4_sram()
{
    if (DeviceID() != 0) {
        return; /* only applicable for Bolt */
    }

    /* Disable PD4 */
    *((volatile uint32_t *)0x1A605048) &= ~(1U << 12);

    /* PD4 PPU HWSTAT Value */
    while((*((volatile uint32_t *)0x1A605058) & 0x7FFUL) == 0x100);
}
