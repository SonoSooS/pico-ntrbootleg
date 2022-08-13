//#define COREBOOT_DBG

#include <stdint.h>
#ifdef COREBOOT_DBG
#include <stdio.h>
#endif
#include "pico/platform.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/regs/dreq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/structs/dma.h"


//extern int __StackOneTop;

extern int __memebank2_start__;
extern int __memebank2_end__;
extern int __memebank2_source__;
extern int __memebank3_start__;
extern int __memebank3_end__;
extern int __memebank3_source__;
extern int __memedata_start__;
extern int __memedata_end__;
extern int __memedata_source__;

static __attribute__((noinline)) void data_cpy(int dummy, int* src, int* dst, int* end)
{
    if((uintptr_t)src < 0x10000000 || (uintptr_t)src >= 0x14000000)
        return;
    
    end = (typeof(end))(((uintptr_t)end)+3);
    
    uint32_t cnt = (((uintptr_t)end) - ((uintptr_t)dst)) >> 2;
    if(!cnt)
        return;
    
    if(cnt >= (128 >> 2))
    {
        while(!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY_BITS))
            (void)xip_ctrl_hw->stream_fifo;
        
        dma_channel_hw_t* dma = &dma_hw->ch[0];
        dma->read_addr = (uintptr_t)&xip_ctrl_hw->stream_fifo;
        dma->write_addr = (uintptr_t)dst;
        dma->transfer_count = cnt;
        dma->ctrl_trig = 0
            | (DMA_CH0_CTRL_TRIG_EN_BITS)
            | (DMA_CH0_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_WORD << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB)
            | ((0) << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB)
            | (DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS)
            | (DMA_CH0_CTRL_TRIG_INCR_WRITE_BITS)
            | (DMA_CH0_CTRL_TRIG_IRQ_QUIET_BITS)
            | ((DREQ_XIP_STREAM) << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB)
            ;
        
        xip_ctrl_hw->stream_addr = (uintptr_t)src;
        xip_ctrl_hw->stream_ctr = cnt;
        
        // There are surely better ways to do this
        //while(__builtin_expect(dma->al1_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS, true))
        while(__builtin_expect(dma->transfer_count, true))
            tight_loop_contents();
        
        /*
        do
        {
            while(__builtin_expect(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY_BITS, false))
                tight_loop_contents();
            
            *(dst++) = xip_ctrl_hw->stream_fifo;
        }
        while(__builtin_expect(--cnt, true));
        */
    }
    else
    {
        int dat;
        asm volatile(
            "n_loop:"
            "LDMIA %[src]!, {%[datp]}\n\t"
            "STMIA %[dst]!, {%[datp]}\n\t"
            "CMP %[endp], %[dst]\n\t"
            "BHI n_loop"
            : [datp]"=&r"(dat), [src]"+r"(src), [dst]"+r"(dst)
            : [endp]"r"(end)
            : "memory", "cc");
        
        // gcc being garbage
        /*
        volatile int* dst_ = dst;
        
        while(dst_ < end)
            *(dst_++) = *(src++);
        */
    }
}

void coreboot(void(*pfn)(void))
{
    bool isirq = irq_is_enabled(SIO_IRQ_PROC0);
    irq_set_enabled(SIO_IRQ_PROC0, false);
    
#if !defined(PICO_NO_FLASH) || !PICO_NO_FLASH
    #ifdef COREBOOT_DBG
    puts("+ data_cpy");
    #endif
    
    // Give way to DMA during init!
    bus_ctrl_hw->priority = 0
        | BUSCTRL_BUS_PRIORITY_DMA_R_BITS
        | BUSCTRL_BUS_PRIORITY_DMA_W_BITS
        ;
    //while(!(bus_ctrl_hw->priority_ack & BUSCTRL_BUS_PRIORITY_ACK_BITS))
    //    tight_loop_contents();
    
    data_cpy(0, &__memebank2_source__, &__memebank2_start__, &__memebank2_end__);
    data_cpy(0, &__memebank3_source__, &__memebank3_start__, &__memebank3_end__);
    
    data_cpy(0, &__memedata_source__, &__memedata_start__, &__memedata_end__);
#else
    #ifdef COREBOOT_DBG
    puts("+ no cpy (NO FLASH)");
    #endif
#endif
    
    #ifdef COREBOOT_DBG
    puts("+ bootseq");
    #endif
    
    //const uint32_t cmd_sequence[] = {0, 0, 1, (uintptr_t)(*(io_ro_32*)0xE000ED08), (uintptr_t)&__StackOneTop, ((uintptr_t)pfn) | 1};
    const uint32_t cmd_sequence[] = {0, 0, 1, (uintptr_t)(*(io_ro_32*)0xE000ED08), (uintptr_t)0x20041800, ((uintptr_t)pfn) | 1};
    uint seq = 0;
    do
    {
        uint cmd = cmd_sequence[seq];
        if(!cmd)
        {
            #ifdef COREBOOT_DBG
            puts("+ - drain");
            #endif
            
            multicore_fifo_drain();
            __sev();
        }
        
        #ifdef COREBOOT_DBG
        printf("+ - push %08X\n", cmd);
        #endif
        
        multicore_fifo_push_blocking(cmd);
        
        uint32_t response = multicore_fifo_pop_blocking();
        
        #ifdef COREBOOT_DBG
        printf("+ - pop %08X\n", response);
        #endif
        
        if(cmd == response)
            ++seq;
        else
            seq = 0;
    }
    while(seq < count_of(cmd_sequence));
    
    #ifdef COREBOOT_DBG
    puts("+ booted?");
    #endif
    
    irq_set_enabled(SIO_IRQ_PROC0, isirq);
}
