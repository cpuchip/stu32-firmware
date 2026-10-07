/* On-chip test: runs make test's engine tests and vector files from flash and prints a summary
 * on the console, so the decimal library and the core are checked on the product's own
 * processor (abacus #3719: the library's division uses binary double and the rounding-mode
 * calls inside, and neither target has a double-precision FPU). Serial only, no board support.
 *
 * On the chip it prints READY until a byte arrives, then runs, so a capture started after the
 * flash's reset misses nothing. Built on the host (make device-host) it runs at once: the same
 * code, proven before it meets a chip. fmt_fuzz is left out: it needs a Python loop. */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bundle.h"
#include "state.h"

int vectors_run(const char *name, FILE *f);
int fmt_vectors_run(const char *name, FILE *fp);
int diff_test_run(long count, uint64_t seed);
int abn_test_main(void);
int draw_test_main(int argc, char **argv);
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
   saved image must load and save to the same bytes (a newer core writes the newer version). */
ab_calc *vectors_calc(void);

/* A file the vectors load (LOADFILE:), from the bundle. */
bool device_file(const char *path, const uint8_t **data, size_t *len)
{
    for (unsigned i = 0; i < BUNDLE_COUNT; i++)
        if (strcmp(BUNDLE[i].name, path) == 0) {
            *data = (const uint8_t *)BUNDLE[i].text;
            *len = BUNDLE[i].len;
            return true;
        }
    return false;
}

static int time_image(const bundle_file *b)
{
    ab_calc *cp = vectors_calc();
#define c (*cp)
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
    {
        static uint8_t again[4096];
        size_t n2;
        if (len == 0 || len > sizeof out || ab_state_load(&c, out, len) != ABN_OK) return 1;
        n2 = ab_state_save(&c, again, sizeof again);
        return n2 == len && memcmp(again, out, len) == 0 ? 0 : 1;
    }
#undef c
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

static int steps, failed, skipped;

static void step(const char *label, int r, int64_t t0)
{
    steps++;
    if (r) failed++;
    printf("  [%s] %s, %.1f s\n", r ? "FAIL" : "ok", label, (double)(now_us() - t0) / 1e6);
    fflush(stdout);
}

/* ---- unit 018: the device's store and slices in the loop ----
   The harness saves and loads after every key with its image going through the store's two slots
   on the abstate partition (store.h; vectors.c's vectors_flash), and runs programs in slices of 7
   lines, as the device's calc task does. Where there is no partition (the C6's 4 MB, the host) the
   store steps are reported skipped, not passed. */
#include "store.h"
extern const ab_flash *vectors_flash;
static ab_flash dev_flash;

#ifdef ESP_PLATFORM
#include "esp_partition.h"

static int pf_erase(void *ctx, uint32_t off, uint32_t len)
{
    return esp_partition_erase_range(ctx, off, len) == ESP_OK ? 0 : -1;
}

static int pf_write(void *ctx, uint32_t off, const void *src, uint32_t len)
{
    return esp_partition_write(ctx, off, src, len) == ESP_OK ? 0 : -1;
}

static int pf_read(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    return esp_partition_read(ctx, off, dst, len) == ESP_OK ? 0 : -1;
}
#endif

static const bundle_file *bundle_find(const char *name)
{
    for (unsigned i = 0; i < BUNDLE_COUNT; i++)
        if (strcmp(BUNDLE[i].name, name) == 0) return &BUNDLE[i];
    return NULL;
}

static void loop_step(const char *name, bool store, const char *slice)
{
    const bundle_file *b = bundle_find(name);
    char label[96];
    snprintf(label, sizeof label, "%s%s%s%s", name, store ? " + store on flash" : "", slice ? " + slices of " : "",
             slice ? slice : "");
    if (!b || (store && !dev_flash.ctx)) {
        printf("  [skip] %s: %s\n", label, !b ? "not in this bundle" : "no abstate partition");
        skipped++;
        return;
    }
    if (store) {
        setenv("AB_SAVELOAD", "1", 1);
        vectors_flash = &dev_flash;
    }
    if (slice) setenv("AB_SLICE", slice, 1);
    int64_t t = now_us();
    step(label, run_file(b), t);
    unsetenv("AB_SAVELOAD");
    unsetenv("AB_SLICE");
    vectors_flash = NULL;
}

static void loop_steps(void)
{
#ifdef ESP_PLATFORM
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "abstate");
    if (p) dev_flash = (ab_flash){(void *)p, 4096, p->size / 2, pf_erase, pf_write, pf_read};
#endif
    loop_step("work/016-vectors.txt", true, NULL);
    loop_step("work/017-vectors.txt", true, NULL);
    loop_step("work/011-vectors.txt", false, "7");
    loop_step("work/012-vectors.txt", false, "7");
    loop_step("work/013-vectors.txt", false, "7");
    loop_step("work/015-vectors.txt", false, "7");
    /* both: a save and load after every key and at every slice stop. Not 016: its vectors watch
       for slice stops themselves, which the harness's slicing resumes (the Makefile's slice runs
       leave 016-018 out for the same reason). 015, not 012: on the host the save pass multiplies
       012's slices by about 730 (0.11 s to 80 s), and the S3 ran 012's slices alone in 10.9 s, so
       about two hours there: it never finished inside capture.py's hour (2026-10-07). 015's is about
       6 (0.02 s to 0.12 s). */
    loop_step("work/015-vectors.txt", true, "7");
}

static int run_all(void)
{
    int64_t t0 = now_us(), t;
    t = now_us(); step("abn_test", abn_test_main(), t);
    t = now_us(); step("diff_test", diff_test_run(DIFF_COUNT, 0), t);
    t = now_us(); step("xnum_test", xnum_test_main(), t);
    {   /* apps.h v1.2: sdk/draw where long is 32 bits (a far line once drew nothing there; host builds can't see it) */
        static char ops[] = "2000", name[] = "draw_test";
        char *argv[] = {name, ops, NULL};
        t = now_us(); step("draw_test 2000", draw_test_main(2, argv), t);
    }
    for (unsigned i = 0; i < BUNDLE_COUNT; i++) {
        t = now_us();
        step(BUNDLE[i].name, run_file(&BUNDLE[i]), t);
    }
    loop_steps();
    printf("DEVICE %s: %d/%d steps passed, %d skipped, %.1f s\n", failed ? "FAIL" : "PASS", steps - failed,
           steps, skipped, (double)(now_us() - t0) / 1e6);
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
    if (xTaskCreate(test_task, "abacus_test", TEST_STACK, NULL, 5, NULL) != pdPASS)
        printf("DEVICE FAIL: no room for the test task's %u-byte stack (heap free %u)\nDEVICE DONE\n",
               (unsigned)TEST_STACK, (unsigned)esp_get_free_heap_size());
}
#else
int main(void)
{
    return run_all() ? 1 : 0;
}
#endif
