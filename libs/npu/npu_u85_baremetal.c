#include <RTE_Components.h>
#include CMSIS_device_header
#include "ethosu85_interface.h"
#include "app_mem_regions.h"
#include "drv_counter.h"
#include "sys_utils.h"
#include "pm.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ENABLE_NPU_KWS          1
#define ENABLE_NPU_VALIDATION   1

extern const uint32_t TYPICAL_U85_REFERENCE_SIZE;
extern const uint32_t TYPICAL_U85_MEMORYMAP_SIZE;
extern const uint32_t TYPICAL_U85_MEMORYMAP_BASEP0_offset;
extern const uint32_t typical_u85_mem_map[];
extern const uint32_t typical_u85_reference[];
#define TYPICAL_U85_MEMORYMAP_SIZE_IN_BYTES (TYPICAL_U85_MEMORYMAP_SIZE * 4)
#define TYPICAL_U85_REFERENCE_SIZE_IN_BYTES (TYPICAL_U85_REFERENCE_SIZE * 4)

#if ENABLE_NPU_KWS
extern const void *get_kws_u85_model(void);
extern size_t get_kws_u85_model_len(void);
extern size_t get_kws_u85_qsize(void);
extern size_t get_kws_u85_qbase_offset(void);
extern size_t get_kws_u85_basep0_offset(void);
extern const uint8_t kws_reference_input[];
extern const uint8_t kws_reference_output[];
#if __HAS_HYPER_RAM
static const uint8_t kws_model_hram[157600] __attribute__((aligned(16), section(".bss.kws_model_hram")));
#endif
static const uint8_t kws_model_sram[157600] __attribute__((aligned(16), section(".bss.kws_model_sram")));
#endif

#define NPU_BASE_ADDRESS                0x49042000
#define NPU                             ((volatile struct NPU_REG *) NPU_BASE_ADDRESS)

#define NPU_IRQn                        366

static const struct reset_r npu_reset_config = { .pending_CPL = PRIVILEGE_LEVEL_USER, .pending_CSL = SECURITY_LEVEL_SECURE };

static const void *npu_readonly_addr;
static const void *reference_data_addr;
static uint32_t reference_data_size;
// static void *npu_workspace_addr; /* NPU workspace always in SRAM */
static uint32_t npu_workspace_addr[32768] __attribute__((aligned(16), section(".bss.kws_workspace_sram")));
static uint32_t npu_workspace_size;
static uint32_t npu_output_offset;
static uint32_t npu_register_QBASE_offset;
static uint32_t npu_register_BASEP0_offset;
static uint32_t npu_command_length;
static uint32_t test_type;

static volatile int continue_npu = 0;
static volatile int npu_running = 0;
static volatile uint32_t max_inferences = 0;
static volatile uint32_t npu_inferences = 0;

void IRQ366_Handler(void)
{
    // clear irq with bit 1
    // (take care not to re-poke the run bit - this is how the Ethos driver does it)
    struct cmd_r cmd;
    cmd.word      = NPU->CMD.word & 0xC;
    cmd.clear_irq = 1;
    NPU->CMD.word = cmd.word;

    npu_inferences++;

    if (continue_npu) {
        // restart npu by settings Command Register bit 0 to 1 (Write 1 to transition the NPU to running state.)
        cmd.clear_irq = 0;
        // Need to call below line for the NPU to reset properly. If not called, NPU internal clock gating kicks in on typical case.
        // ARM case number 03512338
        NPU->QBASE.offset_LO = LocalToGlobal(npu_readonly_addr) + npu_register_QBASE_offset; // Address for Command-queue.
        cmd.transition_to_running_state = 1;
        NPU->CMD.word = cmd.word;
    } else {
        NPU->CMD.word = 0xC; // Set default value, enables off for clocks and power.

        if (NPU->STATUS.state == STATE_STOPPED) {
            npu_running = 0;
        }
    }
}

static int32_t set_global_attributes(void)
{
    const void *model_data_addr;
    size_t model_data_size;

#if ENABLE_NPU_KWS
    if (test_type > 0) {
        npu_workspace_size = 128*1024;
        reference_data_size = 12;
        reference_data_addr = kws_reference_output;
        model_data_addr = get_kws_u85_model();
        model_data_size = get_kws_u85_model_len();
        npu_register_QBASE_offset = get_kws_u85_qbase_offset();
        npu_command_length = get_kws_u85_qsize();
        npu_register_BASEP0_offset = get_kws_u85_basep0_offset();
        npu_output_offset = 0xbd0;

        /* copy model from MRAM to SRAM */
        switch(test_type) {
            case 1:
                memcpy((void *) kws_model_sram, model_data_addr, model_data_size);
                npu_readonly_addr = kws_model_sram;
                break;

            case 2:
                npu_readonly_addr = model_data_addr;
                break;

            case 3:
#if __HAS_HYPER_RAM
                memcpy((void *) kws_model_hram, model_data_addr, model_data_size);
                npu_readonly_addr = kws_model_hram;
#endif
                break;

            default:
                while(1);
                break;
        }

    }
    else
#endif
    {
        npu_workspace_size = TYPICAL_U85_REFERENCE_SIZE_IN_BYTES;
        reference_data_size = TYPICAL_U85_REFERENCE_SIZE_IN_BYTES;
        reference_data_addr = typical_u85_reference;
        model_data_addr = typical_u85_mem_map;
        model_data_size = TYPICAL_U85_MEMORYMAP_SIZE_IN_BYTES;
        npu_register_QBASE_offset = 0x0;
        npu_command_length = 0xE8;
        npu_register_BASEP0_offset = TYPICAL_U85_MEMORYMAP_BASEP0_offset;
        npu_output_offset = 0x2000;
        npu_readonly_addr = model_data_addr;
    }

    RTSS_CleanInvalidateDCache_by_Addr((void *) npu_readonly_addr, model_data_size);

    return 0;
}

static void fill_input_data(void)
{
#if ENABLE_NPU_KWS
    if (test_type) {
        memcpy((uint8_t *) npu_workspace_addr + 0x13d20, kws_reference_input, 49 * 10);
        // printf("fill input data done\n\r");
    }
#endif
}

#if ENABLE_NPU_VALIDATION == 1
static int32_t verify_npu_output()
{
    RTSS_InvalidateDCache_by_Addr((uint8_t *) npu_workspace_addr + npu_output_offset, reference_data_size);
    return memcmp((uint8_t *) npu_workspace_addr + npu_output_offset, reference_data_addr, reference_data_size);
}
#endif

static bool use_ext_port(const void *addr)
{
    // Accessible regions to U85 are main SRAM (0), OSPI (2,A,B,C,D), MRAM (8) and TCMs (5)
    // SRAM and TCMs need to use SRAM ports, MRAM and OSPI need to use EXT port
    // Quick-and-dirty check that gives the right answer for valid addresses:
    return LocalToGlobal(addr) & 0xA0000000;
}

static void stop_npu(void)
{
    continue_npu = 0;

    while (npu_running) __WFE();

    // Disable NPU interrupt
    NVIC_DisableIRQ(NPU_IRQn);
    NVIC_ClearPendingIRQ(NPU_IRQn);

    // Reset NPU
    NPU->RESET.word = npu_reset_config.word;

    // Wait until resetted
    while (NPU->STATUS.reset_status);

    // Set default value, enables off for clocks and power.
    NPU->CMD.word = (struct cmd_r) { .clock_q_enable = 1, .power_q_enable = 1 }.word;

    CGU->CLK_ENA &= ~(1U << 31);
}

static int32_t start_npu(void)
{
    uint64_t npu_runtime;
    int32_t ret = 0;
    struct regioncfg_r regioncfg = { .word = 0 };

    ret = set_global_attributes();
    if (ret) {
        return ret;
    }

#if ENABLE_NPU_VALIDATION == 1
    // Write random data to NPU output area so we can verify NPU output that it is not zero and matched to reference output.
    uint8_t* p = (uint8_t*) npu_workspace_addr;

    // initialize rand
    extern volatile uint32_t ms_ticks;
    srand(ms_ticks);

    for (uint32_t i = 0; i < npu_workspace_size; i++, p++) {
        *p = rand();
    }
    // printf("rand() done\n\r");
#endif

    fill_input_data();

    RTSS_CleanInvalidateDCache_by_Addr(npu_workspace_addr, npu_workspace_size);

    // Enable NPU interrupt
    NVIC_ClearPendingIRQ(NPU_IRQn);
    NVIC_EnableIRQ(NPU_IRQn);

    // Clock gating enable to Ethos-U85
    CGU->CLK_ENA |= (1U << 31);

    // Reset NPU
    NPU->RESET.word = npu_reset_config.word;

    // Wait until resetted
    while (NPU->STATUS.reset_status);

    struct mem_attr_r sram_attr = { .axi_port = AXI_PORT_SRAM, .memtype = AXI_MEM_ENCODING_NORMAL_NON_CACHEABLE_NON_BUFFERABLE };
    struct mem_attr_r ext_attr = { .axi_port = AXI_PORT_EXT, .memtype = AXI_MEM_ENCODING_NORMAL_NON_CACHEABLE_NON_BUFFERABLE };
    NPU->MEM_ATTR[0].word = sram_attr.word;
    NPU->MEM_ATTR[1].word = sram_attr.word;
    NPU->MEM_ATTR[2].word = ext_attr.word;
    NPU->MEM_ATTR[3].word = ext_attr.word;
    NPU->QBASE.offset_LO = LocalToGlobal(npu_readonly_addr) + npu_register_QBASE_offset; // Address for Command-queue.
    NPU->QCONFIG.word = (struct qconfig_r) { .cmd_region0 = use_ext_port(npu_readonly_addr) ? 2 : 0 }.word;
    NPU->QSIZE.QSIZE = npu_command_length; // Size of command stream in bytes
    NPU->BASEP[0].offset_LO = LocalToGlobal(npu_readonly_addr) + npu_register_BASEP0_offset; // Region-0 Address - Weights
    if (use_ext_port(npu_readonly_addr)) {
        regioncfg.region0 = 2;
    }
    if (test_type > 0) {
        NPU->BASEP[1].offset_LO = LocalToGlobal(npu_workspace_addr);                        // Region 1 = Arena
        NPU->BASEP[2].offset_LO = LocalToGlobal(npu_workspace_addr);                        // Region 2 = Scratch
        NPU->BASEP[3].offset_LO = LocalToGlobal(npu_workspace_addr) + 0x13D20;              // Region 3 = Input
        NPU->BASEP[4].offset_LO = LocalToGlobal(npu_workspace_addr) + 0xBD0;                // Region 4 = Output
        if (use_ext_port(npu_workspace_addr)) {
            regioncfg.region1 = regioncfg.region2 = regioncfg.region3 = regioncfg.region4 = 2;
        }
    } else {
        NPU->BASEP[1].offset_LO = LocalToGlobal(npu_workspace_addr);                        // Region-1 Address - Scratch Buffer
        NPU->BASEP[2].offset_LO = LocalToGlobal(npu_readonly_addr) + 0xF0;                  // Region-2 Address - Input Data Stream
        NPU->BASEP[3].offset_LO = LocalToGlobal(npu_workspace_addr) + npu_output_offset;    // Region-3 Address - Output Data Stream
        if (use_ext_port(npu_readonly_addr)) {
            regioncfg.region2 = 2;
        }
        if (use_ext_port(npu_workspace_addr)) {
            regioncfg.region3 = 2;
        }
    }
    NPU->REGIONCFG.word = regioncfg.word;

#if ENABLE_NPU_VALIDATION == 1
    continue_npu = 0;
    npu_running = 1;
    npu_inferences = 0;

    NPU->PMEVTYPER[0].word = PMU_EVENT_SRAM_RD_DATA_BEAT_RECEIVED;
    NPU->PMEVTYPER[1].word = PMU_EVENT_EXT_RD_DATA_BEAT_RECEIVED;
    NPU->PMEVTYPER[2].word = PMU_EVENT_NPU_ACTIVE;
    // Enable event counters 0, 1 & 2
    NPU->PMCNTENSET.word = (struct pmcntenset_r) { .EVENT_CNT_0 = 1, .EVENT_CNT_1 = 1, .EVENT_CNT_2 = 1 }.word;
    NPU->PMCR.word = (struct pmcr_r) { .cnt_en = 1, .event_cnt_rst = 1 }.word;

    // start NPU and do one inference
    NPU->CMD.word = (struct cmd_r) { .transition_to_running_state = 1 }.word;

    // printf("Waiting for one ETHOS-U85 interrupt.\n\r");
    while (npu_inferences == 0) __WFE();

    // Get the cycle count one interference
    uint32_t cycle_cnt_active = NPU->PMEVCNTR[2].count;
    printf("NPU: CYCLE_CNT_ACTIVE: %" PRIu32 "\n\r", cycle_cnt_active);

    // Validate that inference worked
    ret = verify_npu_output();
    // printf("NPU: verify: %" PRIi32 "\n\r", ret);
    if (ret) {
        return ret;
    }

    memset((uint8_t *)npu_workspace_addr + npu_output_offset, 0, reference_data_size);
    RTSS_CleanDCache_by_Addr(npu_workspace_addr + npu_output_offset, reference_data_size);

    printf("NPU: SRAM_RD_DATA_BEAT_RECEIVED: %" PRIu32 "\n\r", NPU->PMEVCNTR[0].count);
    printf("NPU: EXT_RD_DATA_BEAT_RECEIVED: %" PRIu32 "\n\r", NPU->PMEVCNTR[1].count);
    NPU->PMCR.word = (struct pmcr_r) { .cnt_en = 0, .event_cnt_rst = 1 }.word;

    // Need to call below line for the NPU to reset properly. If not called, NPU internal clock gating kicks in on typical case.
    // ARM case number 03512338
    NPU->QBASE.offset_LO = LocalToGlobal(npu_readonly_addr) + npu_register_QBASE_offset; // Address for Command-queue.
#endif

    // start NPU again and do inferences until stop command
    continue_npu = 1;
    npu_running = 1;
    npu_inferences = 0;

    if (test_type > 0) {
        npu_runtime = s32k_cntr_val64();
    }

    NPU->CMD.word = (struct cmd_r) { .transition_to_running_state = 1 }.word;
    while (npu_inferences < max_inferences) __WFE();
    stop_npu();

    if (test_type > 0) {
        npu_runtime = s32k_cntr_val64() - npu_runtime;
        npu_runtime = npu_runtime * 30.5 / (max_inferences + 1);
        printf("NPU: %" PRIu64 " usec per inf.\n\r", npu_runtime);
    }
    printf("\n");
    return ret;
}

void npuTestStartU85(uint32_t test_count, uint32_t test_case)
{
    test_type = test_case;
    max_inferences = test_count < 1 ? 0 : test_count - 1;
    int32_t ret = start_npu();
    if (ret != 0) {
        printf("NPU: start failed: %" PRIi32 "\n\r", ret);
        return;
    }
}
