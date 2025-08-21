#include <RTE_Components.h>
#include CMSIS_device_header
#include "drv_mhu.h"

static MHU_Callback_t user_rx_cb;
static MHU_Callback_t user_tx_cb;

/* Set bits in the sending channel to generate an interrupt */
void MHU_SENDER_Set(uint32_t target, uint32_t channel, uint32_t value)
{
    MHU_SENDER_regs *MHU = (MHU_SENDER_regs *) target;
    if (value == 0) return;
    if (channel < (MHU->MHU_CFG & 0x7FU)) {
        MHU->ACCESS_REQUEST = 1;
        while(MHU->ACCESS_READY == 0);
        MHU->SEND_CHANNELS[channel].CH_SET = value;
        MHU->ACCESS_REQUEST = 0;
    }
}

/* Check if bits in the sending channel were cleared by the receiver */
void MHU_SENDER_Check(uint32_t target, uint32_t channel, uint32_t *value)
{
    MHU_SENDER_regs *MHU = (MHU_SENDER_regs *) target;
    if (channel < (MHU->MHU_CFG & 0x7FU)) {
        *value = MHU->SEND_CHANNELS[channel].CH_ST;
    }
}

/* Read bits in the receiving channel */
void MHU_RECEIVER_Read(uint32_t source, uint32_t channel, uint32_t *value)
{
    MHU_RECEIVER_regs *MHU = (MHU_RECEIVER_regs *) source;
    if (channel < (MHU->MHU_CFG & 0x7FU)) {
        *value = MHU->RECV_CHANNELS[channel].CH_ST;
    }
}

/* Clear bits in the receiving channel */
void MHU_RECEIVER_Clear(uint32_t source, uint32_t channel, uint32_t value)
{
    MHU_RECEIVER_regs *MHU = (MHU_RECEIVER_regs *) source;
    if (value == 0) return;
    if (channel < (MHU->MHU_CFG & 0x7FU)) {
        MHU->RECV_CHANNELS[channel].CH_CLR = value;
    }
}

void MHU_RTSS_S_TX_IRQHandler()
{
    MHU_SENDER_regs *MHU = (MHU_SENDER_regs *) RTSS_TX_MHU0_BASE;
    uint32_t int_st = MHU->INT_CLR = MHU->INT_ST;

    /* MHU Channel Interrupt */
    if ((int_st & 4) && (user_tx_cb)) {
        user_tx_cb();
    }
}

void MHU_RTSS_S_RX_IRQHandler()
{
    MHU_RECEIVER_regs *MHU = (MHU_RECEIVER_regs *) RTSS_RX_MHU0_BASE;
    uint32_t int_st = MHU->INT_CLR = MHU->INT_ST;

    /* MHU Channel Interrupt */
    if ((int_st & 4) && (user_rx_cb)) {
        user_rx_cb();
    }
}

void MHU_register_cb(MHU_Callback_t rx_cb, MHU_Callback_t tx_cb)
{
    NVIC_DisableIRQ(41);
    NVIC_DisableIRQ(42);
    NVIC_ClearPendingIRQ(41);
    NVIC_ClearPendingIRQ(42);

    if (rx_cb) {
        user_rx_cb = rx_cb;
        NVIC_EnableIRQ(41);
    }

    if (tx_cb) {
        user_tx_cb = tx_cb;
        NVIC_EnableIRQ(42);
    }
}
