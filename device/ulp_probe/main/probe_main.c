/* ulp_probe: does the ULP-RISC-V keep running through deep sleep with Rev A's sleep configuration
 * (EXT1 on IO0, a timer, GPIO hold, RTC slow memory kept on: review R1)? abacus's Rev A review, R2.
 *
 * The review's R1 call, esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ...), does not exist on the
 * S3 in IDF 5.4.2: soc_caps.h for the S3 defines SOC_PM_SUPPORT_RTC_PERIPH_PD and _RC_FAST_PD only,
 * and esp_sleep.h declares the slow-memory domain only under SOC_PM_SUPPORT_RTC_SLOW_MEM_PD. The
 * domain esp_sleep.h describes as "RTC IO, sensors and ULP co-processor" is RTC_PERIPH, so that is
 * the one measured.
 *
 * Console commands, each a 10 s deep sleep woken by the timer; the ULP's edge count before and after
 * is printed (expect about +20 at one edge per 500 ms):
 *   SLEEP      Rev A as it is (EXT1 + timer + hold, no domain forced)   the measurement
 *   PERIPHON   SLEEP with RTC_PERIPH forced on (R1 on the S3)            the measurement, R1 applied
 *   PERIPHOFF  SLEEP with RTC_PERIPH forced off                          falsifier: flat or reset
 *   HALT       SLEEP with the ULP stopped first                          falsifier: flat
 *   ULPWAKE    SLEEP plus esp_sleep_enable_ulp_wakeup()                  the fallback the review names
 *   RCFAST     ULPWAKE plus RC_FAST kept on                              the second fallback
 * The bookkeeping is in RTC fast memory. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ulp_common.h"
#include "ulp_main.h"
#include "ulp_riscv.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

static RTC_FAST_ATTR uint32_t before;
static RTC_FAST_ATTR char mode[12];

static void sleep_as_reva(const char *m)
{
    snprintf(mode, sizeof mode, "%s", m);
    if (!strcmp(m, "HALT")) {
        ulp_riscv_timer_stop();
        ulp_riscv_halt();
        vTaskDelay(pdMS_TO_TICKS(600));
    }
    before = ulp_edges;
    printf("sleep: mode %s, edges before %lu\n", m, (unsigned long)before);
    fflush(stdout);
    gpio_deep_sleep_hold_en();                                  /* as Rev A's sleep_now */
    esp_sleep_enable_ext1_wakeup(1ull << 0, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_sleep_enable_timer_wakeup(10ull * 1000000);
    if (!strcmp(m, "PERIPHON")) esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    if (!strcmp(m, "PERIPHOFF")) esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    if (!strcmp(m, "ULPWAKE") || !strcmp(m, "RCFAST")) esp_sleep_enable_ulp_wakeup();
    if (!strcmp(m, "RCFAST")) esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_ON);
    esp_deep_sleep_start();
}

void app_main(void)
{
    esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
    if (why == ESP_SLEEP_WAKEUP_TIMER || why == ESP_SLEEP_WAKEUP_EXT1 || why == ESP_SLEEP_WAKEUP_ULP) {
        uint32_t after = ulp_edges;
        printf("wake: cause %d, mode %s, edges after %lu, delta %ld over 10 s (one edge per 500 ms: about 20)\n",
               (int)why, mode, (unsigned long)after, (long)(after - before));
    } else {
        ESP_ERROR_CHECK(ulp_riscv_load_binary(ulp_main_bin_start, (size_t)(ulp_main_bin_end - ulp_main_bin_start)));
        ulp_set_wakeup_period(0, 500000);
        ulp_riscv_cfg_t cfg = ULP_RISCV_DEFAULT_CONFIG();
        ESP_ERROR_CHECK(ulp_riscv_config_and_run(&cfg));
        printf("cold: the ULP loaded and started\n");
    }
    gpio_deep_sleep_hold_dis();
    for (int i = 0; i < 3; i++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("awake: edges %lu\n", (unsigned long)ulp_edges);
    }
    printf("PROBE READY\n");
    fflush(stdout);
    char line[16];
    int n = 0;
    for (;;) {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) != 1) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (ch != '\n' && ch != '\r') {
            if (n < (int)sizeof line - 1) line[n++] = ch;
            continue;
        }
        line[n] = 0;
        n = 0;
        if (!strcmp(line, "SLEEP") || !strcmp(line, "PERIPHON") || !strcmp(line, "PERIPHOFF") || !strcmp(line, "HALT") ||
            !strcmp(line, "ULPWAKE") || !strcmp(line, "RCFAST"))
            sleep_as_reva(line);
        else if (!strcmp(line, "RESTART"))
            esp_restart();
    }
}
