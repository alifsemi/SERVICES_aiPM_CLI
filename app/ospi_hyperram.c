/* Copyright (C) 2025 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
 */

/***************************************************************************************
 * @file     ospi_hyperram.c
 * @author   Manoj A Murudi
 * @email    manoj.murudi@alifsemi.com
 * @version  V1.0.0
 * @date     20-May-2025
 * @brief    Demo program for HyperRAM on OSPI
 ***************************************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include "RTE_Components.h"

#include "alif.h"
#include "pinconf.h"
#include "sys_utils.h"
#include "ospi_hyperram.h"
#include "ospi_hyperram_xip.h"
#include "Driver_IO.h"

#define OSPI_RESET_PORT                15
extern ARM_DRIVER_GPIO ARM_Driver_GPIO_(OSPI_RESET_PORT);
static ARM_DRIVER_GPIO *GPIODrv = &ARM_Driver_GPIO_(OSPI_RESET_PORT);

#define DDR_DRIVE_EDGE 1
#define RXDS_DELAY     8
#define SIGNAL_DELAY   22
#define OSPI_BUS_SPEED 100000000 /* 100MHz */
#define OSPI_DFS       32

#define OSPI_RESET_PIN  2
#define OSPI_XIP_BASE   0xA0000000
#define HRAM_SIZE_BYTES (64 * 1024 * 1024) /* 64MB */

#define SLAVE_SELECT    0
#define WAIT_CYCLES     6

static const ospi_hyperram_xip_config issi_config = {
    .instance       = OSPI_INSTANCE_0,
    .bus_speed      = OSPI_BUS_SPEED,
    .hyperram_init  = NULL, /* No special initialization needed by issi hyperram device */
    .ddr_drive_edge = DDR_DRIVE_EDGE,
    .rxds_delay     = RXDS_DELAY,
    .signal_delay   = SIGNAL_DELAY,
    .wait_cycles    = WAIT_CYCLES,
    .dfs            = OSPI_DFS,
    .slave_select   = SLAVE_SELECT,
    .spi_mode       = OSPI_SPI_MODE_OCTAL
};

static int32_t issi_pinmux_setup(void)
{
    int32_t ret;

    ret = pinconf_set(PORT_(BOARD_OSPI0_D0_GPIO_PORT),
                      BOARD_OSPI0_D0_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    ret = pinconf_set(PORT_(BOARD_OSPI0_D1_GPIO_PORT),
                      BOARD_OSPI0_D1_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    ret = pinconf_set(PORT_(BOARD_OSPI0_D2_GPIO_PORT),
                      BOARD_OSPI0_D2_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    ret = pinconf_set(PORT_(BOARD_OSPI0_D3_GPIO_PORT),
                      BOARD_OSPI0_D3_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    ret = pinconf_set(PORT_(BOARD_OSPI0_D4_GPIO_PORT),
                      BOARD_OSPI0_D4_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    ret = pinconf_set(PORT_(BOARD_OSPI0_D5_GPIO_PORT),
                      BOARD_OSPI0_D5_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    ret = pinconf_set(PORT_(BOARD_OSPI0_D6_GPIO_PORT),
                      BOARD_OSPI0_D6_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    ret = pinconf_set(PORT_(BOARD_OSPI0_D7_GPIO_PORT),
                      BOARD_OSPI0_D7_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    /* SCLK */
    ret = pinconf_set(PORT_(BOARD_OSPI0_SCLK_GPIO_PORT),
                      BOARD_OSPI0_SCLK_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    /* SCLK_N */
    ret = pinconf_set(PORT_(BOARD_OSPI0_SCLKN_GPIO_PORT),
                      BOARD_OSPI0_SCLKN_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    /* SS_0 */
    ret = pinconf_set(PORT_(BOARD_OSPI0_SS0_GPIO_PORT),
                      BOARD_OSPI0_SS0_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    /* RXDS */
    ret = pinconf_set(PORT_(BOARD_OSPI0_RXDS_GPIO_PORT),
                      BOARD_OSPI0_RXDS_GPIO_PIN,
                      PINMUX_ALTERNATE_FUNCTION_1, PADCTRL_SLEW_RATE_FAST |
                      PADCTRL_READ_ENABLE | PADCTRL_OUTPUT_DRIVE_STRENGTH_12MA);
    if (ret) {
        return -1;
    }

    return 0;
}

static int32_t hyperram_reset(void)
{
    int32_t ret;

    ret = pinconf_set(OSPI_RESET_PORT, OSPI_RESET_PIN, PINMUX_ALTERNATE_FUNCTION_0, 0);
    if (ret) {
        return -1;
    }

    ret = GPIODrv->Initialize(OSPI_RESET_PIN, NULL);
    if (ret != ARM_DRIVER_OK) {
        return -1;
    }

    ret = GPIODrv->PowerControl(OSPI_RESET_PIN, ARM_POWER_FULL);
    if (ret != ARM_DRIVER_OK) {
        return -1;
    }

    ret = GPIODrv->SetDirection(OSPI_RESET_PIN, GPIO_PIN_DIRECTION_OUTPUT);
    if (ret != ARM_DRIVER_OK) {
        return -1;
    }

    ret = GPIODrv->SetValue(OSPI_RESET_PIN, GPIO_PIN_OUTPUT_STATE_LOW);
    if (ret != ARM_DRIVER_OK) {
        return -1;
    }

    sys_busy_loop_us(30);

    ret = GPIODrv->SetValue(OSPI_RESET_PIN, GPIO_PIN_OUTPUT_STATE_HIGH);
    if (ret != ARM_DRIVER_OK) {
        return -1;
    }

    sys_busy_loop_us(150);

    return 0;
}

int ospi_hyperram_init(void)
{
    uint32_t static init_done = 0;
    if (init_done == 0) {
        issi_pinmux_setup();
        hyperram_reset();
        if (ospi_hyperram_xip_init(&issi_config) < 0)
        {
            printf("OSPI HyperRAM init failed\n\n");
            return -1;
        }
        init_done = 1;
    }

#define OSPI_VERIFICATION_STEP 0
#if OSPI_VERIFICATION_STEP == 1
    uint32_t *const ptr          = (uint32_t *) OSPI_XIP_BASE;
    uint32_t        total_errors = 0, random_val;

    printf("Writing data to the XIP region...\n");

    srand(1);
    for (uint32_t i = 0; i < (HRAM_SIZE_BYTES / sizeof(uint32_t)); i++) {
        ptr[i] = rand();
    }

    printf("Done. Reading back and verifying...\n");

    srand(1);
    for (uint32_t i = 0; i < (HRAM_SIZE_BYTES / sizeof(uint32_t)); i++) {
        random_val = rand();
        if (ptr[i] != random_val) {
            printf("Data error at addr %" PRIx32 ", got %" PRIx32 ", expected %" PRIx32 "\n",
                   (i * sizeof(uint32_t)),
                   ptr[i],
                   random_val);
            total_errors++;
        }
    }

    printf("Verification done, total errors = %" PRIu32 "\n\n", total_errors);
#endif
    return 0;
}
