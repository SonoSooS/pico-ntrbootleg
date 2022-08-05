
//#define PNGBUF_SIZE (0x910 + 0x910 + 4 + ((0x910 + 0x1000 + 0x18) * 4))
#define PNGBUF_SIZE (0x910 + 0x1000 + 0x18 + 0x914 + 4)


extern volatile uint32_t png_prog;
extern uint32_t png_buf[PNGBUF_SIZE >> 2];


void pngPreinit(void);
void pngInit(uint32_t seed, uint32_t ek);

uint32_t pngDo(void);
void pngDoBuf(uint32_t* __restrict buf, uint32_t size, volatile uint32_t* __restrict const progbuf);
