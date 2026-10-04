/* Forced into core/arena.c's build on the devices: its 73 KB pool goes to PSRAM, leaving internal
   RAM for what must stay there (on the stand-in, the RGB interrupt's keyboard image, frames and
   bounce buffers). Only the calc task uses the pool, and never while it writes flash. */
#define AB_POOL_ATTR __attribute__((section(".ext_ram.bss.ab_pool")))
