#include <stdio.h>
#include <alif.h>
#include "app_mem_regions.h"

#include "SERVICES_aiPM_print.h"

static const char star_char = '*';
static const char star_bar[]      = "*************************************************************";
static const char star_domains[]  = "* PD0 * PD1 * PD2 * PD3 * PD4 * PD5 * PD6 * PD7 * PD8 * PD9 *";
static const char star_memories[] = "* SRAM0 * SRAM1 * HETCM1 * HETCM2 * MRAM * SERAM * BKUP4K * MASK     *";
static const char star_clocks[]   = "* AON CLK * RUN CLK * SCALED CLK FREQ    * CPU FREQ *";
static const char * star_ip_clks[]  = {
    "* HP NPU * HE NPU * OSPI1 * CAN * SDC * USB * ETH *",
    "* GPU * DPI * CPI * DSI * CSI * LP- * PHY PWR Mask *"
};
static const char star_dcdc[]     = "* DCDC      * Mode *";
static const char star_wake[]     = "* VBAT    * SE EWIC * VTOR       *";
static const char * str_pd_stat[] = { " OFF ", " ON  " };
static const char * str_aon[] =  { "LFRC", "LFXO" };
static const char * str_run[] =  { "HFRC", "HFXO", "PLL " };
static const char * str_mode[] =  { "OFF    ", "AUTOPFM", "PFM    ", "PWM   " };
static const char * str_voltage[] =  { "VOUT_ERR ", "VOUT_0800", "VOUT_0825", "VOUT_0850" };
static const char * str_scaled_clks[] = {
  "RC ACTIVE 76.8 MHZ", "RC ACTIVE 38.4 MHZ", "RC ACTIVE 19.2 MHZ", "RC ACTIVE 9.6 MHZ ",
  "RC ACTIVE 4.8 MHZ ", "RC ACTIVE 2.4 MHZ ", "RC ACTIVE 1.2 MHZ ", "RC ACTIVE 600 KHZ ",

  "RC STDBY 76.8 MHZ ", "RC STDBY 38.4 MHZ ", "RC STDBY 19.2 MHZ ", "RC STDBY 4.8 MHZ  ",
  "RC STDBY 1.2 MHZ  ", "RC STDBY 600 KHZ  ", "RC STDBY 300 KHZ  ", "RC STDBY 75 KHZ   ",

  "XO LO DIV 38.4 MHZ", "XO LO DIV 19.2 MHZ", "XO LO DIV 9.6 MHZ ", "XO LO DIV 4.8 MHZ ",
  "XO LO DIV 2.4 MHZ ", "XO LO DIV 1.2 MHZ ", "XO LO DIV 600 KHZ ", "XO LO DIV 300 KHZ ",

  "XO HI DIV 38.4 MHZ", "XO HI DIV 19.2 MHZ", "XO HI DIV 9.6 MHZ ", "XO HI DIV 2.4 MHZ ",
  "XO HI DIV 600 KHZ ", "XO HI DIV 300 KHZ ", "XO HI DIV 150 KHZ ", "XO HI DIV 37.5 KHZ",
  "SCALED_FREQ_NONE  "
};
static const char * str_cpu_clks[] = {
  "800MHZ", "400MHZ", "300MHZ", "200MHZ", "160MHZ", "120MHZ",
  "80MHZ ", "60MHZ ", "100MHZ", "50MHZ ", "20MHZ ", "10MHZ ",
  "HFRC  ", "HFRC/2", "HFXOx2", "HFXO  ",
  "ERROR ", "NONE  "
};

static const char * str_user_menu[] = {
  " 1: AIPM set/get run config", " 2: AIPM set/get off config", " 3: DEBUG clock and power",
  " 4: read/write register", " 5: CPU exercises", " 6: NPU exercises",
  " 7: SERVICES boot testing", " 8: SE Other Services", " 9: PMU AMUX config"
};

static const char * str_user_submenu[] = {
  " 1: set_run_cfg()\r\n 2: get_run_cfg()",
  " 1: set_off_cfg()\r\n 2: get_off_cfg()",
  " 1: DEBUG_frequencies()\r\n 2: DEBUG_peripherals()\r\n 3: DEBUG_power()\r\n 4: Disable OSC\r\n 5: Enable OSC\r\n 6: Disable PLL\r\n 7: Enable PLL",
  " 1: read register\r\n 2: write register\r\n 3: Enable XVCLKs\r\n 4: Toggle DCDC Mode\r\n 5: Toggle MRAM LDO\r\n 6: Toggle SYSTOP\r\n 7: Toggle DBGTOP\r\n 8: Force STOP Mode\r\n 9: exit",
  " 1: M55: Coremark 10s\r\n 2: M55: while(1) 10s\r\n 3: M55: light sleep 10s\r\n 4: M55: deep sleep 10s\r\n 5: M55 subsystem off\r\n 6: NVIC_SystemReset()\r\n 7: WAKE other core via MHU",
  " 1: U55: Typical 10s\r\n 2: U85: Typical 10s\r\n 3: U55: MicroNet Medium 100 iterations (DTCM)\r\n 4: U55: MicroNet Medium 100 iterations (SRAM)\r\n 5: U55: MicroNet Medium 100 iterations (MRAM)\r\n 6: U85: MicroNet Medium 100 iterations (SRAM)\r\n 7: U85: MicroNet Medium 100 iterations (MRAM)\r\n 8: U85: MicroNet Medium 100 iterations (HyperRAM)",
  " 1: boot_process_toc_entry()\r\n 2: boot_cpu()\r\n 3: boot_reset_cpu()\r\n 4: boot_release_cpu()\r\n 5: boot_reset_soc()",
  " 1: SE Clock Source to HFRC\r\n 2: SE Subsystem Off Request\r\n 3: MCU Stop Mode Request via SE"
};

static const char * str_cfg_main_menu[] = {
  " 1: set power_domains", " 2: set memory_blocks", " 3: set aon clk",
  " 4: set run clk", " 5: set scaled clk", " 6: set cpu clk",
  " 7: set ip clk gating", " 8: set dcdc vout", " 9: set dcdc mode",
  " a: apply changes", " d: discard changes"
};

static const char * str_cfg_sub_menu[] = {
  "enter power_domains mask (hex)", "enter memory_blocks mask (hex)", "enter aon clk enum (hex)",
  "enter run clk enum (hex)", "enter scaled clk enum (hex)", "enter cpu clk enum (hex)",
  "enter ip clk gate enum (hex)", "enter dcdc vout (dec)", "enter dcdc mode enum (dec)"
};

static const char * str_boot_cpu_menu[] = {
  " 1. HOST_CPU_0", " 2. HOST_CPU_1", " 3. EXTSYS_0", " 4. EXTSYS_1"
};

static const char * str_amux_menu[] = {
  " 1: PMU BG   1000mV", " 2: MAIN LDO 1800mV", " 3: PER BG   1000mV",
  " 4: ANA_PER  1800mV", " 5: HFRC/16  4.8MHz", " 6: MAIN RET LDO 550mV",
  " 7: VBAT RET LDO 550mV"," 8: LFXO    32768Hz", " 9: LFRC    32768Hz"
};

static void print_cfg_power_domains(uint32_t pd_status_mask)
{
    uint32_t tmp_mask;
    printf("%s\r\n", star_bar);
    printf("%s\r\n", star_domains);
    for (tmp_mask = 1; tmp_mask < 0x400; tmp_mask <<= 1){
        printf("%c", star_char);
        if(tmp_mask & pd_status_mask) {
            printf("%s", str_pd_stat[1]);
        }
        else {
            printf("%s", str_pd_stat[0]);
        }
    }
    printf("%c\r\n", star_char);
}

static void print_cfg_memory_blocks(uint32_t mb_status_mask)
{
    printf("%s\r\n", star_bar);
    printf("%s\r\n", star_memories);
    printf("%c", star_char);
#if SOC_FEAT_HAS_BULK_SRAM
    if (SRAM0_MASK & mb_status_mask) {
        printf("%s  ", str_pd_stat[1]);
    }
    else {
        printf("%s  ", str_pd_stat[0]);
    }
    printf("%c", star_char);
    if (SRAM1_MASK & mb_status_mask) {
        printf("%s  ", str_pd_stat[1]);
    }
    else {
        printf("%s  ", str_pd_stat[0]);
    }
    printf("%c", star_char);
#endif
    if ((SRAM4_1_MASK|SRAM5_1_MASK) & mb_status_mask) {
        printf("%s   ", str_pd_stat[1]);
    }
    else {
        printf("%s   ", str_pd_stat[0]);
    }
    printf("%c", star_char);
    if ((SRAM4_2_MASK|SRAM5_2_MASK) & mb_status_mask) {
        printf("%s   ", str_pd_stat[1]);
    }
    else {
        printf("%s   ", str_pd_stat[0]);
    }
    printf("%c", star_char);
    if (MRAM_MASK & mb_status_mask) {
        printf("%s ", str_pd_stat[1]);
    }
    else {
        printf("%s ", str_pd_stat[0]);
    }
    printf("%c", star_char);
    if (SERAM_MASK & mb_status_mask) {
        printf("%s  ", str_pd_stat[1]);
    }
    else {
        printf("%s  ", str_pd_stat[0]);
    }
    printf("%c", star_char);
    if (BACKUP4K_MASK & mb_status_mask) {
        printf("%s   ", str_pd_stat[1]);
    }
    else {
        printf("%s   ", str_pd_stat[0]);
    }
    printf("%c", star_char);
    printf(" 0x%06X ", mb_status_mask);
    printf("%c\r\n", star_char);
}

static void print_cfg_clock_sources(uint32_t aon_clk, uint32_t run_clk, scaled_clk_freq_t scaled_clk, uint32_t cpu_clk)
{
    if (aon_clk > CLK_SRC_LFXO) return;
    if (run_clk > CLK_SRC_PLL) return;
    if (scaled_clk > SCALED_FREQ_NONE) return;
    if (cpu_clk > 17) return;

    printf("%s\r\n", star_bar);
    printf("%s\r\n", star_clocks);
    printf("%c", star_char);
    printf(" %s    ", str_aon[aon_clk]);
    printf("%c", star_char);
    printf(" %s    ", str_run[run_clk]);
    printf("%c", star_char);
    printf(" %s ", str_scaled_clks[scaled_clk]);
    printf("%c", star_char);
    printf(" %s   ", str_cpu_clks[cpu_clk]);
    printf("%c\r\n", star_char);
}

static void print_cfg_ip_clk_and_pwr(uint32_t ip_clk_gates, uint32_t phy_pwr_gates)
{
    uint32_t mask = 1;
    printf("%s\r\n", star_bar);
    printf("%s\r\n", star_ip_clks[0]);
    while(mask < 0x4) {
        printf("%c", star_char);
        if (ip_clk_gates & mask) {
            printf("%s   ", str_pd_stat[1]);
        }
        else {
            printf("%s   ", str_pd_stat[0]);
        }
        mask = mask << 1;
    }
    mask = mask << 1;
    printf("%c", star_char);
    if (ip_clk_gates & mask) {
        printf("%s  ", str_pd_stat[1]);
    }
    else {
        printf("%s  ", str_pd_stat[0]);
    }
    mask = mask << 1;
    while(mask < 0x100) {
        printf("%c", star_char);
        if (ip_clk_gates & mask) {
            printf("%s", str_pd_stat[1]);
        }
        else {
            printf("%s", str_pd_stat[0]);
        }
        mask = mask << 1;
        if (mask & 0x4) mask = mask << 1;
    }
    printf("%c\r\n", star_char);
    printf("%s\r\n", star_bar);
    printf("%s\r\n", star_ip_clks[1]);
    while(mask < 0x4000) {
        printf("%c", star_char);
        if (ip_clk_gates & mask) {
            printf("%s", str_pd_stat[1]);
        }
        else {
            printf("%s", str_pd_stat[0]);
        }
        mask = mask << 1;
    }
    printf("%c", star_char);
    printf(" 0x%04X ", phy_pwr_gates);
    printf("%c\r\n", star_char);
}

static void print_cfg_dcdc_settings(uint32_t dcdc_voltage, uint32_t dcdc_mode)
{
    printf("%s\r\n", star_bar);
    printf("%s\r\n", star_dcdc);
    printf("%c", star_char);
    if (dcdc_voltage > 3)
        printf(" %9u ", dcdc_voltage);
    else
        printf(" %s ", str_voltage[dcdc_voltage]);
    printf("%c", star_char);
    printf(" %s ", str_mode[dcdc_mode]);
    // printf(" %u    ", dcdc_mode);
    printf("%c\r\n", star_char);
}

static void print_cfg_wake_settings(uint32_t vbat_cfg, uint32_t ewic_cfg, uint32_t vtor_cfg)
{
    printf("%s\r\n", star_bar);
    printf("%s\r\n", star_wake);
    printf("%c", star_char);
    printf(" 0x%05X ", vbat_cfg);
    printf("%c", star_char);
    printf(" 0x%05X ", ewic_cfg);
    printf("%c", star_char);
    printf(" 0x%08X ", vtor_cfg);
    printf("%c\r\n", star_char);
}

void print_run_cfg(run_profile_t *runp)
{
    print_cfg_power_domains(runp->power_domains);
    print_cfg_memory_blocks(runp->memory_blocks);
    print_cfg_clock_sources(runp->aon_clk_src, runp->run_clk_src, runp->scaled_clk_freq, runp->cpu_clk_freq);
    // print_cfg_ip_clk_and_pwr(runp->ip_clock_gating, runp->phy_pwr_gating);
    print_cfg_dcdc_settings(runp->dcdc_voltage, runp->dcdc_mode);
    printf("%s\r\n\n", star_bar);
}

void print_off_cfg(off_profile_t *offp)
{
    print_cfg_power_domains(offp->power_domains);
    print_cfg_memory_blocks(offp->memory_blocks);
    print_cfg_clock_sources(offp->aon_clk_src, offp->stby_clk_src, offp->stby_clk_freq, 17);
    // print_cfg_dcdc_settings(offp->dcdc_voltage, offp->dcdc_mode);
    print_cfg_wake_settings(offp->wakeup_events, offp->ewic_cfg, offp->vtor_address);
    printf("%s\r\n\n", star_bar);
}

int32_t print_user_menu()
{
    char ret = '0';
    for(uint32_t i = 0; i < 9; i++) {
        printf("%s\r\n", str_user_menu[i]);
    }
    printf("> ");
    while ((ret < '1') || (ret > '9')) {
        scanf("%c", &ret);
    }
    printf("%c\r\n\n", ret);

    return (ret - '0');
}

int32_t print_user_submenu(int32_t choice)
{
    if (choice > 8) return 0;

    char ret = '0';
    printf("%s\r\n", str_user_submenu[choice-1]);
    printf("> ");
    while ((ret < '1') || (ret > '9')) {
        scanf("%c", &ret);
    }
    printf("%c\r\n\n", ret);

    return (ret - '0');
}

int32_t print_cfg_main_menu()
{
    char ret = '0';
    for(uint32_t i = 0; i < 11; i++) {
        printf("%s\r\n", str_cfg_main_menu[i]);
    }
    printf("> ");
    while ((ret < '1') || (ret > '9')) {
        if (ret == 'a') break;
        if (ret == 'd') break;
        scanf("%c", &ret);
    }
    printf("%c\r\n\n", ret);

    if (ret >= 'a')
        return (ret - 'W');
    else
        return (ret - '0');
}

int32_t print_cfg_sub_menu(int32_t choice)
{
    if (choice > 9) return 0;

    uint32_t ret = 0;
    printf("%s\r\n", str_cfg_sub_menu[choice-1]);
    printf("> ");
    if (choice > 7) {
        scanf("%u", &ret);
        printf("%u\r\n\n", ret);
    }
    else {
        scanf("%x", &ret);
        printf("0x%06X\r\n\n", ret);
    }

    return ret;
}

static int32_t validate_register_address(uint32_t reg_addr)
{
    if (reg_addr & 3U)
        return 0;

    if (reg_addr <= (ITCM_SIZE - 4))
        return 1;

    if ((reg_addr >= DTCM_BASE) && (reg_addr <= (DTCM_BASE + DTCM_SIZE - 4)))
        return 1;

#if __HAS_BULK_SRAM
    if ((reg_addr >= APP_SRAM0_BASE) && (reg_addr <= (APP_SRAM0_BASE + APP_SRAM0_SIZE - 4)))
        return 1;

    if ((reg_addr >= APP_SRAM1_BASE) && (reg_addr <= (APP_SRAM1_BASE + APP_SRAM1_SIZE - 4)))
        return 1;
#endif

    if ((reg_addr >= 0x1A000000) && (reg_addr <= (0x1A020000 - 4)))
        return 1;

    if ((reg_addr >= 0x1A200000) && (reg_addr <= (0x1A60F000)))
        return 1;

    if ((reg_addr >= 0x40000000) && (reg_addr <= (0x49100000 - 4)))
        return 1;

    // if ((reg_addr >= SRAM9_BASE) && (reg_addr <= (SRAM8_BASE + SRAM8_SIZE - 4)))
        // return 1;

    if ((reg_addr >= 0xE000E100) && (reg_addr <= (0xE000E500 - 4)))
        return 1;

    return 0;
}

int32_t print_dialog_boot_testing()
{
    char ret = '0';
    for(uint32_t i = 0; i < 4; i++) {
        printf("%s\r\n", str_boot_cpu_menu[i]);
    }
    printf("> ");
    while ((ret < '1') || (ret > '4')) {
        scanf("%c", &ret);
    }
    printf("%c\r\n\n", ret);

    return (ret - '0');
}

uint32_t print_dialog_boot_addr()
{
    uint32_t address = 0, mask = 0, value = 0;

    printf("Enter hex address without 0x\r\n");
    printf("> ");
    scanf("%x", &address);
    printf("%x\r\n", address);

    if (validate_register_address(address) == 0) {
        printf("\n");
        return 1;
    }
    return address;
}

void print_dialog_read_reg()
{
    uint32_t address = 0, mask = 0, value = 0;

    printf("Enter hex address without 0x\r\n");
    printf("> ");
    scanf("%x", &address);
    printf("%x\r\n", address);

    if (validate_register_address(address) == 0) {
        printf("\n");
        return;
    }

    printf("Address: 0x%08X\r\n", address);
    volatile uint32_t *ptr = (volatile uint32_t *) address;
    uint32_t reg_data = *ptr;
    printf("RdData:  0x%08X\r\n\n", reg_data);
}

void print_dialog_write_reg()
{
    uint32_t address = 0, mask = 0, value = 0;

    printf("Enter hex address without 0x\r\n");
    printf("> ");
    scanf("%x", &address);
    printf("%x\r\n", address);

    if (validate_register_address(address) == 0) {
        printf("\n");
        return;
    }

    printf("Enter hex mask without 0x\r\n");
    printf("> ");
    scanf("%x", &mask);
    printf("%x\r\n", mask);

    printf("Enter hex value without 0x\r\n");
    printf("> ");
    scanf("%x", &value);
    printf("%x\r\n", value);

    printf("Address: 0x%08X\r\n", address);

    volatile uint32_t *ptr = (volatile uint32_t *) address;
    uint32_t reg_data = *ptr;
    reg_data &= ~mask;
    reg_data |= (value & mask);
    *ptr = reg_data;

    printf("RegMask: 0x%08X\r\n", mask);
    printf("WrData:  0x%08X\r\n", value);

    reg_data = *ptr;
    printf("RdData:  0x%08X\r\n\n", reg_data);
}

void print_dialog_configure_amux()
{
    static uint32_t amux_initialized = 0;
    if (amux_initialized == 0) {
        /* enable clock source in CFG_EXPMST0 for ADC12_0  */
        *(volatile uint32_t *) 0x4902F030 |= (1U);

        amux_initialized = 1;
    }

    char ret = '0';
    for(uint32_t i = 0; i < 9; i++) {
        printf("%s\r\n", str_amux_menu[i]);
    }
    printf("> ");
    while ((ret < '1') || (ret > '9')) {
        scanf("%c", &ret);
    }
    printf("%c\r\n\n", ret);

    uint32_t amux_index = (ret - '0') - 1;
    const uint32_t amux_array[] = {0x80,0x87,0xA0,0xA7,0xC0,0xD0,0xD1,0xD2,0xD3};

    *(volatile uint32_t *)(0x49020038) = amux_array[amux_index] << 24;
}

void adjust_run_cfg(run_profile_t *runp, int32_t opt1, int32_t opt2)
{
    int32_t ret;
    uint32_t service_response = 0;

    switch (opt1)
    {
    case 1:
        runp->power_domains = opt2;
        break;

    case 2:
        runp->memory_blocks = opt2;
        break;

    case 3:
        runp->aon_clk_src = opt2;
        break;

    case 4:
        runp->run_clk_src = opt2;
        break;

    case 5:
        runp->scaled_clk_freq = opt2;
        break;

    case 6:
        runp->cpu_clk_freq = opt2;
        break;

    case 7:
        runp->ip_clock_gating = opt2;
        break;

    case 8:
        runp->dcdc_voltage = opt2;
        break;

    case 9:
        runp->dcdc_mode = opt2;
        break;

    default:
        break;
    }
}

void adjust_off_cfg(off_profile_t *offp, int32_t opt1, int32_t opt2)
{
    int32_t ret;
    uint32_t service_response = 0;

    switch (opt1)
    {
    case 1:
        offp->power_domains = opt2;
        break;

    case 2:
        offp->memory_blocks = opt2;
        break;

    case 3:
        offp->aon_clk_src = opt2;
        break;

    case 4:
        offp->stby_clk_src = opt2;
        break;

    case 5:
        offp->stby_clk_freq = opt2;
        break;

    case 7:
        offp->ip_clock_gating = opt2;
        break;

    case 8:
        offp->dcdc_voltage = opt2;
        break;

    case 9:
        offp->dcdc_mode = opt2;
        break;

    default:
        break;
    }
}