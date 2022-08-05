
#define MEMELOC_BANK2_D(d) __attribute__((section(".memebank2." d)))
#define MEMELOC_BANK2_F(f) __attribute__((section(".memebank2." #f))) f
#define MEMELOC_BANK3_D(d) __attribute__((section(".memebank3." d)))
#define MEMELOC_BANK3_F(f) __attribute__((section(".memebank3." #f))) f
