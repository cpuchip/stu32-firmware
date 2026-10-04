/* The ULP-RISC-V program: one edge of EXTCOMIN (IO16, RTC GPIO 16) each time it wakes, every
   500 ms, so COM inverts at 1 Hz (Sharp Table 6-3-2: typ 1 Hz, max 20 Hz; p.19: COM follows the
   rising edge with EXTMODE high, as on Rev A). It runs while the main CPU works and while it is in
   deep sleep, so the panel keeps its image (unit 018 scope 3). */
#include <stdint.h>
#include "ulp_riscv_gpio.h"

volatile uint32_t edges;                /* read by the main CPU: the ULP is alive */

int main(void)
{
    static int level;
    if (edges == 0) {
        ulp_riscv_gpio_init(GPIO_NUM_16);
        ulp_riscv_gpio_output_enable(GPIO_NUM_16);
    }
    level = !level;
    ulp_riscv_gpio_output_level(GPIO_NUM_16, (uint8_t)level);
    edges++;
    return 0;                           /* halts until the next timer wake */
}
