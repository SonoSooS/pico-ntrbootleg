/*
    main.c - Nitro Emulated Gamecart sequencer
    Copyright (C) 2022 Sono (https://github.com/SonoSooS)
    All Rights Reserved
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/sync.h"
#include "hardware/exception.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/structs/sio.h"
#include "hardware/gpio.h"

#include "memeloc.h"

#include "neg.h"
#include "bf.h"
#include "png.h"

#define ENSURE_MODE(f) MEMELOC_RAM_F(f)
//#define ENSURE_MODE(f) __noinline MEMELOC_RAM_F(f)
//#define ENSURE_MODE(f) __not_in_flash_func(f)
//#define ENSURE_MODE(f) __no_inline_not_in_flash_func(f)
#define ENSURE_MODE_NOINLINE(f) __noinline MEMELOC_RAM_F(f)
//#define ENSURE_MODE_NOINLINE(f) __no_inline_not_in_flash_func(f)


static uint32_t readhead;
static uint32_t ensurehead;
static volatile int32_t png_lastsize;

void main1(void);

static bool ENSURE_MODE(can)(int32_t count)
{
    int32_t currpos = (*dmadstptr & DMALOG_BITS) >> 2;
    return (currpos - readhead) >= count;
}

static bool ENSURE_MODE(is)(void)
{
    return can(1);
}

static void ENSURE_MODE_NOINLINE(ensure)(int32_t count)
{
    ensurehead = readhead;
    int32_t until = readhead + count;
    for(;;)
    {
        int32_t currpos = (*dmadstptr & DMALOG_BITS) >> 2;
        if(currpos < until)
            continue;
        
        if(currpos > readhead)
            break;
        
        break;
    }
    
    readhead = until;
}

static uint32_t ENSURE_MODE(ensure1)(void)
{
    ensure(1);
    return dmadstbuf[ensurehead];
}

static void bfTest(uint32_t cmd_hi, uint32_t cmd_lo)
{
    uint32_t dec_hi = cmd_hi;
    uint32_t dec_lo = cmd_lo;
    
    bfDecrypt(&dec_hi, &dec_lo);
    
    printf("- TEST: %08X %08X | %08X %08X\n", cmd_hi, cmd_lo, dec_hi, dec_lo);
}

static void bfTests(void)
{
    puts("Doing Blowfish tests!");
    
    bfTest(0xD1159B87, 0x34726BB6);
    bfTest(0xFF3945E0, 0xEA2F7482);
    bfTest(0xCB925087, 0xE6F5E201);
    puts("");
    
    bfTest(0x5685C4A2, 0xED5C6690);
    bfTest(0xFD53D199, 0xA37E7D19);
    bfTest(0x7D2FFEA5, 0xCAC1FE71);
    bfTest(0xFD2CE53F, 0xEA7DD457);
    bfTest(0x8CE6C3E3, 0xAF485482);
    //bfTest();
    
    puts("Blowfish test end.");
}

static void pngTest(void)
{
    pngInit(0xBE5D0F, 0xE8);
    pngPreinit();
    
    for(uint32_t i = 0; i < 16; i++)
    {
        printf("%5i: %02X\n", i, pngDo());
    }
}

#define main1_fifo_pop multicore_fifo_pop_blocking
#define main1_fifo_push multicore_fifo_push_blocking

/*
uint32_t MEMELOC_BANK2_F(main1_fifo_pop)(void)
{
    sio_hw_t* sio = sio_hw;
    while(!(sio->fifo_st & SIO_FIFO_ST_VLD_BITS))
        __wfe();
    return sio->fifo_rd;
}

void MEMELOC_BANK2_F(main1_fifo_push)(uint32_t data)
{
    sio_hw_t* sio = sio_hw;
    while(!(sio->fifo_st & SIO_FIFO_ST_RDY_BITS))
        tight_loop_contents();
    sio->fifo_wr = data;
    __sev();
}
*/

void MEMELOC_RAM_F(negDoHighperf)(void)
{
    uint32_t irq = save_and_disable_interrupts();
    
    ensure(2); // Read dummy 3C begin and end
    
    ensure(2);
    uint32_t cmd_hi = dmadstbuf[ensurehead + 0]; // Read 4x cmd hi
    uint32_t cmd_lo = dmadstbuf[ensurehead + 1]; // Read 4x cmd lo
    
    uint32_t dec_hi = cmd_hi;
    uint32_t dec_lo = cmd_lo;
    bfDecrypt(&dec_hi, &dec_lo);
    
    if(can(2))
    {
        puts("Too late to decrypt cmd!");
        
        printf("ENC: %08X %08X\n", cmd_hi, cmd_lo);
        printf("DEC: %08X %08X\n", dec_hi, dec_lo);
    }
    else
    {
        uint32_t seed = ((dec_lo >> (32 - 12)) | (dec_hi << 12)) & 0xFFFFFF;
        multicore_fifo_push_blocking(seed); // Kick off KEY2 stream init
        png_prog = 0;
        multicore_fifo_push_blocking(0x910 + 4); // Kick off KEY2 stream gen
        
        if(png_prog < (0x910 + 4))
        {
            while(png_prog < (0x910 + 4)) // Wait to reach KEY2 stream
                tight_loop_contents();
        }
        else
        {
            puts("Bad tight timing...");
        }
        
        uint32_t key2 = png_buf[0x910 >> 2];
        
        if(!can(2)) // KEY2 enable command still active(good)
        {
            ensure(2); // Read start and end flag
            
            negDMAFillFIFO(0x910);
            while((zerodma->al1_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS) && zerodma->transfer_count)
                tight_loop_contents();
            
            //puts("Pushing data");
            
            //negBlockingDummy(0x910, 0);
            negBlockingWrite(key2);
            negBlockingWrite(key2 >> 8);
            negBlockingWrite(key2 >> 16);
            negBlockingWrite(key2 >> 24);
            
            
            ensure(4); // Wait for sCardID query cmd to finish
            
            negDMAFillFIFO(0x910);
            multicore_fifo_push_blocking(0x910); // Restart HiZ area keygen
            
            
            if(can(1))
            {
                puts("Key init done, but too late");
            }
            else
            {
                //printf("Key init done 0x%06X\n", seed);
                
                //ensure(4);
            }
            
            // ???
            
            //negBlockingRead(); // sChipID read cmd hi
            //negBlockingRead(); // sChipID read cmd lo
            //negBlockingRead(); // sChipID read cmd begin
            //negBlockingRead(); // sChipID read cmd end
            
            //puts("FUCK YEAH!");
        }
        else
        {
            puts("Too late to write!");
        }
        
        //printf("ENC: %08X %08X\n", cmd_hi, cmd_lo);
        //printf("DEC: %08X %08X\n", dec_hi, dec_lo);
    }
    
    restore_interrupts(irq);
}


static uint8_t MEMELOC_RAM_N("main") firmbuf[0x1000];

uint32_t MEMELOC_RAM_F(negDoKeyCmd)(uint32_t cmd_hi, uint32_t cmd_lo, uint32_t bad)
{
    uint32_t irq = save_and_disable_interrupts();
    
    uint32_t raw_cmd = cmd_hi >> 28;
    
    if(raw_cmd == 2)
    {
        while(png_prog < 0x910)
            tight_loop_contents();
        
        png_prog = 0;
        int32_t datasize = (0x200 * 8) + (0x18 * (8 - 1));
        multicore_fifo_push_blocking(datasize);
        
        const uint8_t* bufptr = ((const uint8_t*)png_buf);
        uint32_t sect = (cmd_hi >> 12) & 0xFFFF;
        
        if(!zerodma->transfer_count)
        {
            puts("DMA too slow");
        }
        else
        {
            //puts("+ Waiting for DMA");
            //while(zerodma->al1_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS)
            //    tight_loop_contents();
            
            while((zerodma->al1_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS) && zerodma->transfer_count)
                tight_loop_contents();
            
            //puts("+ DMA done");
        }
        
        if(!bad)
        {
            if(__builtin_expect(sect == 7, true))
            //if(0)
            {
                const uint8_t* xorptr = ((const uint8_t*)firmbuf);
                
                for(uint32_t j = 0; j != 8; j++)
                {
                    for(uint32_t i = 0; i != 0x200; i++)
                    {
                        if(__builtin_expect(png_prog >= i, true))
                        {
                            uint8_t data = (*bufptr) ^ (*xorptr);
                            negBlockingWrite(data);
                            ++bufptr;
                            ++xorptr;
                            continue;
                        }
                        
                        printf("KEYGEN TOO SLOW @ %X\n", i);
                        goto fastend;
                    }
                    
                    if(__builtin_expect(j != 7, true))
                    {
                        for(uint32_t i = 0; i != 0x18; i++)
                        {
                            negBlockingWrite(*bufptr);
                            ++bufptr;
                        }
                    }
                }
            }
            else
            {
                for(uint32_t i = 0; i != datasize; ++i)
                {
                    if(__builtin_expect(png_prog >= i, true))
                    {
                        negBlockingWrite(*bufptr);
                            ++bufptr;
                        continue;
                    }
                    
                    printf("Keygen is too slow, oops @ %X\n", i);
                    goto fastend;
                }
            }
            
            goto fastend;
        }
    }
    else
    {
        printf("Unknown cmd: %1X\n", raw_cmd);
    }
    
    if(__builtin_expect(png_prog < png_lastsize, false))
    {
        printf("- Unfinished KEY2 stream! %X/%X\n", png_prog, png_lastsize);
        
        while(png_prog < png_lastsize)
            tight_loop_contents();
        
        puts("- KEY2 stream done");
    }
    
    if(__builtin_expect(zerodma->al1_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS, false))
    {
        printf("- Unfinished fill DMA! rem=%X\n", zerodma->transfer_count);
        while(zerodma->al1_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS)
            tight_loop_contents();
        
        puts("- DMA fill done");
    }

fastend:
    restore_interrupts(irq);
    
    ensure(2);
    
    if(!can(1))
    {
        negDMAFillFIFO(0x910);
        bad = 0;
        if(can(2))
            bad |= 4;
        
        multicore_fifo_push_blocking(0x910);
        
    }
    else
    {
        bad = 0;
        
        bad |= 2;
        
        if(!negCanDrain())
            bad |= 1;
    }
    
    return bad;
}

void MEMELOC_RAM_F(sMainLoop)(void)
{
    bool had_KEY2 = false;
    
    uint32_t bad = 0;
    
    //uint32_t irq = save_and_disable_interrupts();
    
    for(;;)
    {
        ensure(2);
        uint32_t cmd_hi = dmadstbuf[ensurehead + 0];
        uint32_t cmd_lo = dmadstbuf[ensurehead + 1];
        
        if(had_KEY2)
        {
            uint32_t dec_hi = cmd_hi;
            uint32_t dec_lo = cmd_lo;
            bfDecrypt(&dec_hi, &dec_lo);
            bad = negDoKeyCmd(dec_hi, dec_lo, bad);
            
            
            //ensure(1);
            //ensure(2);
            
            if(!bad)
            {
                //negDMAFillFIFO(0x910);
                //multicore_fifo_push_blocking(0x910);
            }
            else
            {
                if(bad & 1)
                {
                    puts("! Write FIFO is bad");
                }
                if(bad & 2)
                {
                    puts("! Processing too slow");
                }
                if(bad & 4)
                {
                    puts("! Risky bullshit too late");
                }
                
                printf("KC2: %08X %08X\n", dec_hi, dec_lo);
            }
            
            
            //printf("KC2: %08X %08X\n", dec_hi, dec_lo);
            //printf("CMD: %08X %08X\n", cmd_hi, cmd_lo);
            //ensure(1);
            
            //printf("db1: %08X\n", ensure1());
            //printf("db2: %08X\n", ensure1());
            //ensure(2);
            continue;
        }
        else if((cmd_hi >> 24) == 0x3C)
        {
            negDoHighperf();
            had_KEY2 = true;
            //restore_interrupts(irq);
            continue;
        }
        
        //printf("CMD: %08X %08X\n", cmd_hi, cmd_lo);
        //printf("es1: %08X\n", ensure1());
        //printf("es2: %08X\n", ensure1());
        ensure(2);
    }
}

void coreboot(void(*pfn)(void));

static __attribute__((noinline)) void faulthandler_real(uint32_t* extdata)
{
    printf("HARDFAULT Core%u\n", *(io_ro_32*)0xD0000000);
    printf("r0: %08X\n", extdata[0]);
    printf("r1: %08X\n", extdata[1]);
    printf("r2: %08X\n", extdata[2]);
    printf("r3: %08X\n", extdata[3]);
    printf("12: %08X\n", extdata[4]);
    printf("LR: %08X\n", extdata[5]);
    printf("PC: %08X\n", extdata[6]);
    printf("CP: %08X\n", extdata[7]);
    puts("yeah... well crap");
    
    for(;;)
    {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(200);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
        sleep_ms(200);
    }
}

static __attribute__((noinline, naked)) void faulthandler(void)
{
    asm volatile(
        "MRS r0, MSP\n\t"
        "BX %[asd]"
        :
        : [asd] "r" (faulthandler_real)
        : "memory");
}

extern const uint8_t nfirmdata[0x200];

void main(void)
{
    // Make regulator stable
    //gpio_init(23);
    //gpio_set_dir(23, GPIO_OUT);
    //gpio_put(23, true);
    
    //sleep_ms(150); // Wait for regulator to stabilize
    
    set_sys_clock_khz(250000, true);
    
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    
    gpio_init(1);
    gpio_set_dir(1, GPIO_OUT);
    gpio_put(1, false);
    
    gpio_init(0);
    gpio_set_dir(0, GPIO_IN);
    gpio_set_pulls(0, true, false);
    
    readhead = 0;
    ensurehead = 0;
    png_lastsize = 0;
    
    if(gpio_get(0))
    {
        //stdio_usb_init();
        //sleep_ms(1000);
        
        stdio_uart_init();
        
        puts("Hello from USB!");
    }
    
    gpio_set_pulls(0, false, false);
    gpio_set_dir(1, GPIO_IN);
    
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, faulthandler);
    
    memset(firmbuf, 0, sizeof(firmbuf));
    firmbuf[0xE00] = (uint8_t)'F';
    firmbuf[0xE01] = (uint8_t)'I';
    firmbuf[0xE02] = (uint8_t)'R';
    firmbuf[0xE03] = (uint8_t)'M';
    memcpy(firmbuf + 0xE00, nfirmdata, 0x200);
    
    coreboot(main1); // Setup rest of the environment, and launch Core1
    //puts("- push");
    
    //bfTests();
    //pngTest();
    
    //multicore_fifo_push_blocking(0xF00FC0CC);
    main1_fifo_push(0xF00FC0CC);
    
    //puts("- wait for pop");
    while(main1_fifo_pop() != 0x55AA00F1) tight_loop_contents();
    multicore_fifo_drain();
    puts("Setup complete, running");
    
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    /*
    {
        
        volatile int lol = 125000000 / 4 / 4;
        while(--lol)
            ;
        
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }
    */
    
    sMainLoop();
}

void MEMELOC_BANK2_F(main1)(void)
{
    //puts("? hello Core1");
    
    while(main1_fifo_pop() != 0xF00FC0CC)
        tight_loop_contents();
    
    //puts("? random init");
    
    //puts("Hello from Core1!");
    
    negInit();
    negResetFull();
    
    pngPreinit();
    png_prog = 0;
    //png_buf[0] = 0x55AA69CC;
    //png_buf[1] = 0x55AA69CC;
    //png_buf[2] = 0x55AA69CC;
    //png_buf[3] = 0x55AA69CC;
    
    //puts("? push ready");
    
    main1_fifo_push(0x55AA00F1);
    multicore_fifo_drain();
    
    //puts("? pushed, initing...");
    
    negEnable();
    
    //puts("? wait for C&C");
    
    pngInit(main1_fifo_pop(), 0xE8);
    for(;;)
    {
        uint32_t newsize = main1_fifo_pop() & ~3;;
        png_lastsize = newsize;
        
        if(png_lastsize <= PNGBUF_SIZE)
        {
            uint32_t irq = save_and_disable_interrupts();
            {
                pngDoBuf(png_buf, newsize, &png_prog);
            }
            restore_interrupts(irq);
        }
        else
        {
            panic("PNG requested size out of bounds: %X > %X\n", newsize, PNGBUF_SIZE);
        }
    }
}
