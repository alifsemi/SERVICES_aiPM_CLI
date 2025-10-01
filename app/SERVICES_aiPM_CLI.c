#include <math.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <RTE_Components.h>
#include CMSIS_device_header
#include <se_services_port.h>
#include <app_mem_regions.h>
#include <board_config.h>
#include <sys_clocks.h>
#include <pinconf.h>
#include <aipm.h>
#include <alif.h>

#if defined(RTE_CMSIS_Compiler_STDIN_Custom) || defined(RTE_CMSIS_Compiler_STDOUT_Custom)
#include <retarget_init.h>
#include <retarget_config.h>
#if defined(RTE_CMSIS_Compiler_STDIN_Custom)
#include <retarget_stdin.h>
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
#include <retarget_stdout.h>
#endif
#endif

#define COREMARK_ENABLED    1
#if COREMARK_ENABLED
#include "coremark.h"
extern volatile ee_s32 seed4_volatile;
extern void coremark_main(void);
#endif

#define ETHOS_ENABLED       1
#if ETHOS_ENABLED
extern void npuTestStartU55(uint32_t test_count, uint32_t test_select);
#if defined(ENSEMBLE_SOC_GEN2)
extern void npuTestStartU85(uint32_t test_count, uint32_t test_select);
#endif
#endif

#define HYPERRAM_ENABLED __HAS_HYPER_RAM
#if HYPERRAM_ENABLED
extern int ospi_hyperram_init(void);
#endif

#include "gpio.h"
#include "lptimer.h"

#include "deviceID.h"
#include "debug_clks.h"
#include "debug_pwr.h"
#include "drv_counter.h"
#include "drv_mhu.h"
#include "drv_pll.h"
#include "soc_clk.h"
#include "soc_pwr.h"
#include "hostbase.h"

#include "SERVICES_aiPM_print.h"
#include "SERVICES_other.h"

#define TICKS_PER_SECOND        1000
volatile uint32_t ms_ticks;
void SysTick_Handler (void) { ms_ticks++; }
void delay_ms(uint32_t nticks) { nticks += ms_ticks; while(ms_ticks < nticks) __WFI(); }

void LPTIMER1_IRQHandler()
{
    LPTIMER_Type *lptimer = (LPTIMER_Type *) LPTIMER_BASE;
    lptimer_disable_counter(lptimer, 1);
    lptimer_clear_interrupt(lptimer, 1);
    lptimer_clear_interrupt(lptimer, 1);
    NVIC_DisableIRQ(61);
}

static void lptimer_init()
{
    NVIC_DisableIRQ(61);
    uint32_t count = 327680 - 1;    // 10 seconds

    LPTIMER_Type *lptimer = (LPTIMER_Type *) LPTIMER_BASE;
    lptimer_disable_counter(lptimer, 1);
    lptimer_set_mode_userdefined(lptimer, 1);
    lptimer_load_count(lptimer, 1, &count);
    lptimer_enable_counter(lptimer, 1);
    lptimer_clear_interrupt(lptimer, 1);
    lptimer_clear_interrupt(lptimer, 1);

    NVIC_ClearPendingIRQ(61);
    NVIC_EnableIRQ(61);
}

volatile uint32_t mhu_rcv_count, mhu_snd_count;
static void mhu_rcv_isr()
{
    mhu_rcv_count++;
    MHU_RECEIVER_Clear(RTSS_RX_MHU0_BASE, 0, 0x1234);
}

static void mhu_snd_isr()
{
    mhu_snd_count++;
}

static void mhu_init()
{
    mhu_rcv_count = 0;
    mhu_snd_count = 0;
    MHU_register_cb(mhu_rcv_isr, mhu_snd_isr);
}

static int32_t print_wakeup_source()
{
    int32_t ret = 0;

    /* check the pending status of IRQs that are capable of powering up the M55 */
    /* then enable IRQ to allow ISR to clear the pending status */
    if (NVIC_GetPendingIRQ(41)) {
        printf("MHU RX is pending\r\n");
        ret = 1;
    }
    if (NVIC_GetPendingIRQ(43)) {
        printf("MHU TX is pending\r\n");
        ret = 1;
    }
    if (NVIC_GetPendingIRQ(57)) {
        printf("LPGPIO is pending\r\n");
        NVIC_EnableIRQ(57);
        ret = 1;
    }
    if (NVIC_GetPendingIRQ(60)) {
        printf("LPTIMER0 is pending\r\n");
        NVIC_EnableIRQ(60);
        ret = 1;
    }
    if (NVIC_GetPendingIRQ(61)) {
        printf("LPTIMER1 is pending\r\n");
        NVIC_EnableIRQ(61);
        ret = 1;
    }
    return ret;
}

#include "uart.h"
#define _UART_BASE_(n)      UART##n##_BASE
#define UART_BASE(n)        _UART_BASE_(n)
#define _UART_CLK_SRC_(n)   RTE_UART##n##_CLK_SOURCE
#define UART_CLK_SRC(n)     _UART_CLK_SRC_(n)
static void reconfigure_uart()
{
#if defined(RTE_CMSIS_Compiler_STDIN_Custom) || defined(RTE_CMSIS_Compiler_STDOUT_Custom)
#if PRINTF_UART_CONSOLE == LP
    uart_set_baudrate((UART_Type*)LPUART_BASE, SystemCoreClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#elif UART_CLK_SRC(PRINTF_UART_CONSOLE) == 0
    SocTopClockHFOSC();
    uart_set_baudrate((UART_Type*)UART_BASE(PRINTF_UART_CONSOLE), SystemHFOSCClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#else
    SystBusClkUpdate();
    uart_set_baudrate((UART_Type*)UART_BASE(PRINTF_UART_CONSOLE), SystemAPBClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#endif
#endif
}

void main (void)
{
    s32k_cntr_init();
    refclk_cntr_init();

    CoreClockUpdate();
    SysTick_Config(SystemCoreClock/TICKS_PER_SECOND);

#if PRINTF_UART_CONSOLE != LP
    /* enable HFOSC_CLK source, used for peripherals like UART */
    CGU->CLK_ENA |= 1U << 23;
#endif
#if defined(RTE_CMSIS_Compiler_STDIN_Custom)
    stdin_init();
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    stdout_init();
#endif
    if (CoreID()) {
        printf("RTSS-HE is running\r\n\n");
    }
    else {
        printf("RTSS-HP is running\r\n\n");
    }
    int32_t wakeup_event = print_wakeup_source();
    mhu_init();

    /* Initialize the SE services */
    uint32_t ret = 0;
    uint32_t reg_data = 0;
    uint32_t service_response = 0;
    se_services_port_init();

    print_se_startup_info();

    run_profile_t runp = {0};
    off_profile_t offp = {0};
    ret = SERVICES_get_run_cfg(se_services_s_handle, &runp, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    /* print the run config at boot before this application makes changes */
    printf("aiPM Run Config Settings at Application Start:\r\n");
    print_run_cfg(&runp);

    /* weird stuff happening when aiPM is used on Eagle */
    if ((wakeup_event == 0) && (DeviceID() != 2)) {
        runp.dcdc_voltage = 775;
        runp.power_domains = PD6_MASK;// | PD8_MASK;// | PD4_MASK | PD1_MASK;
        runp.memory_blocks = MRAM_MASK | BACKUP4K_MASK;
#if SOC_FEAT_HAS_BULK_SRAM
        runp.memory_blocks |= SRAM0_MASK | SRAM1_MASK;
#endif
        runp.vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;
        runp.scaled_clk_freq = SCALED_FREQ_RC_STDBY_0_6_MHZ;

        ret = SERVICES_set_run_cfg(se_services_s_handle, &runp, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }

        SysTick_Config(CoreClockUpdate()/TICKS_PER_SECOND);
        reconfigure_uart();
        refclk_cntr_update();

        ret = SERVICES_get_off_cfg(se_services_s_handle, &offp, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }

        /* setting some memories to retain by default */
        offp.memory_blocks = MRAM_MASK | SERAM_MASK | BACKUP4K_MASK;
#if defined(M55_HE)
        offp.memory_blocks |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM5_1_MASK | SRAM5_2_MASK;
#endif
#if defined(M55_HE_E1C)
        offp.memory_blocks |= SRAM4_3_MASK | SRAM4_4_MASK | SRAM5_3_MASK | SRAM5_4_MASK | SRAM5_5_MASK;
#endif
        offp.stby_clk_src = CLK_SRC_HFRC;
        offp.stby_clk_freq = SCALED_FREQ_RC_STDBY_0_6_MHZ;
#if defined(M55_HE)
        offp.wakeup_events = WE_LPTIMER1;
        offp.ewic_cfg = EWIC_VBAT_TIMER1;
#endif
        offp.aon_clk_src = CLK_SRC_LFXO;
        offp.vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;
        offp.vtor_address = SCB->VTOR;

        ret = SERVICES_set_off_cfg(se_services_s_handle, &offp, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }

        ret = SERVICES_get_run_cfg(se_services_s_handle, &runp, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }

        ret = SERVICES_get_off_cfg(se_services_s_handle, &offp, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }

        /* print the run config and off config after this application has made changes */
        printf("aiPM Run Config after Configuration:\r\n");
        print_run_cfg(&runp);
        printf("aiPM Off Config after Configuration:\r\n");
        print_off_cfg(&offp);
    }

    int32_t user_choice, user_subchoice, cfg_choice, cfg_param;
    while(1) {
        user_choice = print_user_menu();
        user_subchoice = print_user_submenu(user_choice);

        switch (user_choice)
        {
        case 1:
            switch (user_subchoice)
            {
            case 1:
                while(1) {
                    cfg_choice = print_cfg_main_menu();
                    cfg_param = print_cfg_sub_menu(cfg_choice);

                    /* discard changes */
                    if (cfg_choice == 13) break;

                    adjust_run_cfg(&runp, cfg_choice, cfg_param);

                    if (cfg_choice == 10) {
                        ret = SERVICES_set_run_cfg(se_services_s_handle, &runp, &service_response);

                        SysTick_Config(CoreClockUpdate()/TICKS_PER_SECOND);
                        reconfigure_uart();
                        refclk_cntr_update();

                        if (ret != 0) {
                            SERVICES_ret(ret);
                        }
                        else if (service_response != 0) {
                            SERVICES_response(service_response);
                        }

                        ret = SERVICES_get_run_cfg(se_services_s_handle, &runp, &service_response);
                        if (ret != 0) {
                            SERVICES_ret(ret);
                        }
                        else if (service_response != 0) {
                            SERVICES_response(service_response);
                        }
                        else {
                            printf("aiPM Run Config after Configuration:\r\n");
                            print_run_cfg(&runp);
                        }
                        break;
                    } else {
                        /* print settings for review before applying them */
                        printf("aiPM Run Config Settings to be applied:\r\n");
                        print_run_cfg(&runp);
                    }
                }
                break;

            case 2:
                ret = SERVICES_get_run_cfg(se_services_s_handle, &runp, &service_response);
                if (ret != 0) {
                    SERVICES_ret(ret);
                }
                else if (service_response != 0) {
                    SERVICES_response(service_response);
                }
                else {
                    print_run_cfg(&runp);
                }
                break;

            default:
                break;
            }
            break;

        case 2:
            switch(user_subchoice)
            {
            case 1:
                while(1) {
                    cfg_choice = print_cfg_main_menu();
                    cfg_param = print_cfg_sub_menu(cfg_choice);

                    /* discard changes */
                    if (cfg_choice == 13) break;

                    adjust_off_cfg(&offp, cfg_choice, cfg_param);

                    if (cfg_choice == 10) {
                        ret = SERVICES_set_off_cfg(se_services_s_handle, &offp, &service_response);

                        if (ret != 0) {
                            SERVICES_ret(ret);
                        }
                        else if (service_response != 0) {
                            SERVICES_response(service_response);
                        }

                        ret = SERVICES_get_off_cfg(se_services_s_handle, &offp, &service_response);
                        if (ret != 0) {
                            SERVICES_ret(ret);
                        }
                        else if (service_response != 0) {
                            SERVICES_response(service_response);
                        }
                        else {
                            printf("aiPM Off Config after Configuration:\r\n");
                            print_off_cfg(&offp);
                        }
                        break;
                    }
                    else {
                        /* print settings for review before applying them */
                        printf("aiPM Off Config Settings to be applied:\r\n");
                        print_off_cfg(&offp);
                    }
                }
                break;

            case 2:
                ret = SERVICES_get_off_cfg(se_services_s_handle, &offp, &service_response);
                if (ret != 0) {
                    SERVICES_ret(ret);
                }
                else if (service_response != 0) {
                    SERVICES_response(service_response);
                }
                else {
                    print_off_cfg(&offp);
                }
                break;

            default:
                break;
            }
            break;

        case 3:
            switch(user_subchoice)
            {
            case 1:
                DEBUG_frequencies();
                break;

            case 2:
                DEBUG_peripherals();
                break;

            case 3:
                DEBUG_power();
                break;

            case 4:
                OSC_uninitialize();
                break;

            case 5:
                OSC_initialize();
                break;

            case 6:
                PLL_uninitialize();
                break;

            case 7:
                PLL_initialize(38400000);
                break;

            default:
                break;
            }
            SysTick_Config(CoreClockUpdate()/TICKS_PER_SECOND);
            reconfigure_uart();
            refclk_cntr_update();
            break;

        case 4:
            while(user_subchoice != 9) {
                switch(user_subchoice)
                {
                case 1:
                    print_dialog_read_reg();
                    break;

                case 2:
                    print_dialog_write_reg();
                    break;

                case 3:
                    DEBUG_clks_xvclks();
                    break;

                case 4:
                    ANA->DCDC_REG2 ^= (1U << 23);
                    break;

                case 5:
                    ANA->VBAT_ANA_REG2 ^= (1U << 5);
                    break;

                case 6:
                    HOSTBASE->BSYS_PWR_REQ ^= 0x20;
                    break;

                case 7:
                    HOSTBASE->BSYS_PWR_REQ ^= 0x04;
                    break;

                case 8:
#if defined(M55_HE)
                lptimer_init();
                ANA->WKUP_CTRL = WE_LPTIMER1;
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    pinconf_set(PRINTF_UART_CONSOLE_TX_PORT_NUM,  PRINTF_UART_CONSOLE_TX_PIN, 0, 0);     /* set TX_PIN as input */
#endif
                    /* this pulls the plug, there is no coming back from this */
                    STOP_MODE->VBAT_STOP_MODE_REG = 1U; __DSB(); __ISB();
                    break;

                default:
                    break;
                }
                user_subchoice = print_user_submenu(user_choice);
                if (user_subchoice == 7) break;
            }
            break;

        case 5:
            switch(user_subchoice)
            {
            case 1:
#if COREMARK_ENABLED == 1
                /* ten seconds of Coremark */
                seed4_volatile = roundf(0.00003 * SystemCoreClock);
                coremark_main();
#endif
                break;

            case 2:
                /* ten seconds of while(1) */
                uint32_t nticks = ms_ticks + (10 * TICKS_PER_SECOND);
                while(ms_ticks < nticks);
                break;

            case 3:
                /* ten seconds of light sleep */
                delay_ms(10000);
                break;

            case 4:
                /* ten seconds of deep sleep */
#if defined(M55_HE)
                lptimer_init();
#endif
                __disable_irq();
                pm_core_enter_deep_sleep();
                __enable_irq();
                break;

            case 5:
                /* M55-HE: ten seconds of subsystem off 
                 * M55-HP: subsystem stays off until woken by MHU */
#if defined(M55_HE)
                lptimer_init();
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom) && (PRINTF_UART_CONSOLE == LP)
    pinconf_set(PRINTF_UART_CONSOLE_TX_PORT_NUM,  PRINTF_UART_CONSOLE_TX_PIN, 0, 0);     /* set TX_PIN as input */
#endif
                __disable_irq();
                pm_core_enter_deep_sleep_request_subsys_off();
                __enable_irq();

#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    pinconf_set(PRINTF_UART_CONSOLE_TX_PORT_NUM,  PRINTF_UART_CONSOLE_TX_PIN, \
            PRINTF_UART_CONSOLE_TX_PINMUX_FUNCTION, PRINTF_UART_CONSOLE_TX_PADCTRL);  /* restore TX_PIN state */
#endif
                break;

            case 6:
                NVIC_SystemReset();
                break;

            case 7:
                MHU_SENDER_Set(RTSS_TX_MHU0_BASE, 0, 0x1234);
                break;

            default:
                break;
            }
            break;

        case 6:
#if ETHOS_ENABLED == 1
            switch(user_subchoice)
            {
            case 1:
                /* ten seconds of Ethos "Typical" Test Case */
#if defined(M55_HP)
                npuTestStartU55(roundf(0.00092 * SystemCoreClock), 0);
#elif defined(M55_HE)
                npuTestStartU55(roundf(0.00050 * SystemCoreClock), 0);
#endif
                break;

            case 2:
#if defined(ENSEMBLE_SOC_GEN2)
                /* ten seconds of Ethos "Typical" Test Case */
                SystBusClkUpdate();
                npuTestStartU85(roundf(0.00041 * SystemAXIClock), 0);
#else
                printf("Feature not implemented on this device\r\n\n");
#endif
                break;

            case 3:
                /* 100 iterations of Ethos KWS (MicroNet Medium) Test Case, Model in TCM */
                npuTestStartU55(100, 1);
                break;

            case 4:
#if SOC_FEAT_HAS_BULK_SRAM
                /* 100 iterations of Ethos KWS (MicroNet Medium) Test Case, Model in SRAM */
                npuTestStartU55(100, 2);
#else
                printf("Feature not implemented on this device\r\n\n");
#endif
                break;

            case 5:
                /* 100 iterations of Ethos KWS (MicroNet Medium) Test Case, Model in MRAM */
                npuTestStartU55(100, 3);
                break;

            case 6:
#if defined(ENSEMBLE_SOC_GEN2)
                /* 100 iterations of Ethos KWS (MicroNet Medium) Test Case, Model in SRAM */
                npuTestStartU85(100, 1);
#else
                printf("Feature not implemented on this device\r\n\n");
#endif
                break;

            case 7:
#if defined(ENSEMBLE_SOC_GEN2)
                /* 100 iterations of Ethos KWS (MicroNet Medium) Test Case, Model in MRAM */
                npuTestStartU85(100, 2);
#else
                printf("Feature not implemented on this device\r\n\n");
#endif
                break;

            case 8:
#if defined(ENSEMBLE_SOC_GEN2)
                /* 100 iterations of Ethos KWS (MicroNet Medium) Test Case, Model in HyperRAM */
                ospi_hyperram_init();
                npuTestStartU85(100, 3);
#else
                printf("Feature not implemented on this device\r\n\n");
#endif
                break;

            default:
                break;
            }
#endif
            break;

        case 7:
            if (user_subchoice < 5) {
                int32_t cpu_selection = print_dialog_boot_testing() - 1;
                SERVICES_boot_testing(user_subchoice, cpu_selection);
            } else {
                SERVICES_boot_testing(user_subchoice, 0);
            }
            break;

        case 8:
            switch(user_subchoice)
            {
            case 1:
                ret = SERVICES_clocks_select_pll_source(se_services_s_handle, PLL_SOURCE_OSC, PLL_TARGET_SECENC, &service_response);
                if (ret != 0) {
                    SERVICES_ret(ret);
                }
                if (service_response != 0) {
                    SERVICES_response(service_response);
                }
                break;

            case 2:
                ret = SERVICES_power_se_sleep_req(se_services_s_handle, 0, &service_response);
                if (ret != 0) {
                    SERVICES_ret(ret);
                }
                if (service_response != 0) {
                    SERVICES_response(service_response);
                }
                break;

            case 3:
#if defined(M55_HE)
                lptimer_init();
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    pinconf_set(PRINTF_UART_CONSOLE_TX_PORT_NUM,  PRINTF_UART_CONSOLE_TX_PIN, 0, 0);     /* set TX_PIN as input */
#endif
                ret = SERVICES_power_stop_mode_req(se_services_s_handle, OFF_PROFILE, 0);
                delay_ms(5);
                if (ret != 0) {
                    SERVICES_ret(ret);
                }
                if (service_response != 0) {
                    SERVICES_response(service_response);
                }

#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    pinconf_set(PRINTF_UART_CONSOLE_TX_PORT_NUM,  PRINTF_UART_CONSOLE_TX_PIN, \
            PRINTF_UART_CONSOLE_TX_PINMUX_FUNCTION, PRINTF_UART_CONSOLE_TX_PADCTRL);  /* restore TX_PIN state */
#endif
                break;

            default:
                break;
            }
            break;

        case 9:
            print_dialog_configure_amux();
            break;

        default:
            break;
        }
    }
}
