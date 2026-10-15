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
 * The bookkeeping is in RTC fast memory.
 *
 * Unit 023 (light sleep between keys), with automatic light sleep configured at boot (esp_pm,
 * tickless idle, min 40 MHz) and IO0 as a GPIO wake on a low level:
 *   LIGHT      10 s blocked in light sleep: the ULP's edges (expect about +20) and the time the
 *              PM layer spent in light sleep (the positive control: it did sleep)
 *   LIGHTHALT  LIGHT with the ULP stopped first                          falsifier: flat
 *   LIGHTKEY   blocked in light sleep up to 20 s until IO0 goes low (the host pulls it through the
 *              board's auto-reset DTR line): IO0 wakes it, and how long it took */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_attr.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ulp_common.h"
#include "ulp_main.h"
#include "ulp_riscv.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

static RTC_FAST_ATTR uint32_t before;
static SemaphoreHandle_t io0_low;
static esp_pm_lock_handle_t console;    /* no light sleep while the console waits for a command: the UART
                                           drops what arrives in light sleep (seen on the board) */

static void IRAM_ATTR io0_isr(void *arg)
{
    (void)arg;
    gpio_intr_disable(GPIO_NUM_0);              /* a level interrupt: once, until re-armed */
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(io0_low, &woken);
    if (woken) portYIELD_FROM_ISR();
}

static void light(const char *m)
{
    if (!strcmp(m, "LIGHTHALT")) {
        ulp_riscv_timer_stop();
        ulp_riscv_halt();
        vTaskDelay(pdMS_TO_TICKS(600));
    }
    uint32_t e0 = ulp_edges;
    int64_t t0 = esp_timer_get_time();
    printf("light: mode %s, edges before %lu\n", m, (unsigned long)e0);
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));             /* the line out before the sleep */
    esp_pm_lock_release(console);
    if (!strcmp(m, "LIGHTSETTLE")) {            /* E1: a second for the cores to settle, then measure */
        vTaskDelay(pdMS_TO_TICKS(1000));
        e0 = ulp_edges;
        t0 = esp_timer_get_time();
    }
    if (!strcmp(m, "LIGHTKEY")) {
        xSemaphoreTake(io0_low, 0);
        gpio_intr_enable(GPIO_NUM_0);
        bool got = xSemaphoreTake(io0_low, pdMS_TO_TICKS(20000)) == pdTRUE;
        printf("lightkey: %s after %lld ms, edges %+ld\n", got ? "IO0 woke it" : "no IO0 in 20 s",
               (long long)((esp_timer_get_time() - t0) / 1000), (long)(ulp_edges - e0));
    } else {
        vTaskDelay(pdMS_TO_TICKS(10000));
        uint32_t e1 = ulp_edges;
        printf("light: mode %s, edges after %lu, delta %ld over %lld ms (one edge per 500 ms: about 20)\n", m,
               (unsigned long)e1, (long)(e1 - e0), (long long)((esp_timer_get_time() - t0) / 1000));
    }
    esp_pm_lock_acquire(console);
    esp_pm_dump_locks(stdout);                  /* with CONFIG_PM_PROFILING: the time in light sleep */
    static char stats[1024];                    /* which task held each core (a diagnostic) */
    vTaskGetRunTimeStats(stats);
    printf("tasks:\n%s", stats);
    printf("PROBE READY\n");
    fflush(stdout);
}
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
    if (!strcmp(mode, "IO0") && (why == ESP_SLEEP_WAKEUP_TIMER || why == ESP_SLEEP_WAKEUP_EXT1)) {
        /* abacus #8590: Rev A's refresh wake reads C (IO0, the BOOT button here) with gpio_set_direction only, before
           anything reconfigures the pad EXT1 used. Read it so for 3 s, then after rtc_gpio_deinit, untouched both
           times: a low count with the button up is pwr_release's STUCK (sleep without C's wake) */
        gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
        int lo = 0, n = 0;
        for (; n < 60; n++, vTaskDelay(pdMS_TO_TICKS(50))) lo += !gpio_get_level(GPIO_NUM_0);
        printf("io0: wake cause %d, as Rev A's refresh reads it: low %d of %d\n", (int)why, lo, n);
        rtc_gpio_deinit(GPIO_NUM_0);
        gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
        lo = 0;
        for (n = 0; n < 60; n++, vTaskDelay(pdMS_TO_TICKS(50))) lo += !gpio_get_level(GPIO_NUM_0);
        printf("io0: after rtc_gpio_deinit: low %d of %d\n", lo, n);
        fflush(stdout);
    }
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
    /* unit 023: automatic light sleep, and IO0 (the BOOT button, Rev A's C) as a wake */
    esp_pm_config_t pm = {.max_freq_mhz = 240, .min_freq_mhz = 40, .light_sleep_enable = true};
    ESP_ERROR_CHECK(esp_pm_configure(&pm));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "console", &console));
    ESP_ERROR_CHECK(esp_pm_lock_acquire(console));
    io0_low = xSemaphoreCreateBinary();
    gpio_config_t io0 = {.pin_bit_mask = 1ull << 0, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
                         .intr_type = GPIO_INTR_LOW_LEVEL};
    ESP_ERROR_CHECK(gpio_config(&io0));
    gpio_intr_disable(GPIO_NUM_0);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_0, io0_isr, NULL));
    ESP_ERROR_CHECK(gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
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
            !strcmp(line, "ULPWAKE") || !strcmp(line, "RCFAST") || !strcmp(line, "IO0"))
            sleep_as_reva(line);
        else if (!strcmp(line, "LIGHT") || !strcmp(line, "LIGHTHALT") || !strcmp(line, "LIGHTKEY") ||
                 !strcmp(line, "LIGHTSETTLE"))
            light(line);
        else if (!strcmp(line, "RESTART"))
            esp_restart();
    }
}
