/*
    picart_setup.c - PIO and SM configuration for proper working
    Copyright (C) 2022 Sono (https://github.com/SonoSooS)
    All Rights Reserved
*/

#include <stdint.h>
#include "hardware/pio.h"
#include "nicart.pio.h"


#define SHIFT_IN_FROM_RIGHT (0)
#define SHIFT_IN_FROM_LEFT (1)
#define SHIFT_OUT_FROM_LEFT (0)
#define SHIFT_OUT_FROM_RIGHT (1)

#define SHIFT_IN_FROM_RIGHT_BITS (SHIFT_IN_FROM_RIGHT << PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB)
#define SHIFT_IN_FROM_LEFT_BITS (SHIFT_IN_FROM_LEFT << PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB)
#define SHIFT_OUT_FROM_LEFT_BITS (SHIFT_OUT_FROM_LEFT << PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB)
#define SHIFT_OUT_FROM_RIGHT_BITS (SHIFT_OUT_FROM_RIGHT << PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB)

static uint piof_mux;
static uint piof_clk;
static PIO _pio;
static uint _sm1;
static uint _sm2;


static void picartInstall1(void)
{
    pio_sm_config smcfg = NICHAN_program_get_default_config(piof_mux);
    sm_config_set_in_pins(&smcfg, 2);
    sm_config_set_out_pins(&smcfg, 2, 12);
    sm_config_set_set_pins(&smcfg, 10, 4);
    sm_config_set_jmp_pin(&smcfg, 11);
    
    smcfg.shiftctrl = smcfg.shiftctrl
        & ~(PIO_SM0_SHIFTCTRL_AUTOPULL_BITS | PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS | PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS | PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS)
        | ((8 & 31) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB)
        | ((false & 1) << PIO_SM0_SHIFTCTRL_AUTOPULL_LSB)
        | ((32 & 31) << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB)
        | ((true & 1) << PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB)
        ;
    
    smcfg.shiftctrl = smcfg.shiftctrl
        & ~(PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_BITS | PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_BITS)
        | (SHIFT_IN_FROM_RIGHT_BITS)
        | (SHIFT_OUT_FROM_RIGHT_BITS)
        ;
    
    pio_sm_set_config(_pio, _sm1, &smcfg);
}

static void picartInstall2(void)
{
    pio_sm_config smcfg = NINACLOCK_program_get_default_config(piof_clk);
    sm_config_set_in_pins(&smcfg, 10);
    sm_config_set_out_pins(&smcfg, 10, 2);
    sm_config_set_set_pins(&smcfg, 10, 2);
    sm_config_set_jmp_pin(&smcfg, 11);
    
    smcfg.shiftctrl = smcfg.shiftctrl
        & ~(PIO_SM0_SHIFTCTRL_AUTOPULL_BITS | PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS | PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS | PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS)
        | ((PIO_SM0_SHIFTCTRL_PULL_THRESH_RESET) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB)
        | ((PIO_SM0_SHIFTCTRL_PUSH_THRESH_RESET) << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB)
        ;
    
    smcfg.shiftctrl = smcfg.shiftctrl
        & ~(PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_BITS | PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_BITS)
        | (SHIFT_IN_FROM_RIGHT_BITS)
        | (SHIFT_OUT_FROM_RIGHT_BITS)
        ;
    
    pio_sm_set_config(_pio, _sm2, &smcfg);
}

static void picartArm1(void)
{
    pio_sm_hw_t* sm = &_pio->sm[_sm1];
    io_rw_32* exec = &sm->instr;
    
    *exec = pio_encode_set(pio_x, 0);
    *exec = pio_encode_mov(pio_isr, pio_null);
    *exec = pio_encode_mov(pio_osr, pio_null);
    *exec = pio_encode_out(pio_pindirs, 12);
    *exec = pio_encode_out(pio_pins, 12);
    *exec = pio_encode_mov(pio_isr, pio_null);
    *exec = pio_encode_mov(pio_osr, pio_null);
    
    *exec = pio_encode_jmp(piof_mux);
}

static void picartArm2(void)
{
    pio_sm_hw_t* sm = &_pio->sm[_sm2];
    io_rw_32* exec = &sm->instr;
    
    *exec = pio_encode_set(pio_x, 0b01);
    *exec = pio_encode_mov(pio_isr, pio_null);
    *exec = pio_encode_mov(pio_osr, pio_null);
    *exec = pio_encode_out(pio_pindirs, 12);
    *exec = pio_encode_out(pio_pins, 12);
    *exec = pio_encode_mov(pio_isr, pio_null);
    *exec = pio_encode_mov(pio_osr, pio_null);
    
    *exec = pio_encode_jmp(piof_clk);
}

void picartResetFull(void)
{
    picartInstall1();
    picartInstall2();
}

void picartSetup(PIO pio, int sm1, int sm2)
{
    _pio = pio;
    _sm1 = sm1;
    _sm2 = sm2;
    
    pio_add_program(pio, &NINO_program); // Should be 0
    
    piof_mux = pio_add_program(pio, &NICHAN_program);
    piof_clk = pio_add_program(pio, &NINACLOCK_program);
}

void picartPreinit(void)
{
    picartArm2();
    picartArm1();
}
