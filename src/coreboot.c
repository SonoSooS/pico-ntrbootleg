//#define COREBOOT_DBG

#include <stdint.h>
#ifdef COREBOOT_DBG
#include <stdio.h>
#endif
#include "pico/platform.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/structs/xip_ctrl.h"


//extern int __StackOneTop;

extern int __memebank2_start__;
extern int __memebank2_end__;
extern int __memebank2_source__;
extern int __memebank3_start__;
extern int __memebank3_end__;
extern int __memebank3_source__;

static __attribute__((noinline)) void data_cpy(int dummy, int* src, int* dst, int* end)
{
    if((uintptr_t)src >= 0x20000000 && (uintptr_t)src < 0x21040000)
        return;
    
    end = (typeof(end))(((uintptr_t)end)+3);
    
    while(!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY_BITS))
        (void)xip_ctrl_hw->stream_fifo;
    
    uint32_t cnt = (((uintptr_t)end) - ((uintptr_t)dst)) >> 2;
    if(!cnt)
        return;
    
    xip_ctrl_hw->stream_addr = (uintptr_t)src;
    xip_ctrl_hw->stream_ctr = cnt;
    
    do
    {
        while(__builtin_expect(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY_BITS, false))
            tight_loop_contents();
        
        *(dst++) = xip_ctrl_hw->stream_fifo;
    }
    while(__builtin_expect(--cnt, true));
    
    /*
    while(dst < end)
        *(dst++) = *(src++);
    */
}

void coreboot(void(*pfn)(void))
{
    bool isirq = irq_is_enabled(SIO_IRQ_PROC0);
    irq_set_enabled(SIO_IRQ_PROC0, false);
    
#if !defined(PICO_NO_FLASH) || !PICO_NO_FLASH
    #ifdef COREBOOT_DBG
    puts("+ data_cpy");
    #endif
    
    data_cpy(0, &__memebank2_source__, &__memebank2_start__, &__memebank2_end__);
    data_cpy(0, &__memebank3_source__, &__memebank3_start__, &__memebank3_end__);
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
