/* Rev A's ULP program (device/reva/main/ulp/main.c) with the pin changed: on the 4.3" board IO16 is
   the RS485 transceiver's receive line, driven by that chip, so the probe toggles IO6 (the sensor
   header, unconnected). Whether the ULP runs through sleep does not depend on the pin; the count of
   edges is the measurement. */
#include <stdint.h>
#include "ulp_riscv_gpio.h"

volatile uint32_t edges;

int main(void)
{
    static int level;
    if (edges == 0) {
        ulp_riscv_gpio_init(GPIO_NUM_6);
        ulp_riscv_gpio_output_enable(GPIO_NUM_6);
    }
    level = !level;
    ulp_riscv_gpio_output_level(GPIO_NUM_6, (uint8_t)level);
    edges++;
    return 0;
}
