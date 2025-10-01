#include <stdio.h>
#include <string.h>
#include <se_services_port.h>

#include "SERVICES_aiPM_print.h"
#include "SERVICES_other.h"

#define TOC_NAME_COUNT 4
static const char str_cpu_names[TOC_NAME_COUNT][TOC_NAME_LENGTH] = {
  "A32_0", "A32_1", "M55_HP", "M55_HE"
};

#define USE_HARD_TOC_NAMES  0
#if USE_HARD_TOC_NAMES == 0
static char toc_names[TOC_NAME_COUNT][TOC_NAME_LENGTH] = {0};
#else
static const char toc_hard_names[TOC_NAME_COUNT][TOC_NAME_LENGTH] = {
  "A32_APP", "A32_APP", "HP_APP", "HE_APP"
};
#endif

void print_se_startup_info()
{
    int32_t ret = 0;
    uint32_t service_response = 0;

    printf("SERVICES API Version: %s\r\n", SERVICES_version());

    char print_buffer[80] = {0};
    ret = SERVICES_get_se_revision(se_services_s_handle, print_buffer, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    printf("%s\r\n\n", print_buffer);

    SERVICES_version_data_t device_info = {0};
    ret = SERVICES_system_get_device_data(se_services_s_handle, &device_info, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    printf("ALIF_PN: %s\t", device_info.ALIF_PN);
    printf("Revision: %X\t", device_info.revision_id);
    printf("LCS=%u\r\n\n", device_info.LCS);

        /* Bolt B4 */
    if (!((device_info.revision_id == 0xB400) ||
        /* Spark A5 */
    (device_info.revision_id == 0xA501) ||
        /* Eagle engr samples */
    (device_info.revision_id == 0x02A0))) {
        /* device not supported */
        printf("Application stopping, device revision %X is not supported.\r\n\n", device_info.revision_id);
        while(1);
    }

    SERVICES_toc_data_t toc_data = {0};
    ret = SERVICES_system_get_toc_data(se_services_s_handle, &toc_data, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    printf("ATOC Information:\r\n");
    for (int toc_entry_num = 0; toc_entry_num < toc_data.number_of_toc_entries; toc_entry_num++)
    {
        SERVICES_toc_info_t *toc_entry = &toc_data.toc_entry[toc_entry_num];
        if (toc_entry->cpu < TOC_NAME_COUNT) {
            printf("Entry %d: %s on %s (CPU %d)\r\n", toc_entry_num, toc_entry->image_identifier, str_cpu_names[toc_entry->cpu], toc_entry->cpu);
#if USE_HARD_TOC_NAMES == 0
            strncpy(toc_names[toc_entry->cpu], toc_entry->image_identifier, TOC_NAME_LENGTH);
#endif
        }
    }
    printf("\n");
}

void SERVICES_boot_testing(int32_t boot_selection, int32_t cpu_selection)
{
    int32_t ret = 0;
    uint32_t service_response = 0;

    switch(boot_selection)
    {
    case 1:
#if USE_HARD_TOC_NAMES == 0
        ret = SERVICES_boot_process_toc_entry(se_services_s_handle, toc_names[cpu_selection], &service_response);
#else
        ret = SERVICES_boot_process_toc_entry(se_services_s_handle, toc_hard_names[cpu_selection], &service_response);
#endif
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }
        break;

    case 2:
        uint32_t boot_addr = print_dialog_boot_addr();
        if (boot_addr == 1) {
            break;
        }

        ret = SERVICES_boot_cpu(se_services_s_handle, cpu_selection, boot_addr, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }
        break;

    case 3:
        ret = SERVICES_boot_reset_cpu(se_services_s_handle, cpu_selection, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }
        break;

    case 4:
        ret = SERVICES_boot_release_cpu(se_services_s_handle, cpu_selection, &service_response);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }
        break;

    case 5:
        ret = SERVICES_boot_reset_soc(se_services_s_handle);
        if (ret != 0) {
            SERVICES_ret(ret);
        }
        if (service_response != 0) {
            SERVICES_response(service_response);
        }
        while(1);
        break;

    default:
        break;
    } 
}

void SERVICES_ret(uint32_t ret)
{
    printf("\r\n\nSERVICES function ret = %u\r\n\n", ret);
}

void SERVICES_response(uint32_t response)
{
    printf("\r\n\nSERVICES function response = %u\r\n\n", response);
}