#include <RTE_Components.h>
#include CMSIS_device_header
#include "ethosu55_interface.h"
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

extern const uint32_t TYPICAL_REFERENCE_SIZE;
extern const uint32_t TYPICAL_MEMORYMAP_SIZE;
extern const uint32_t TYPICAL_MEMORYMAP_BASEP0_offset;
extern const uint32_t typical_mem_map[];
extern const uint32_t typical_reference[];
#define TYPICAL_MEMORYMAP_SIZE_IN_BYTES (TYPICAL_MEMORYMAP_SIZE * 4)
#define TYPICAL_REFERENCE_SIZE_IN_BYTES (TYPICAL_REFERENCE_SIZE * 4)

#if ENABLE_NPU_KWS
extern const void *get_kws_model(void);
extern size_t get_kws_model_len(void);
extern size_t get_kws_qsize(void);
extern size_t get_kws_qbase_offset(void);
extern size_t get_kws_basep0_offset(void);
extern const uint8_t kws_reference_input[];
extern const uint8_t kws_reference_output[];
static const uint8_t kws_model_tcm[157600] __attribute__((aligned(16), section(".bss.kws_model_tcm")));
#if SOC_FEAT_HAS_BULK_SRAM
static const uint8_t kws_model_sram[157600] __attribute__((aligned(16), section(".bss.kws_model_sram")));
#endif
#endif

#define NPU_BASE_ADDRESS                NPULOCAL_BASE
#define NPU                             ((volatile struct NPU_REG *) NPULOCAL_BASE)

#if defined(CORE_M55_HE) || defined(M55_HE) || defined(M55_HE_E1C)
#define M55_CFG_BASE_ADDRESS            M55HE_CFG_BASE
#define NPU_IRQn                        NPU_HE_IRQ_IRQn
#elif defined(CORE_M55_HP) || defined(M55_HP)
#define M55_CFG_BASE_ADDRESS            M55HP_CFG_BASE
#define NPU_IRQn                        NPU_HP_IRQ_IRQn
#else
#error "NPU only usable with HE or HP core"
#endif

static const struct reset_r npu_reset_config = { .pending_CPL = PRIVILEGE_LEVEL_USER, .pending_CSL = SECURITY_LEVEL_SECURE };

static const void *npu_readonly_addr;
static const void *reference_data_addr;
static uint32_t reference_data_size;
// static void *npu_workspace_addr; /* NPU workspace always in DTCM */
static uint32_t npu_workspace_addr[32768] __attribute__((aligned(16), section(".bss.noinit")));
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

#if defined(CORE_M55_HE) || defined(M55_HE) || defined(M55_HE_E1C)
void NPU_HE_IRQHandler(void)
#elif defined(CORE_M55_HP) || defined(M55_HP)
void NPU_HP_IRQHandler(void)
#endif
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
        model_data_addr = get_kws_model();
        model_data_size = get_kws_model_len();
        npu_register_QBASE_offset = get_kws_qbase_offset();
        npu_command_length = get_kws_qsize();
        npu_register_BASEP0_offset = get_kws_basep0_offset();
        npu_output_offset = 0xd0;

        /* copy model from MRAM to TCM or SRAM */
        switch(test_type) {
            case 1:
                memcpy((void *) kws_model_tcm, model_data_addr, model_data_size);
                npu_readonly_addr = kws_model_tcm;
                break;

#if SOC_FEAT_HAS_BULK_SRAM
            case 2:
                memcpy((void *) kws_model_sram, model_data_addr, model_data_size);
                npu_readonly_addr = kws_model_sram;
                break;
#endif

            case 3:
                npu_readonly_addr = model_data_addr;
                break;

            default:
                while(1);
                break;
        }
    }
    else
#endif
    {
        npu_workspace_size = TYPICAL_REFERENCE_SIZE_IN_BYTES;
        reference_data_size = TYPICAL_REFERENCE_SIZE_IN_BYTES;
        reference_data_addr = typical_reference;
        model_data_addr = typical_mem_map;
        model_data_size = TYPICAL_MEMORYMAP_SIZE_IN_BYTES;
        npu_register_QBASE_offset = 0x1cb0;
        npu_command_length = 0xec;
        npu_register_BASEP0_offset = TYPICAL_MEMORYMAP_BASEP0_offset;
        npu_output_offset = 0;
        npu_readonly_addr = model_data_addr;
    }

    RTSS_CleanInvalidateDCache_by_Addr((void *) npu_readonly_addr, model_data_size);

    return 0;
}

static void fill_input_data(void)
{
#if ENABLE_NPU_KWS
    if (test_type > 0) {
        memcpy((uint8_t *) npu_workspace_addr + 0x113a0, kws_reference_input, 49 * 10);
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

    M55LOCAL_CFG->CLK_ENA &= ~(1U);
}

static int32_t start_npu(void)
{
    uint64_t npu_runtime;
    int32_t ret = 0;

    ret = set_global_attributes();
    if (ret) {
        return ret;
    }

#if ENABLE_NPU_VALIDATION == 1
    // Write random data to HP and HE NPU output area so we can verify NPU output that it is not zero and matched to reference output.
    uint8_t* p = (uint8_t*)npu_workspace_addr;

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

    // Clock gating enable to Ethos-U55
    M55LOCAL_CFG->CLK_ENA |= 1;

    // Reset NPU
    NPU->RESET.word = npu_reset_config.word;

    // Wait until resetted
    while(NPU->STATUS.reset_status == 1);

    NPU->QCONFIG.word = (struct qconfig_r) { .cmd_region0 = MEM_ATTR_AXI0_OUTSTANDING_COUNTER0 }.word;
    NPU->REGIONCFG.word = 0x00000000; // all regions using MEM_ATTR_AXI0_OUTSTANDING_COUNTER0
    NPU->AXI_LIMIT0.word = (struct axi_limit0_r) { .max_beats = MAX_BEATS_B128, .memtype = AXI_MEM_ENCODING_NORMAL_NON_CACHEABLE_NON_BUFFERABLE, .max_outstanding_read_m1 = 31, .max_outstanding_write_m1 = 15 }.word;
    NPU->AXI_LIMIT1.word = (struct axi_limit1_r) { .max_beats = MAX_BEATS_B128, .memtype = AXI_MEM_ENCODING_NORMAL_NON_CACHEABLE_NON_BUFFERABLE, .max_outstanding_read_m1 = 31, .max_outstanding_write_m1 = 15 }.word;
    NPU->AXI_LIMIT2.word = (struct axi_limit2_r) { .max_beats = MAX_BEATS_B128, .memtype = AXI_MEM_ENCODING_NORMAL_NON_CACHEABLE_NON_BUFFERABLE, .max_outstanding_read_m1 = 31, .max_outstanding_write_m1 = 15 }.word;
    NPU->AXI_LIMIT3.word = (struct axi_limit3_r) { .max_beats = MAX_BEATS_B128, .memtype = AXI_MEM_ENCODING_NORMAL_NON_CACHEABLE_NON_BUFFERABLE, .max_outstanding_read_m1 = 31, .max_outstanding_write_m1 = 15 }.word;
    NPU->QBASE.offset = LocalToGlobal(npu_readonly_addr) + npu_register_QBASE_offset; // Address for Command-queue.
    NPU->BASEP[0].offset = LocalToGlobal(npu_readonly_addr) + npu_register_BASEP0_offset; // Region 0 = Weights
    NPU->QSIZE.QSIZE = npu_command_length; // Size of command stream in bytes
    if (test_type > 0) {
        NPU->BASEP[1].offset = LocalToGlobal(npu_workspace_addr); // Region 1 = Arena
        NPU->BASEP[2].offset = LocalToGlobal(npu_workspace_addr); // Region 2 = Scratch
        NPU->BASEP[3].offset = LocalToGlobal(npu_workspace_addr) + 0x113a0; // Region 3 = Input
        NPU->BASEP[4].offset = LocalToGlobal(npu_workspace_addr) + 0xd0; // Region 4 = Output
    } else {
        NPU->BASEP[1].offset = 0x00000000;
        NPU->BASEP[2].offset = LocalToGlobal(npu_readonly_addr) + 0x1da0; // Region 2 = Input
        NPU->BASEP[3].offset = LocalToGlobal(npu_workspace_addr); // Region 3 = Output
    }

#if ENABLE_NPU_VALIDATION == 1
    continue_npu = 0;
    npu_running = 1;
    npu_inferences = 0;

    NPU->PMEVTYPER[0].word = PMU_EVENT_AXI0_RD_DATA_BEAT_RECEIVED;
    NPU->PMEVTYPER[1].word = PMU_EVENT_AXI1_RD_DATA_BEAT_RECEIVED;
    NPU->PMEVTYPER[2].word = PMU_EVENT_NPU_ACTIVE;
    // Enable event counters 0, 1 & 2
    NPU->PMCNTENSET.word = (struct pmcntenset_r) {.EVENT_CNT_0 = 1, .EVENT_CNT_1 = 1, .EVENT_CNT_2 = 1}.word;
    NPU->PMCR.word = (struct pmcr_r) { .cnt_en = 1, .event_cnt_rst = 1 }.word;

    // start NPU and do one inference
    NPU->CMD.word = (struct cmd_r) { .transition_to_running_state = 1 }.word;

    // printf("Waiting for one ETHOS-U55 interrupt.\n\r");
    while (npu_inferences == 0) __WFE();

    // Get the cycle count one interference
    uint32_t cycle_cnt_active = NPU->PMEVCNTR[2].count;
    printf("NPU: CYCLE_CNT_ACTIVE: %" PRIu32 "\n\r", cycle_cnt_active);

    // Validate that interference worked
    ret = verify_npu_output();
    // printf("NPU: verify: %" PRIi32 "\n\r", ret);
    if (ret) {
        return ret;
    }

    memset((uint8_t *)npu_workspace_addr + npu_output_offset, 0, reference_data_size);
    RTSS_CleanDCache_by_Addr(npu_workspace_addr + npu_output_offset, reference_data_size);

    printf("NPU: AXI0_RD_DATA_BEAT_RECEIVED: %" PRIu32 "\n\r", NPU->PMEVCNTR[0].count);
    printf("NPU: AXI1_RD_DATA_BEAT_RECEIVED: %" PRIu32 "\n\r", NPU->PMEVCNTR[1].count);
    NPU->PMCR.word = (struct pmcr_r) { .cnt_en = 0, .event_cnt_rst = 1 }.word;
#endif

    // start NPU again and do interferences until stop command
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

void npuTestStartU55(uint32_t test_count, uint32_t test_case)
{
    test_type = test_case;
    max_inferences = test_count < 1 ? 0 : test_count - 1;
    int32_t ret = start_npu();
    if (ret != 0) {
        printf("NPU: start failed: %" PRIi32 "\n\r", ret);
        return;
    }
}
