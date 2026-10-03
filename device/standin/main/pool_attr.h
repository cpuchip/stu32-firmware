/* Forced into core/arena.c's build: its 73 KB pool goes to PSRAM (the internal RAM is the RGB
   interrupt's: the keyboard image, the frames, the bounce buffers). Only the calc task uses the
   pool, and never while it writes flash. */
#define AB_POOL_ATTR __attribute__((section(".ext_ram.bss.ab_pool")))
