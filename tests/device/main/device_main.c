/* On-chip test: runs make test's engine tests and vector files from flash and prints a summary
 * on the console, so the decimal library and the core are checked on the product's own
 * processor (abacus #3719: the library's division uses binary double and the rounding-mode
 * calls inside, and neither target has a double-precision FPU). Serial only, no board support.
 *
 * On the chip it prints READY until a byte arrives, then runs, so a capture started after the
 * flash's reset misses nothing. Built on the host (make device-host) it runs at once: the same
 * code, proven before it meets a chip. fmt_fuzz is left out: it needs a Python loop. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bundle.h"
#include "state.h"

int vectors_run(const char *name, FILE *f);
int fmt_vectors_run(const char *name, FILE *fp);
int diff_test_run(long count, uint64_t seed);
int abn_test_main(void);
int xnum_test_main(void);

#ifndef DIFF_COUNT
#define DIFF_COUNT 200000       /* make test's count */
#endif

#ifdef ESP_PLATFORM
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
static int64_t now_us(void) { return esp_timer_get_time(); }
#else
#include <time.h>
static int64_t now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
#endif

/* Unit 016: load the image and save it again, n times each, and report the time per call; the
   saved bytes must be the image's. */
static int time_image(const bundle_file *b)
{
    static ab_calc c;
    static uint8_t out[4096];
    int64_t t0, t1, t2;
    const int n = 200;
    size_t len = 0;
    if (b->len > sizeof out) return 2;
    ab_init(&c);
    t0 = now_us();
    for (int i = 0; i < n; i++)
        if (ab_state_load(&c, (const uint8_t *)b->text, b->len) != ABN_OK) return 1;
    t1 = now_us();
    for (int i = 0; i < n; i++) len = ab_state_save(&c, out, sizeof out);
    t2 = now_us();
    printf("%s: %u bytes; load %.0f us, save %.0f us\n", b->name, (unsigned)b->len,
           (double)(t1 - t0) / n, (double)(t2 - t1) / n);
    return len == b->len && memcmp(out, b->text, len) == 0 ? 0 : 1;
}

static int run_file(const bundle_file *b)
{
    if (b->fmt == 2) return time_image(b);
    FILE *f = fmemopen((void *)b->text, b->len, "r");
    if (!f) {
        printf("%s: cannot open from flash\n", b->name);
        return 2;
    }
    int r = b->fmt ? fmt_vectors_run(b->name, f) : vectors_run(b->name, f);
    fclose(f);
    return r;
}

static int steps, failed;

static void step(const char *label, int r, int64_t t0)
{
    steps++;
    if (r) failed++;
    printf("  [%s] %s, %.1f s\n", r ? "FAIL" : "ok", label, (double)(now_us() - t0) / 1e6);
    fflush(stdout);
}

static int run_all(void)
{
    int64_t t0 = now_us(), t;
    t = now_us(); step("abn_test", abn_test_main(), t);
    t = now_us(); step("diff_test", diff_test_run(DIFF_COUNT, 0), t);
    t = now_us(); step("xnum_test", xnum_test_main(), t);
    for (unsigned i = 0; i < BUNDLE_COUNT; i++) {
        t = now_us();
        step(BUNDLE[i].name, run_file(&BUNDLE[i]), t);
    }
    printf("DEVICE %s: %d/%d steps passed, %.1f s\n", failed ? "FAIL" : "PASS", steps - failed,
           steps, (double)(now_us() - t0) / 1e6);
    return failed;
}

#ifdef ESP_PLATFORM
static void test_task(void *arg)
{
    (void)arg;
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    printf("chip model %d rev %d, %d core(s), features 0x%lx; reset reason %d\n", (int)chip.model,
           chip.revision, chip.cores, (unsigned long)chip.features, (int)esp_reset_reason());
    printf("heap free %u (internal %u, largest internal block %u)\n",
           (unsigned)esp_get_free_heap_size(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    fflush(stdout);
    run_all();
    printf("stack high-water: %u bytes unused of %u; heap minimum free %u\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)TEST_STACK,
           (unsigned)esp_get_minimum_free_heap_size());
    printf("DEVICE DONE\n");
    fflush(stdout);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    char ch;
    for (int n = 0; read(STDIN_FILENO, &ch, 1) != 1; n++) {  /* the console reads without blocking;
                                                                read() has no sticky EOF */
        if (n % 20 == 0) {
            printf("READY (send any byte)\n");
            fflush(stdout);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    xTaskCreate(test_task, "abacus_test", TEST_STACK, NULL, 5, NULL);
}
#else
int main(void)
{
    return run_all() ? 1 : 0;
}
#endif
