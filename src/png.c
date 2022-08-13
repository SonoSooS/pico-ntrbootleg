/*
    png.c - Nitro Gamecart encryption pseudonumber-generator
    Copyright (C) 2022 Sono (https://github.com/SonoSooS)
    All Rights Reserved
*/

#include <stdint.h>
#include "pico/platform.h"
#include "pico/bit_ops.h"
#include "pico/platform.h"

#include "memeloc.h"
#include "png.h"

#define PNGLOC __scratch_y("png")
//#define PNGLOC __not_in_flash("png")
#define PNGNLOC MEMELOC_BANK2_N("png_shared")
//#define PNGNLOC __not_in_flash("png_main")

#define PNGFUNC(f) MEMELOC_BANK2_F(f)
//#define PNGFUNC(f) __not_in_flash_func(f)

typedef struct png_lohi
{
    uint32_t x_lo;
    uint32_t x_hi;
} png_lohi_t;


volatile uint32_t PNGNLOC png_prog;
uint32_t PNGNLOC png_buf[PNGBUF_SIZE >> 2];

static png_lohi_t PNGLOC key_x;
static png_lohi_t PNGLOC key_y;


static inline uint32_t PNGFUNC(F)(uint32_t x, uint32_t shift)
{
    return ((x >> 5) ^ (x >> shift) ^ (x >> 18)) & 0xFF;
}

static inline uint32_t PNGFUNC(pngRunDo)(png_lohi_t* __restrict x, uint32_t shift)
{
    uint32_t new_hi = (x->x_lo >> 23) & 0xFF;
    uint32_t new_dat = F(x->x_lo, shift) ^ x->x_hi;
    x->x_lo = (x->x_lo << 8) | new_dat;
    x->x_hi = new_hi;
    
    return new_dat;
}

static void PNGFUNC(pngRunInit)(png_lohi_t* __restrict x, uint32_t x_lo, uint32_t x_hi)
{
    x_lo = __rev(x_lo);
    x_hi = __rev(x_hi);
    
    x->x_lo = (x_hi >> (32 - 7)) | (x_lo << 7);
    x->x_hi = (x_lo >> 24);
}

uint32_t PNGFUNC(pngDo)(void)
{
    uint32_t nx = pngRunDo(&key_x, 17);
    uint32_t ny = pngRunDo(&key_y, 23);
    
    return nx ^ ny;
}

void PNGFUNC(pngDoBuf)(uint32_t* __restrict buf, uint32_t size, volatile uint32_t* __restrict const progbuf)
{
    uint32_t prog = 0;
    
    while(prog < size)
    {
        uint32_t dat;
        
        dat = pngDo();
        dat |= pngDo() << 8;
        dat |= pngDo() << 16;
        dat |= pngDo() << 24;
        
        *buf = dat;
        prog += 4;
        
        __compiler_memory_barrier();
        
        *progbuf = prog;
        
        ++buf;
    }
}

void PNGFUNC(pngPreinit)(void)
{
    // 0x5C879B9B05
    pngRunInit(&key_y, 0x879B9B05, 0x5C);
}

void PNGFUNC(pngInit)(uint32_t seed, uint32_t ek)
{
    pngRunInit(&key_x, (seed << 15) | (0x60 << 8) | ek, seed >> (24 - 7));
}
