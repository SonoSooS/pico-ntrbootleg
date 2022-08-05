

#define DMALOG_SIZE (32768)
#define DMALOG_BITS ((DMALOG_SIZE - 1) & ~3)

extern uint32_t dmadstbuf[DMALOG_SIZE >> 2];
extern io_ro_32* dmadstptr;
extern dma_channel_hw_t* zerodma;

void negInit(void);
void negResetFull(void);
void negReset(void);
void negEnable(void);
io_ro_32* negGetPtrRead(void);
io_wo_32* negGetPtrWrite(void);
bool negCanRead(void);
bool negCanDrain(void);
bool negCanWrite(void);
uint32_t negBlockingRead(void);
void negBlockingWrite(uint32_t data);
void negBlockingSkip(uint32_t cnt);
void negBlockingDummy(uint32_t cnt, uint32_t data);
void negDMAFillFIFO(uint32_t amount);
