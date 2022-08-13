#include <stdint.h>
#include "pico/platform.h"

#include "memeloc.h"

//extern const uint32_t blowfish_S[0x100 * 4];
//extern const uint32_t blowfish_P[18];
extern const uint32_t blowfish_table[(0x100 * 4) + 18];

#pragma GCC optimize ("Os")


#define F F_CPU
//#define F F_INTERP

static inline uint32_t MEMELOC_RAM_F(F_CPU)(uint32_t indata)
{
    //register const uint32_t* dptr = blowfish_S;
    register const uint32_t* dptr = blowfish_table;
    
    register uint32_t data = indata;
    register uint32_t res;
    register uint8_t idx;
    
    {
        data = (data >> 24) | (data << 8);
        idx = (uint8_t)data;
        
        res = dptr[idx];
        dptr += 0x100;
    }
    
    {
        data = (data >> 24) | (data << 8);
        idx = (uint8_t)data;
        
        res += dptr[idx];
        dptr += 0x100;
    }
    
    {
        data = (data >> 24) | (data << 8);
        idx = (uint8_t)data;
        
        res ^= dptr[idx];
        dptr += 0x100;
    }
    
    {
        data = (data >> 24) | (data << 8);
        idx = (uint8_t)data;
        
        res += dptr[idx];
        //dptr += 0x100;
    }
    
    return res;
}

void MEMELOC_RAM_F(bfEncrypt)(uint32_t* __restrict ptr0, uint32_t* __restrict ptr1)
{
    register uint32_t x1 = *ptr0;
    register uint32_t x2 = *ptr1;
    
    //register const uint32_t* dptr = blowfish_P;
    register const uint32_t* dptr = &blowfish_table[0x400];
    
    for(uint32_t i = 0; i != 16; i++)
    {
        x1 ^= *(dptr++);
        x2 ^= F(x1);
        
        uint32_t tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    
    *ptr0 = x2 ^ dptr[1];
    *ptr1 = x1 ^ dptr[0];
}

void MEMELOC_RAM_F(bfDecrypt)(uint32_t* __restrict ptr0, uint32_t* __restrict ptr1)
{
    register uint32_t x1 = *ptr0;
    register uint32_t x2 = *ptr1;
    
    //register const uint32_t* dptr = &blowfish_P[18];
    register const uint32_t* dptr = &blowfish_table[0x400 + 18];
    
    for(uint32_t i = 0; i != 16; i++)
    {
        x1 ^= *(--dptr);
        x2 ^= F(x1);
        
        uint32_t tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    
    *ptr0 = x2 ^ dptr[-2];
    *ptr1 = x1 ^ dptr[-1];
}
