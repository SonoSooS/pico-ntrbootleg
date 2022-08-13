/*
    neg.c - Nitro Emulated Gamecart glue logic
    Copyright (C) 2022 Sono (https://github.com/SonoSooS)
    All Rights Reserved
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include "pico/stdlib.h"
#include "pico/platform.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/iobank0.h"
#include "hardware/structs/padsbank0.h"

#include "memeloc.h"
#include "neg.h"
#include "png.h"
#include "picart_setup.h"


#define NPIN_MIN (2+0)
#define NPIN_MAX (2+12)

#define NLOC __scratch_x("neg")
#define NFUNC(f) MEMELOC_RAM_F(f)
#define DMALOC MEMELOC_BANK3_N("neg_d")


static PIO NLOC pio;
static int NLOC sm_proto;
static int NLOC sm_clk;
static int NLOC dma_from_pio;
static int NLOC dma_to_pio;
static int NLOC dma_to_pio_fill;

dma_channel_hw_t* zerodma;

//static uint8_t dmazero[PNGBUF_SIZE];
static uint8_t DMALOC dmazero[0x910 + ((0x200 + 0x18) * 8) + 4];

uint32_t DMALOC dmadstbuf[DMALOG_SIZE >> 2] __attribute__((aligned(DMALOG_SIZE)));
io_ro_32* dmadstptr;
//static uint32_t* NLOC dmasrcbuf;


io_ro_32* NFUNC(negGetPtrRead)(void)
{
    return &pio->rxf[sm_proto];
}

io_wo_32* NFUNC(negGetPtrWrite)(void)
{
    return &pio->txf[sm_proto];
}

bool NFUNC(negCanRead)(void)
{
    uint32_t tm = 1 << (PIO_FSTAT_RXEMPTY_LSB + sm_proto);
    return !(pio->fstat & tm);
}

bool NFUNC(negCanDrain)(void)
{
    uint32_t tm = 1 << (PIO_FSTAT_TXEMPTY_LSB + sm_proto);
    return !!(pio->fstat & tm);
}

bool NFUNC(negCanWrite)(void)
{
    uint32_t tm = 1 << (PIO_FSTAT_TXFULL_LSB + sm_proto);
    return !(pio->fstat & tm);
}

uint32_t NFUNC(negBlockingRead)(void)
{
    io_ro_32* fifo = negGetPtrRead();
    uint32_t tm = 1 << (PIO_FSTAT_RXEMPTY_LSB + sm_proto);
    while(pio->fstat & tm) tight_loop_contents();
    return *fifo;
}

void NFUNC(negBlockingWrite)(uint32_t data)
{
    io_wo_32* fifo = negGetPtrWrite();
    uint32_t tm = 1 << (PIO_FSTAT_TXFULL_LSB + sm_proto);
    while(pio->fstat & tm) tight_loop_contents();
    *fifo = data;
}

void NFUNC(negBlockingSkip)(uint32_t cnt)
{
    io_ro_32* fifo = negGetPtrRead();
    uint32_t tm = 1 << (PIO_FSTAT_RXEMPTY_LSB + sm_proto);
    
    while(cnt--)
    {
        while(pio->fstat & tm) tight_loop_contents();
        (void)*fifo;
    }
}

void NFUNC(negBlockingDummy)(uint32_t cnt, uint32_t data)
{
    io_wo_32* fifo = negGetPtrWrite();
    uint32_t tm = 1 << (PIO_FSTAT_TXFULL_LSB + sm_proto);
    
    while(cnt--)
    {
        while(pio->fstat & tm) tight_loop_contents();
        *fifo = data;
    }
}

void negInit(void)
{
    pio = pio0;
    sm_proto = pio_claim_unused_sm(pio, true);
    sm_clk = pio_claim_unused_sm(pio, true);
    
    dma_from_pio = dma_claim_unused_channel(true);
    dma_to_pio = dma_claim_unused_channel(true);
    dma_to_pio_fill = dma_claim_unused_channel(true);
    
    zerodma = dma_channel_hw_addr(dma_to_pio_fill);
    memset(&dmazero[0], 0, sizeof(dmazero));
    
    bus_ctrl_hw->priority = 0
        | BUSCTRL_BUS_PRIORITY_DMA_R_BITS
        | BUSCTRL_BUS_PRIORITY_DMA_W_BITS
        //| BUSCTRL_BUS_PRIORITY_PROC1_BITS
        //| BUSCTRL_BUS_PRIORITY_PROC0_BITS
        ; // DMA must take over! Also Core1 must take over Core0
    while(!(bus_ctrl_hw->priority_ack & BUSCTRL_BUS_PRIORITY_ACK_BITS))
        tight_loop_contents();
    
    //padsbank0_hw->voltage_select = PADS_BANK0_VOLTAGE_SELECT_VALUE_3V3 << PADS_BANK0_VOLTAGE_SELECT_LSB;
    
    for(uint i = NPIN_MIN; i != NPIN_MAX; ++i)
    {
        iobank0_hw->io[i].ctrl = IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PIO0_0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
        
        // Lowest drive, due to bugs, do not trip short detection
        // Data move must be decisive and fast for edge detect
        padsbank0_hw->io[i] = PADS_BANK0_GPIO0_RESET
            //& ~(PADS_BANK0_GPIO0_DRIVE_BITS)
            //& ~(PADS_BANK0_GPIO0_SLEWFAST_BITS)
            //| (PADS_BANK0_GPIO0_SLEWFAST_BITS)
            //| (PADS_BANK0_GPIO0_DRIVE_VALUE_2MA << PADS_BANK0_GPIO0_DRIVE_LSB)
            ;
    }
    
    picartSetup(pio, sm_proto, sm_clk);
}

static void negPIOReset(PIO pio)
{
    *hw_clear_alias(&pio->ctrl) = PIO_CTRL_SM_ENABLE_BITS;
    *hw_set_alias(&pio->ctrl) = PIO_CTRL_CLKDIV_RESTART_BITS | PIO_CTRL_SM_RESTART_BITS;
    *&pio->fdebug = PIO_FDEBUG_RXSTALL_BITS | PIO_FDEBUG_RXUNDER_BITS | PIO_FDEBUG_TXOVER_BITS | PIO_FDEBUG_TXSTALL_BITS;
}

static void negSMResetFull(PIO pio, int smid)
{
    pio_sm_hw_t* sm = &pio->sm[smid];
    
    sm->clkdiv = 1 << PIO_SM0_CLKDIV_INT_LSB;
    sm->execctrl = PIO_SM0_EXECCTRL_WRAP_TOP_RESET << PIO_SM0_EXECCTRL_WRAP_TOP_LSB;
    sm->shiftctrl = PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_BITS; // Enter from right, exit to right
    sm->pinctrl = PIO_SM0_PINCTRL_RESET;
    
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS;
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS;
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS;
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS;
}

static void negSMReset(PIO pio, int smid)
{
    pio_sm_hw_t* sm = &pio->sm[smid];
    
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS;
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS;
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS;
    *hw_xor_alias(&sm->shiftctrl) = PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS;
}

static void negDMAReset(int dmach)
{
    dma_channel_hw_t* dma = dma_channel_hw_addr(dmach);
    dma->al1_ctrl = DMA_CH0_CTRL_TRIG_AHB_ERROR_BITS | DMA_CH0_CTRL_TRIG_READ_ERROR_BITS | DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS;
    
    dma_channel_abort(dmach);
    
    dma->al1_ctrl = DMA_CH0_CTRL_TRIG_AHB_ERROR_BITS | DMA_CH0_CTRL_TRIG_READ_ERROR_BITS | DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS;
}

static void negDMAPostinit(void)
{
    dma_channel_hw_t* dma = dma_channel_hw_addr(dma_from_pio);
    
    dmadstptr = &dma->write_addr;
    
    dma->read_addr = (uint32_t)negGetPtrRead();
    dma->write_addr = (uint32_t)&dmadstbuf[0];
    dma->transfer_count = count_of(dmadstbuf);
    dma->ctrl_trig = 0
        | (DMA_CH0_CTRL_TRIG_EN_BITS)
        | (DMA_CH0_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_WORD << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB)
        | ((dma_from_pio) << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB)
        | (DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS)
        | (DMA_CH0_CTRL_TRIG_INCR_WRITE_BITS)
        //| (DMA_CH0_CTRL_TRIG_IRQ_QUIET_BITS) // We want to crash on FIFO read overrun
        //| (DMA_CH0_CTRL_TRIG_RING_SEL_BITS)
        //| ((15) << DMA_CH0_CTRL_TRIG_RING_SIZE_LSB) // (1 << 15)
        | ((DREQ_PIO0_RX0 + sm_proto) << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB)
        ;
}

void __noinline NFUNC(negDMAFillReset)(void)
{
    zerodma->write_addr = (uint32_t)negGetPtrWrite();
    zerodma->al1_ctrl = 0
        | (DMA_CH0_CTRL_TRIG_EN_BITS)
        | (DMA_CH0_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_BYTE << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB)
        | ((dma_to_pio_fill) << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB)
        | (DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS)
        | (DMA_CH0_CTRL_TRIG_INCR_READ_BITS) // No idea why it's broken without read increment
        | (DMA_CH0_CTRL_TRIG_IRQ_QUIET_BITS) // Do not crash after each fill
        | ((DREQ_PIO0_TX0 + sm_proto) << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB)
        ;
}

void __noinline NFUNC(negDMAFillFIFO)(uint32_t amount)
{
    zerodma->read_addr = (uint32_t)&dmazero[0];
    zerodma->al1_transfer_count_trig = amount;
}

void negResetFull(void)
{
    negPIOReset(pio);
    
    negDMAReset(dma_from_pio);
    negDMAReset(dma_to_pio);
    negDMAReset(dma_to_pio_fill);
    
    negSMResetFull(pio, sm_proto);
    negSMResetFull(pio, sm_clk);
    
    picartResetFull();
    
    negDMAFillReset();
}

void negReset(void)
{
    negPIOReset(pio);
    
    negDMAReset(dma_from_pio);
    negDMAReset(dma_to_pio);
    negDMAReset(dma_to_pio_fill);
    
    negSMReset(pio, sm_proto);
    negSMReset(pio, sm_clk);
    
    negDMAFillReset();
}

void negEnable(void)
{
    *hw_set_alias(&pio->ctrl) = (1 << sm_proto) | (1 << sm_clk);
    picartPreinit();
    
    negDMAPostinit();
}
