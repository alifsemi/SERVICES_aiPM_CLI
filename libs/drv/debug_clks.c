#include <stdio.h>
#include <stdint.h>
#include <pinconf.h>
#include "sys_clocks.h"
#include "alif.h"

#include "deviceID.h"
#include "debug_clks.h"
#include "soc_clk.h"

static const char *active[2] = {
    "[ ]", "[x]"
};

static const char *gated[2] = {
    "*", ""
};

void DEBUG_frequencies() {
    uint32_t hfrc_top_clock = 76800000;
    uint32_t hfxo_top_clock = 38400000;
    uint32_t sys_osc_clk, periph_osc_clk;
    uint32_t cpupll_clk, syspll_clk, a32_cpuclk, gic_clk, ctrl_clk, dbg_clk;
    uint32_t syst_refclk, syst_aclk, syst_hclk, syst_pclk;
    uint32_t rtss_hp_clk, rtss_he_clk, pd4_clk;

    uint32_t hfrc_div_select = 0;
    uint32_t hfrc_div_active;
    uint32_t hfrc_div_standby;
    uint32_t hfxo_div;
    uint32_t bus_clk_div;
    uint32_t hf_osc_sel;
    uint32_t pll_clk_sel;
    uint32_t es_clk_sel;
    uint32_t clk_ena;
    uint32_t pll_boost;

    uint32_t reg_data;

    reg_data = ANA->VBAT_ANA_REG2;
    hfrc_div_active = (reg_data >> 11) & 7U;
    hfrc_div_standby = (reg_data >> 19) & 7U;

    if (hfrc_div_standby > 6) hfrc_div_standby += 3;        // 2^(7   + 3) = 1024 (75k)
    else if (hfrc_div_standby > 3) hfrc_div_standby += 2;   // 2^(4-6 + 2) = 64-256 (1.2M-300k)
    else if (hfrc_div_standby > 2) hfrc_div_standby += 1;   // 2^(3   + 1) = 16 (4.8M)
                                                            // 2^(0-2 + 0) = 1-4 (76.8M-19.2M)

    if (DeviceID() == 1) {
        reg_data = ANA->DCDC_REG1;
        hfrc_div_select = reg_data & 3U;
    } else {
        reg_data = ANA->DCDC_REG2;
        hfrc_div_select = (reg_data >> 6) & 3U;
    }

    /* value of 0, 1, or 3 means HFRC ACTIVE divider is used */
    if (hfrc_div_select == 2)
        hfrc_div_select = 1;    // HFRC STANDBY is selected
    else
        hfrc_div_select = 0;    // HFRC ACTIVE is selected

    printf("Top Level Clock Sources\r\n");
    printf("HFRC ACTIVE%10dHz%s\r\n", hfrc_top_clock >> hfrc_div_active, active[hfrc_div_select^1U]);
    printf("HFRC STANDBY%9dHz%s\r\n", hfrc_top_clock >> hfrc_div_standby, active[hfrc_div_select]);
    printf("[x] means clock is selected\r\n\n");

    reg_data = AON->RESERVED2[3];  // MISC_REG1
    if (DeviceID() == 1)
        hfxo_div = (reg_data >> 17) & 15U;
    else
        hfxo_div = (reg_data >> 13) & 15U;

    if (hfxo_div > 7) {
        hfxo_div -= 8;
        if (hfxo_div > 6) hfxo_div += 3;            // 2^(7   + 3) = 1024 (75k)
        else if (hfxo_div > 3) hfxo_div += 2;       // 2^(4-6 + 2) = 64-256 (1.2M-300k)
        else if (hfxo_div > 2) hfxo_div += 1;       // 2^(3   + 1) = 16 (4.8M)
                                                    // 2^(0-2 + 0) = 1-4 (76.8M-19.2M)
    }
    hfxo_top_clock >>= hfxo_div;

    reg_data = ANA->MISC_CTRL;
    printf("LFXO CLK%13dHz%s\r\n", 32768, active[reg_data & 1U]);
    reg_data = *((volatile uint32_t *)0x1A605020);  // XO_REG1
    printf("HFXO CLK%13dHz%s\r\n", hfxo_top_clock, active[reg_data & 1U]);

    reg_data = CGU->PLL_LOCK_CTRL;  // PLL_LOCK
    if (DeviceID() == 0)
        printf("PLL  CLK   2400000000Hz%s\r\n", active[reg_data & 1U]);
    else if (DeviceID() == 1)
        printf("PLL  CLK    480000000Hz%s\r\n", active[reg_data & 1U]);
    else if (DeviceID() == 2)
        printf("PLL  CLK    800000000Hz%s\r\n", active[reg_data & 1U]);

    printf("[x] means clock is enabled\r\n\n");

    /* calculate hfrc_top_clock based on standby override bit */
    if (hfrc_div_select) {
        hfrc_top_clock >>= hfrc_div_standby;
    } else {
        hfrc_top_clock >>= hfrc_div_active;
    }

    hf_osc_sel = CGU->OSC_CTRL;
    pll_clk_sel = CGU->PLL_CLK_SEL;
    es_clk_sel = CGU->ESCLK_SEL;
    clk_ena = CGU->CLK_ENA;
    bus_clk_div = AON->SYSTOP_CLK_DIV;

    if (DeviceID() == 1)
        pll_boost = ((*(volatile uint32_t *)(0x1A605028)) >> 19) & 1;

    /* calculate the sys_osc_clk */
    if (hf_osc_sel & 1) {
        if (DeviceID() == 0)
            sys_osc_clk = hfxo_top_clock;
        else
            sys_osc_clk = 76800000;
    }
    else {
        sys_osc_clk = hfrc_top_clock;
    }

    /* calculate the periph_osc_clk */
    if (hf_osc_sel & 16) {
        periph_osc_clk = hfxo_top_clock;
    }
    else {
        periph_osc_clk = hfrc_top_clock >> 1;
    }
    if (DeviceID() == 2)
        printf("HFXO_OUT%13dHz%s\r\n", sys_osc_clk, (clk_ena & (1U << 19)) == 0 ? "*" : "");
    else
        printf("HFXO_OUT%13dHz%s\r\n", sys_osc_clk, (clk_ena & (1U << 18)) == 0 ? "*" : "");
    printf("HFOSC_CLK%12dHz%s\r\n", periph_osc_clk, gated[(clk_ena >> 23) & 1U]);

    /* calculate the REFCLK */
    if (pll_clk_sel & 1) {
        if (DeviceID() == 1) {
            if (pll_boost)
                syst_refclk = 120000000;
            else
                syst_refclk = 80000000;
        }
        else
            syst_refclk = 100000000;
    }
    else {
        syst_refclk = sys_osc_clk;
    }

    /* calculate the CPUPLL_CLK and SYSPLL_CLK */
    if (pll_clk_sel & 16) {
        if (DeviceID() == 1) {
            if (pll_boost)
                syspll_clk = 240000000;
            else
                syspll_clk = 160000000;
        }
        else {
            cpupll_clk = 800000000;
            syspll_clk = 400000000;
        }
    }
    else {
        cpupll_clk = sys_osc_clk;
        syspll_clk = sys_osc_clk;
    }

    if (DeviceID() != 1)
        printf("CPUPLL_CLK%11dHz%s\r\n", cpupll_clk, gated[(clk_ena >> 4) & 1U]);
    printf("SYSPLL_CLK%11dHz%s\r\n", syspll_clk, gated[clk_ena & 1U]);

    /* calculate the APSS_CLK */
    /* HOSTCPUCLK_CTRL value at [7:0] and
     * HOSTCPUCLK_DIV0 value at [15:8] and
     * HOSTCPUCLK_DIV1 value at [23:16] */
    uint32_t apssclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010800));
    apssclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010804));
    apssclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    reg_data = *((volatile uint32_t *)(0x1A010808));
    apssclk_status |= ((reg_data >> 16) & 0xFF) << 16;
    if ((apssclk_status & 0xF) == 4) {
        a32_cpuclk = cpupll_clk;
        a32_cpuclk /= ((apssclk_status >> 16) & 0xFF) + 1;   // apssclk divider is (n + 1)
    }
    else if ((apssclk_status & 0xF) == 2) {
        a32_cpuclk = syspll_clk;
        a32_cpuclk /= ((apssclk_status >> 8) & 0xFF) + 1;    // apssclk divider is (n + 1)
    }
    else if ((apssclk_status & 0xF) == 1) {
        a32_cpuclk = syst_refclk;
    }
    else {
        a32_cpuclk = 0;
    }
    if (DeviceID() != 1)
        printf("A32_CPUCLK%11dHz%s\r\n", a32_cpuclk, (apssclk_status & 0xF) == 0 ? gated[0] : gated[1]);

    /* calculate the GIC_CLK */
    /* GICCLK_CTRL value at [7:0] and
     * GICCLK_DIV0 value at [15:8] */
    uint32_t gicclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010810));
    gicclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010814));
    gicclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    if ((gicclk_status & 0xF) == 2) {
        gic_clk = syspll_clk;
        gic_clk /= ((gicclk_status >> 8) & 0xFF) + 1;        // gicclk divider is (n + 1)
    }
    else if ((gicclk_status & 0xF) == 1) {
        gic_clk = syst_refclk;
    }
    else {
        gic_clk = 0;
    }
    if (DeviceID() != 1)
        printf("GIC_CLK%14dHz%s\r\n", gic_clk, (gicclk_status & 0xF) == 0 ? gated[0] : gated[1]);

    /* calculate the SYST_ACLK, HCLK, PCLK */
    /* ACLK_CTRL value at [7:0] and
     * ACLK_DIV0 value at [15:8] */
    uint32_t aclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010820));
    aclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010824));
    aclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    if ((aclk_status & 0xF) == 2) {
        syst_aclk = syspll_clk;
        syst_aclk /= ((aclk_status >> 8) & 0xFF) + 1;        // aclk divider is (n + 1)
    }
    else if ((aclk_status & 0xF) == 1) {
        syst_aclk = syst_refclk;
    }
    else {
        syst_aclk = 0;
    }
    printf("SYST_ACLK%12dHz%s\r\n", syst_aclk, (aclk_status & 0xF) == 0 ? gated[0] : gated[1]);
    if (DeviceID() == 0) {
        printf(" SRAM0_CLK%11dHz%s\r\n", syst_aclk, gated[(clk_ena >> 24) & 1U]);
        printf(" SRAM1_CLK%11dHz%s\r\n", syst_aclk, gated[(clk_ena >> 28) & 1U]);
    }
    if (DeviceID() == 2) {
        printf(" SRAM0_CLK%11dHz%s\r\n", syst_aclk, gated[(clk_ena >> 27) & 1U]);
        printf(" SRAM1_CLK%11dHz%s\r\n", syst_aclk, gated[(clk_ena >> 28) & 1U]);
    }

    if (DeviceID() == 0) {
        /* HCLK and PCLK divider is 2^n, but n = 0-2 only */
        reg_data = (bus_clk_div >> 8) & 3U; if (reg_data == 3) reg_data = 2;
        syst_hclk = syst_aclk >> reg_data;

        reg_data = bus_clk_div & 3U; if (reg_data == 3) reg_data = 2;
        syst_pclk = syst_aclk >> reg_data;
    }
    else {
        /* HCLK and PCLK divider is 2^n, but n = 0-2 only */
        reg_data = (bus_clk_div >> 8) & 3U; if (reg_data == 3) reg_data = 2;
        syst_hclk = syspll_clk >> reg_data;

        reg_data = bus_clk_div & 3U; if (reg_data == 3) reg_data = 2;
        syst_pclk = syspll_clk >> reg_data;
    }

    printf(" SYST_HCLK%11dHz\r\n", syst_hclk);
    printf(" SYST_PCLK%11dHz\r\n", syst_pclk);

    /* calculate the CTRL_CLK */
    /* CTRLCLK_CTRL value at [7:0] and
     * CTRLCLK_DIV0 value at [15:8] */
    uint32_t ctrlclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010830));
    ctrlclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010834));
    ctrlclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    if ((ctrlclk_status & 0xF) == 2) {
        ctrl_clk = syspll_clk;
        ctrl_clk /= ((ctrlclk_status >> 8) & 0xFF) + 1;      // ctrlclk divider is (n + 1)
    }
    else if ((ctrlclk_status & 0xF) == 1) {
        ctrl_clk = syst_refclk;
    }
    else {
        ctrl_clk = 0;
    }
    printf("CTRL_CLK%13dHz%s\r\n", ctrl_clk, (ctrlclk_status & 0xF) == 0 ? gated[0] : gated[1]);

    /* calculate the DBG_CLK */
    /* DBGCLK_CTRL value at [7:0] and
     * DBGCLK_DIV0 value at [15:8] */
    uint32_t dbgclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010840));
    dbgclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010844));
    dbgclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    if ((dbgclk_status & 0xF) == 2) {
        dbg_clk = syspll_clk;
        dbg_clk /= ((dbgclk_status >> 8) & 0xFF) + 1;        // dbgclk divider is (n + 1)
    }
    else if ((dbgclk_status & 0xF) == 1) {
        dbg_clk = syst_refclk;
    }
    else {
        dbg_clk = 0;
    }
    printf("DBG_CLK%14dHz%s\r\n", dbg_clk, (dbgclk_status & 0xF) == 0 ? gated[0] : gated[1]);
    printf("SYST_REFCLK%10dHz\r\n", syst_refclk);

    if (DeviceID() != 1) {
        /* calculate the RTSS_HP_CLK */
        const uint32_t pll_rtss_hp_clocks[4] = {100000000UL, 200000000UL, 300000000UL, 400000000UL};
        if (pll_clk_sel & (1U << 16)) {
            rtss_hp_clk = pll_rtss_hp_clocks[es_clk_sel & 3U];
        }
        else {
            switch((es_clk_sel >> 8) & 3U) {
            case 0:
                rtss_hp_clk = hfrc_top_clock;
                break;
            case 1:
                rtss_hp_clk = hfrc_top_clock >> 1;
                break;
            case 2:
                rtss_hp_clk = 76800000;
                break;
            case 3:
                rtss_hp_clk = hfxo_top_clock;
                break;
            }
        }
        printf("RTSS_HP_CLK%10dHz%s\r\n", rtss_hp_clk, gated[(clk_ena >> 12) & 1U]);
    }

    /* calculate the RTSS_HE_CLK */
    const uint32_t pll_rtss_he_clocks[4] = {60000000UL, 80000000UL, 120000000UL, 160000000UL};
    if (pll_clk_sel & (1U << 20)) {
        rtss_he_clk = pll_rtss_he_clocks[(es_clk_sel >> 4) & 3U];
    }
    else {
        switch((es_clk_sel >> 12) & 3U) {
        case 0:
            rtss_he_clk = hfrc_top_clock;
            break;
        case 1:
            rtss_he_clk = hfrc_top_clock >> 1;
            break;
        case 2:
            rtss_he_clk = 76800000;
            break;
        case 3:
            rtss_he_clk = hfxo_top_clock;
            break;
        }
    }
    printf("RTSS_HE_CLK%10dHz%s\r\n", rtss_he_clk, gated[(clk_ena >> 13) & 1U]);

    if (DeviceID() == 0) {
        uint32_t pll_pd4_clocks[4] = {hfxo_top_clock, 0, 120000000, 160000000};

        /* calculate PD4 SRAM6 to SRAM9 clocks */
        if (*((volatile uint32_t *)0x1A60504C) & 1U) {
            pd4_clk = pll_pd4_clocks[*((volatile uint32_t *)0x1A605040) & 3U];
        }
        else {
            pd4_clk = pll_pd4_clocks[0];
        }

        /* check if PD4 is alive */
        reg_data = *((volatile uint32_t *)0x1A605058);
        reg_data &= 0x7FFUL;
        printf("PD4_CLK%14dHz%s\r\n", pd4_clk, reg_data > 1 ? gated[1] : gated[0]);
    }

    printf("160M_CLK%13dHz%s\r\n", 160000000, gated[(clk_ena >> 20) & 1U]);
    if (DeviceID() == 0)
        printf("100M_CLK%13dHz%s\r\n", 100000000, gated[(clk_ena >> 21) & 1U]);
    printf("20M/10M_CLK%10dHz%s\r\n", 20000000, gated[(clk_ena >> 22) & 1U]);
    printf("* means clock is gated\r\n\n");
}

void DEBUG_peripherals() {
    printf("EXPMST0_CTRL       = 0x%08X\r\n", CLKCTL_PER_SLV->EXPMST0_CTRL);
    printf("UART_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->UART_CTRL);
    printf("CANFD_CTRL         = 0x%08X\r\n", CLKCTL_PER_SLV->CANFD_CTRL);
    printf("I2S0_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->I2S_CTRL[0]);
    printf("I2S1_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->I2S_CTRL[1]);
    if (DeviceID() != 1) {
        printf("I2S2_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->I2S_CTRL[2]);
        printf("I2S3_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->I2S_CTRL[3]);
    }
    printf("I3C_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->I3C_CTRL);
    printf("SSI_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->SSI_CTRL);
    printf("ADC_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->ADC_CTRL);
    printf("DAC_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->DAC_CTRL);
    printf("CMP_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->CMP_CTRL);
    printf("CAMERA_PIXCLK_CTRL = 0x%08X\r\n", CLKCTL_PER_MST->CAMERA_PIXCLK_CTRL);
    if (DeviceID() != 1) {
        printf("CDC200_PIXCLK_CTRL = 0x%08X\r\n", CLKCTL_PER_MST->CDC200_PIXCLK_CTRL);
        printf("CSI_PIXCLK_CTRL    = 0x%08X\r\n", CLKCTL_PER_MST->CSI_PIXCLK_CTRL);
    }
    printf("PERIPH_CLK_ENA     = 0x%08X\r\n", CLKCTL_PER_MST->PERIPH_CLK_ENA);
    printf("MIPI_CKEN          = 0x%08X\r\n", CLKCTL_PER_MST->MIPI_CKEN);
    if (DeviceID() != 1) {
        printf("ETH_CTRL0          = 0x%08X\r\n", CLKCTL_PER_MST->ETH_CTRL0);
        printf("MRAM_CTRL/OSPI_CLK = 0x%08X\r\n", *(volatile uint32_t *)0x49041000);
    }
    printf("M55LOCAL_CLK_ENA   = 0x%08X\r\n\n", M55LOCAL_CFG->CLK_ENA);
    if (CoreID()) {
        printf("M55HE_I2S_CTRL  = 0x%08X\r\n\n", M55HE_CFG->HE_I2S_CTRL);
    }
}

void DEBUG_clks_xvclks() {
    if (DeviceID() == 1) {
        pinconf_set(PORT_2, PIN_3, PINMUX_ALTERNATE_FUNCTION_5, 0);     // P2_3:  LPCAM_XVCLK (mux mode 5)
        M55HE_CFG->HE_CAMERA_PIXCLK = 100U << 16 | 1;           // output RTSS_HE_CLK/100 on LPCAM_XVCLK pin
    }
    else {
        pinconf_set(PORT_0, PIN_3, PINMUX_ALTERNATE_FUNCTION_6, 0);     // P0_3:  CAM_XVCLK   (mux mode 6)
        pinconf_set(PORT_1, PIN_3, PINMUX_ALTERNATE_FUNCTION_5, 0);     // P1_3:  LPCAM_XVCLK (mux mode 5)
        CLKCTL_PER_MST->CAMERA_PIXCLK_CTRL = 100U << 16 | 1;    // output SYST_ACLK/100 on CAM_XVCLK pin
        M55HE_CFG->HE_CAMERA_PIXCLK = 100U << 16 | 1;           // output RTSS_HE_CLK/100 on LPCAM_XVCLK pin
    }
}
