
#define MEMELOC_BANK2_D(d) __attribute__((section(".memebank2." d)))
#define MEMELOC_BANK2_N(d) __attribute__((section(".memenbank2." d)))
#define MEMELOC_BANK2_F(f) __attribute__((section(".memebank2." #f))) f

#define MEMELOC_BANK3_D(d) __attribute__((section(".memebank3." d)))
#define MEMELOC_BANK3_N(d) __attribute__((section(".memenbank3." d)))
#define MEMELOC_BANK3_F(f) __attribute__((section(".memebank3." #f))) f

#define MEMELOC_RAM_D(d) __attribute__((section(".memedata." d)))
#define MEMELOC_RAM_N(d) __attribute__((section(".memenodata." d)))
#define MEMELOC_RAM_F(f) __attribute__((section(".memedata." #f))) f
