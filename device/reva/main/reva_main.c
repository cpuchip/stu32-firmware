/* reva_main: the calculator on Rev A (unit 018 scopes 2-7). Built, not yet run: Rev A is being
 * ordered. Everything it does above the pins is the portable firmware the host tests prove
 * (app.c, keymap.c, screen.c, ls027.c, keys.c, power.c, gauge.c, store.c, pins.c) and the stand-in
 * runs; this file is the pins.
 *
 * - The LS027 (docs/hw facts 1.3-1.6): SPI mode 0, least significant bit first, 1 MHz (max 2),
 *   SCS high around each transfer (setup 3 us, hold 1 us); only the rows that changed are sent.
 *   Power on (Sharp 6-2, p.11): BOOST_EN up (5 V), clear the pixel memory, DISP up, 30 us before
 *   SCS. EXTCOMIN is the ULP's (ulp/main.c): 1 Hz, awake and asleep.
 * - Deep sleep keeps the image: BOOST_EN and DISP are held high, the ULP keeps inverting COM. C
 *   (ON) wakes the chip: since Rev A pass 6 (decision 34) it is on IO0, off the matrix, and wakes
 *   it by EXT1 ANY_LOW alone (an RTC GPIO with an external pull-up). Sharp asks that a still image not stand two hours unrefreshed
 *   (p.4): a timer wakes the chip every 100 minutes to rewrite it, then it sleeps again.
 * - Keys (keys.h): a scan every millisecond, columns driven low one at a time (open drain), rows
 *   read on their pull-ups, C from IO0. A key held at power-up (the ON press that woke it) makes no event.
 * - Power (power.h, gauge.h): the MAX17048 read every 30 s; CHG_N and PGOOD_N; BOOT levels from
 *   pins.c, except BOOST_EN and DISP after a deep-sleep wake (they were held high).
 * - The console (unit 053: the stand-in's, so its scripts run here unchanged): K<n> presses key n, U<n> releases it, X
 *   prints the state (dev_common.h's format), SAVE, WIPE, RESTART, TIME, BEEP, VOL, KEYS (unit 061: echo each key);
 *   with AB_BENCH, EQNBENCH, OPBENCH, PROGBENCH. Since unit 061 it is the USB-C port's (USB-Serial-JTAG, the primary
 *   console: a secondary one takes no input); head-less keeps UART0.
 * - REVA_HEADLESS (unit 053, device/reva/headless.sh): this main on the stand-in's S3 before the boards come, driving
 *   none of Rev A's pins (some are the stand-in's RGB lines): no LCD sends, ULP, piezo, gauge, matrix, light or deep
 *   sleep; keys come from the console. What only the real board can test is listed in work/053. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_partition.h"
#include "esp_pm.h"
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ulp_riscv.h"
#include "ulp_common.h"
#include "ulp_main.h"

#include "app.h"
#include "calc.h"
#include "gauge.h"
#include "keymap.h"
#include "keys.h"
#include "ls027.h"
#include "piezo.h"
#include "pins.h"
#include "screen.h"
#include "state.h"
#include "store.h"
#include "dev_common.h"
#include "dev_bench.h"

#ifdef AB_MASK
/* AB_MASK (MASK=1 device/reva/headless.sh, head-less only, never a product image: abacus #6636): MASK ON masks
   interrupts up to the kernel's level on the calc task's core under each key and each run's slice, so no interrupt frame
   lands on the calc task's stack there, and the stack readings can be taken with and without. Its sdkconfig turns the
   interrupt watchdog off (a SOLVE runs longer masked than its 300 ms); package.sh refuses an image with this banner. */
static volatile bool mask_runs;
#define MASKED(call)                                                                                                   \
    do {                                                                                                               \
        bool mask_now = mask_runs; /* (read once: the console task may change it meanwhile) */                        \
        UBaseType_t mask_level = mask_now ? portSET_INTERRUPT_MASK_FROM_ISR() : 0;                                     \
        call;                                                                                                          \
        if (mask_now) portCLEAR_INTERRUPT_MASK_FROM_ISR(mask_level);                                                   \
    } while (0)

/* The level-5 stack sampler (ab_sample.S; casim docs/stack-gate.md's S, the deepest stack pointer): SAMPLE ON <cycles>
   arms the CPU's timer 2 on the calc task's core, and each sample keeps the lowest calc-task A1. Run under MASK ON, no
   level 1-3 frame is on the stack, so the lowest A1 is the path's own. SAMPLE ROOM and SAMPLE FULL are its controls:
   a 6,000-byte frame written only at its top, and the same frame written whole (predictions before the first build). */
#include "esp_intr_alloc.h"
extern volatile uint32_t ab_sample[9];
static intr_handle_t sample_h;
static uint32_t sample_lo, sample_period;
enum { KEY_SAMPLE_ON = -20, KEY_SAMPLE_OFF = -21, KEY_SAMPLE = -22, KEY_SAMPLE_ROOM = -23, KEY_SAMPLE_FULL = -24,
       KEY_SAMPLE_FPU = -25 };
static void sample_on(uint32_t period)          /* on the calc task's core: the timer and its interrupt are that core's */
{
    if (sample_h) esp_intr_disable(sample_h);
    ab_sample[2] = period;
    ab_sample[3] = sample_lo;
    ab_sample[4] = sample_lo + 32768;
    ab_sample[5] = ab_sample[4];
    ab_sample[6] = ab_sample[7] = ab_sample[8] = 0;
    uint32_t c;
    __asm__ volatile("rsr %0, ccount" : "=r"(c));
    c += period;
    __asm__ volatile("wsr %0, ccompare2\n isync" ::"r"(c));
    if (!sample_h && esp_intr_alloc(ETS_INTERNAL_TIMER2_INTR_SOURCE, ESP_INTR_FLAG_LEVEL5 | ESP_INTR_FLAG_IRAM, NULL, NULL,
                                    &sample_h) != ESP_OK) {
        printf("sample: no level-5 interrupt\n");
        return;
    }
    esp_intr_enable(sample_h);
    printf("sample: on, every %lu cycles\n", (unsigned long)period);
}
static void sample_report(void)
{
    uint32_t hi = ab_sample[4], lo = ab_sample[5];
    printf("sample: depth %lu pc 0x%08lx in %lu of %lu samples\n", (unsigned long)(hi - lo), (unsigned long)ab_sample[6],
           (unsigned long)ab_sample[7], (unsigned long)ab_sample[8]);
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="
static __attribute__((noinline)) void sample_ctrl(bool full)
{
    volatile uint8_t buf[6000];
    if (full) memset((void *)buf, 0x5a, sizeof buf);
    else buf[sizeof buf - 1] = 1;
    esp_rom_delay_us(20000);
}
#pragma GCC diagnostic pop
#else
#define MASKED(call) call
#endif

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

#define REFRESH_US (100ull * 60 * 1000000)     /* under Sharp's two hours (p.4) */
#define STUCK_RECHECK_US (10ull * 1000000)   /* C held down through OFF: look again in 10 s (sleep_now) */
#define IMAGE_CAP (256u * 1024u)
#define TX_CAP (2 + LS027_H * (LS027_ROW_BYTES + 2))

static const int ROWS[KB_ROWS] = PIN_ROWS, COLS[KB_COLS] = PIN_COLS;
static spi_device_handle_t lcd;
static uint8_t *tx;
static ls027_frame *shown, next;        /* what the panel holds (PSRAM: apps.h v1.2 item 1), and what it should */
static bool shown_valid;
static QueueHandle_t keys_q;           /* key n down: n; released: KEY_UP_BASE - n (unit 030); -1 dump,
                                           -2 the time */
#define KEY_UP_BASE (-100)
enum { KEY_SAVE = -3, KEY_WIPE = -4, KEY_RESTART = -5, KEY_EQNBENCH = -10, KEY_OPBENCH = -11, KEY_PROGBENCH = -12 };
                                            /* the console's other commands, for the calc task (unit 053) */
/* unit 023: light sleep between keys. usb_lock keeps the chip out of light sleep on USB power (the
   console stays usable, the power is free); tone_lock through a piezo tone (LEDC stops in light
   sleep); key_wake is given by a row's or IO0's interrupt while the scan sleeps. */
static esp_pm_lock_handle_t usb_lock, tone_lock, app_lock;   /* app_lock: an app ticking (unit 037) */
static bool usb_locked;
static SemaphoreHandle_t key_wake;
static ab_calc *calc;
static app_state app;
static uint8_t *image;
static ab_flash flash;
static ab_flash apps_flash;             /* unit 037: the apps' saves, a store of their own (abapps) */
static gauge_bus gbus;
static i2c_master_dev_handle_t gdev;
static int soc = -1;
static uint32_t vbat_uv;                    /* the gauge's last cell voltage, 0 not read (SYS BATT) */
static uint32_t last_image;                 /* the last image saved: SYS FLASH's room left (unit 027) */
static char time_text[16];                  /* TIME's argument, for the calc task (unit 027) */
static volatile bool key_echo;              /* KEYS: each key printed as it goes down and up (unit 061, bring-up) */
RTC_DATA_ATTR static bool rtc_batt_low;     /* BATT's latch through OFF and wake: RTC memory keeps it
                                               through deep sleep, a power-on clears it (abacus #4238) */

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ---- the LS027 ---- */

static void lcd_send(const uint8_t *b, size_t n)
{
#ifdef REVA_HEADLESS
    (void)b, (void)n;                           /* the frame is made and encoded; nothing goes out */
#else
    spi_transaction_t t = {.length = n * 8, .tx_buffer = b};
    spi_device_transmit(lcd, &t);
#endif
}

static void lcd_init(bool cold)
{
#ifdef REVA_HEADLESS
    (void)cold;
    tx = heap_caps_malloc(TX_CAP, MALLOC_CAP_DMA);
    shown_valid = false;
    return;
#endif
    spi_bus_config_t bc = {.mosi_io_num = PIN_SI, .miso_io_num = -1, .sclk_io_num = PIN_SCLK,
                           .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = TX_CAP};
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bc, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dc = {
        .mode = 0, .clock_speed_hz = 1000000, .spics_io_num = PIN_SCS, .queue_size = 1,
        .flags = SPI_DEVICE_POSITIVE_CS | SPI_DEVICE_TXBIT_LSBFIRST,
        .cs_ena_pretrans = 4, .cs_ena_posttrans = 2,          /* SCS setup 3 us, hold 1 us at 1 MHz */
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dc, &lcd));
    tx = heap_caps_malloc(TX_CAP, MALLOC_CAP_DMA);
    if (cold) {                                               /* Sharp 6-2: 5 V, clear, DISP, 30 us */
        gpio_set_level(PIN_BOOST_EN, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        size_t n = ls027_all_clear(tx);
        lcd_send(tx, n);
        if (shown) memset(shown, 0, sizeof *shown);
        shown_valid = shown != NULL;
        gpio_set_level(PIN_DISP, 1);
        esp_rom_delay_us(30);
    }
}

static void lcd_update(void)
{
    bool dirty[LS027_H];
    if (!ls027_dirty(shown_valid ? shown : NULL, &next, dirty)) return;     /* only the lines that changed (v1.2) */
    size_t n = ls027_encode(&next, dirty, tx, TX_CAP);
    if (n <= TX_CAP) lcd_send(tx, n);
    if (shown) {
        *shown = next;
        shown_valid = true;
    }
}

/* ---- keys ---- */

static void kb_drive(void *ctx, int col)
{
    (void)ctx;
    for (int c = 0; c < KB_COLS; c++) {
        if (c == col) {
            gpio_set_direction(COLS[c], GPIO_MODE_OUTPUT_OD);
            gpio_set_level(COLS[c], 0);
        } else {
            gpio_set_direction(COLS[c], GPIO_MODE_INPUT);
        }
    }
    esp_rom_delay_us(5);
}

static uint8_t kb_read(void *ctx)
{
    (void)ctx;
    uint8_t rows = 0;
    for (int r = 0; r < KB_ROWS; r++)
        if (gpio_get_level(ROWS[r]) == 0) rows |= (uint8_t)(1u << r);
    return rows;
}

static bool kb_read_on(void *ctx)
{
    (void)ctx;
    return gpio_get_level(PIN_ON) == 0;         /* active low on its 10k pull-up (decision 34) */
}

/* every column driven low, so any key pressed pulls its row low: the idle scan's wake (unit 023) */
static void kb_all_low(void)
{
    for (int c = 0; c < KB_COLS; c++) {
        gpio_set_direction(COLS[c], GPIO_MODE_OUTPUT_OD);
        gpio_set_level(COLS[c], 0);
    }
}

static void IRAM_ATTR key_isr(void *arg)
{
    (void)arg;
    for (int r = 0; r < KB_ROWS; r++) gpio_intr_disable(ROWS[r]);    /* level interrupts: once */
    gpio_intr_disable(PIN_ON);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(key_wake, &woken);
    if (woken) portYIELD_FROM_ISR();
}

/* The rows and IO0 as wakes on a low level, for light sleep and for the scan's semaphore. (unused: not head-less) */
static __attribute__((unused)) void key_wake_init(void)
{
    key_wake = xSemaphoreCreateBinary();
    gpio_install_isr_service(0);
    for (int r = 0; r <= KB_ROWS; r++) {
        int pin = r < KB_ROWS ? ROWS[r] : PIN_ON;
        gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);  /* also its interrupt type */
        gpio_intr_disable(pin);
        gpio_isr_handler_add(pin, key_isr, NULL);
    }
    esp_sleep_enable_gpio_wakeup();
}

static __attribute__((unused)) void scan_task(void *arg)
{
    (void)arg;
    static kb_state ks;
    kb_io io = {NULL, kb_drive, kb_read, kb_read_on};
    kb_init(&ks);
    for (int i = 0; i <= KB_DEBOUNCE; i++) kb_scan(&ks, &io);   /* keys held at power-up: no event */
    while (kb_next(&ks) >= 0 || kb_next_up(&ks) >= 0) {
    }
    for (;;) {
        kb_scan(&ks, &io);
        for (int k; (k = kb_next_up(&ks)) >= 0;) {   /* releases first: a pass's order of events */
            int u = KEY_UP_BASE - k;
            xQueueSend(keys_q, &u, 0);
        }
        for (int k; (k = kb_next(&ks)) >= 0;) {
            const km_action *f = km_lookup(k, KM_FACE);
            if ((app.running || app.computing) && f->kind == KM_OP && (f->op == AB_CANCEL || f->op == AB_RS)) {
                app_interrupt(&app);                         /* at once; the press is spent */
                continue;
            }
            xQueueSend(keys_q, &k, 0);
        }
        if (kb_idle(&ks)) {
            /* unit 023: every key up and every count settled, so kb_scan would change nothing until
               a key goes down. Sleep on the rows and IO0 (columns low), then scan every millisecond
               again until idle. A key down already makes its level interrupt fire at once. */
            kb_all_low();
            xSemaphoreTake(key_wake, 0);
            for (int r = 0; r < KB_ROWS; r++) gpio_intr_enable(ROWS[r]);
            gpio_intr_enable(PIN_ON);
            xSemaphoreTake(key_wake, portMAX_DELAY);
            kb_drive(NULL, -1);
        }
        vTaskDelay(1);
    }
}

/* ---- the piezo (decision 34): LEDC on IO46, set up only after boot (IO46 is a strapping pin and
   must read 0 at reset; the MOSFET gate's pull-down holds it). What it sounds for is Michael's
   call, so only the driver is here, and the console's BEEP for bring-up. ---- */

static bool piezo_ready;
static int piezo_level = PIEZO_LEVELS - 1;  /* where the setting lives waits for the menu ruling */

static void piezo_init(void)
{
#ifdef REVA_HEADLESS
    return;                                     /* (piezo_tone says what would sound) */
#endif
    ledc_timer_config_t t = {.speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
                             .timer_num = LEDC_TIMER_0, .freq_hz = PIEZO_HZ, .clk_cfg = LEDC_AUTO_CLK};
    ledc_channel_config_t ch = {.gpio_num = PIN_PIEZO, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0,
                                .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0};
    piezo_ready = ledc_timer_config(&t) == ESP_OK && ledc_channel_config(&ch) == ESP_OK;
}

/* a tone of hz for ms (blocking; 0 Hz is a rest) */
static void piezo_tone(uint32_t hz, uint32_t ms)
{
#ifdef REVA_HEADLESS
    printf("tone: %lu Hz %lu ms level %d\n", (unsigned long)hz, (unsigned long)ms, piezo_level);
#endif
    if (!piezo_ready) return;
    if (tone_lock) esp_pm_lock_acquire(tone_lock);
    if (hz) {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, hz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, piezo_duty(piezo_level, 10));   /* the volume */
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (tone_lock) esp_pm_lock_release(tone_lock);
}

/* ---- the gauge, the store ---- */

static __attribute__((unused)) bool g_xfer(void *ctx, uint8_t addr, const uint8_t *w, size_t wn, uint8_t *r, size_t rn)
{
    (void)ctx;
    (void)addr;
    return (rn ? i2c_master_transmit_receive(gdev, w, wn, r, rn, 50) : i2c_master_transmit(gdev, w, wn, 50)) == ESP_OK;
}

static int fl_erase(void *ctx, uint32_t off, uint32_t len)
{
    return esp_partition_erase_range(ctx, off, len) == ESP_OK ? 0 : -1;
}

static int fl_write(void *ctx, uint32_t off, const void *src, uint32_t len)
{
    return esp_partition_write(ctx, off, src, len) == ESP_OK ? 0 : -1;
}

static int fl_read(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    return esp_partition_read(ctx, off, dst, len) == ESP_OK ? 0 : -1;
}

/* the state image and the device's settings after it (unit 022, settings.h) */
/* unit 037: the apps' saves (apart from the calculator's image), stored when they change */
static uint8_t *apps_buf;               /* apps.h v1.2: the saves' image for the store, in PSRAM (APP_SAVES_IMG_MAX) */

static void apps_save(void)
{
    uint8_t *b = apps_buf;
    if (!app.apps_dirty || !apps_flash.ctx || !b) return;
    size_t n = app_saves_image(&app, b, APP_SAVES_IMG_MAX);
    if (n && ab_store_save(&apps_flash, b, (uint32_t)n) == 0) app.apps_dirty = false;
}

static void save(void)
{
    size_t n = dev_image_save(calc, &app.set, NULL, 0);
    if (!flash.ctx || !n || n > IMAGE_CAP) {
        printf("save: not saved (%u bytes)\n", (unsigned)n);
        return;
    }
    dev_image_save(calc, &app.set, image, IMAGE_CAP);
    int64_t t0 = esp_timer_get_time();
    int rc = ab_store_save(&flash, image, (uint32_t)n);
    printf("save: %u bytes, %s, %lld ms\n", (unsigned)n, rc == 0 ? "ok" : "FAILED",
           (long long)((esp_timer_get_time() - t0) / 1000));
    if (rc == 0) {
        app_saved(&app);
        last_image = (uint32_t)n;
    }
}

static bool memory_clear_msg;
static dev_settings loaded;                 /* the settings load() found, for the app once it is set up */

static void load(void)
{
    uint32_t n = flash.ctx ? ab_store_load(&flash, image, IMAGE_CAP) : 0;
    if (n && dev_image_load(calc, &loaded, image, n) == ABN_OK) {
        printf("load: %u bytes ok\n", (unsigned)n);
    } else {
        ab_memory_clear(calc);              /* and the settings' defaults (dev_image_load gave them) */
        dev_settings_default(&loaded);
        memory_clear_msg = true;
        printf("load: %s: MEMORY CLEAR\n", n ? "image refused" : "no image");
    }
}

static void render(int busy)
{
    screen_ui ui;
    app_ui(&app, &ui);
    if (busy) ui.busy = busy;
    if (memory_clear_msg) ui.prompt = "MEMORY CLEAR";
    pwr_inputs in = {gpio_get_level(PIN_CHG_N), gpio_get_level(PIN_PGOOD_N), soc, false};
    ui.battery = soc;
    ui.charging = pwr_charging(&in);
    if (!app_draw(&app, &next)) screen_render(calc, &ui, &next);   /* an app's frame (unit 037) */
    lcd_update();
}

static void on_busy(void *ctx, int op)
{
    (void)ctx;
    render(op);
}

/* off: the image stays (BOOST_EN, DISP held; the ULP inverts COM), ON wakes, the timer refreshes */
static void sleep_now(void)
{
#ifdef REVA_HEADLESS
    printf("reva: off (head-less: no deep sleep)\n");
    return;
#endif
    gpio_hold_en(PIN_BOOST_EN);
    gpio_hold_en(PIN_DISP);
    for (int c = 0; c < KB_COLS; c++) gpio_set_direction(COLS[c], GPIO_MODE_INPUT);
    gpio_deep_sleep_hold_en();
    /* the ULP's domain kept on explicitly (review R1, ddc9b6c; on the S3 the slow-memory domain is not
       configurable in IDF 5.4.2, RTC_PERIPH is: "RTC IO, sensors and ULP co-processor"). Measured on
       the 4.3"'s S3 (device/ulp_probe): the ULP gives +20 edges in 10 s of deep sleep with or
       without this line, and stops when RTC_PERIPH is forced off */
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    /* C alone wakes it (decision 34): EXT1 ANY_LOW on IO0, whose external pull-up holds with the
       RTC peripherals off. OFF is taken on C's key down, so wait for C's release first, or IO0 still
       low wakes the chip at once (board 0005, abacus #7561; power.h). Held 3 s: sleep without C's
       wake and look again in STUCK_RECHECK_US, so a key held in a bag neither wakes it nor keeps it up. */
    pwr_release_state rs;
    pwr_release_init(&rs, now_ms());
    pwr_release st;
    while ((st = pwr_release_step(&rs, gpio_get_level(PIN_ON), now_ms())) == PWR_RELEASE_WAIT)
        vTaskDelay(pdMS_TO_TICKS(5));
    if (st == PWR_RELEASE_OK) esp_sleep_enable_ext1_wakeup(pin_wake_mask(), ESP_EXT1_WAKEUP_ANY_LOW);
    esp_sleep_enable_timer_wakeup(st == PWR_RELEASE_OK ? REFRESH_US : STUCK_RECHECK_US);
    esp_deep_sleep_start();
}

static void console_task(void *arg)
{
    (void)arg;
    char line[32];
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
        if (!strcmp(line, "KEYS")) {            /* unit 061: the matrix's keys echoed, for bring-up (before K<n>) */
            key_echo = !key_echo;
            printf("keys: echo %s\n", key_echo ? "on" : "off");
        } else if (line[0] == 'K') {
            int k = atoi(line + 1);
            xQueueSend(keys_q, &k, 0);
        } else if (line[0] == 'U' && line[1] >= '0' && line[1] <= '9') {   /* U<n>: key n released */
            int u = KEY_UP_BASE - atoi(line + 1);
            xQueueSend(keys_q, &u, 0);
#ifdef AB_MASK
        } else if (!strcmp(line, "MASK ON") || !strcmp(line, "MASK OFF")) {
            mask_runs = line[6] == 'N';
            printf("mask: %s\n", mask_runs ? "on" : "off");
        } else if (!strncmp(line, "SAMPLE", 6)) {
            int k = KEY_SAMPLE;
            if (!strncmp(line, "SAMPLE ON ", 10)) {
                sample_period = (uint32_t)strtoul(line + 10, NULL, 10);
                k = sample_period >= 200 ? KEY_SAMPLE_ON : KEY_SAMPLE;     /* (shorter: the handler outruns it) */
            } else if (!strcmp(line, "SAMPLE OFF")) {
                k = KEY_SAMPLE_OFF;
            } else if (!strcmp(line, "SAMPLE ROOM")) {
                k = KEY_SAMPLE_ROOM;
            } else if (!strcmp(line, "SAMPLE FULL")) {
                k = KEY_SAMPLE_FULL;
            } else if (!strcmp(line, "SAMPLE FPU")) {   /* the calc task takes the FPU now (casim #7133: libbid's FPU */
                k = KEY_SAMPLE_FPU;                    /* code, a lazy coprocessor exception at its first use) */
            }
            xQueueSend(keys_q, &k, 0);
#endif
        } else if (line[0] == 'X') {
            int k = -1;
            xQueueSend(keys_q, &k, 0);
        } else if (!strncmp(line, "TIME ", 5)) {   /* TIME hh:mm[:ss] (unit 027 rule 4) */
            snprintf(time_text, sizeof time_text, "%.15s", line + 5);
            int k = -2;
            xQueueSend(keys_q, &k, 0);
        } else if (!strcmp(line, "SAVE") || !strcmp(line, "WIPE") || !strcmp(line, "RESTART")) {    /* unit 053 */
            int k = line[0] == 'S' ? KEY_SAVE : line[0] == 'W' ? KEY_WIPE : KEY_RESTART;
            xQueueSend(keys_q, &k, 0);
#ifdef AB_BENCH
        } else if (!strcmp(line, "EQNBENCH") || !strcmp(line, "OPBENCH") || !strcmp(line, "PROGBENCH")) {
            int k = line[0] == 'E' ? KEY_EQNBENCH : line[0] == 'O' ? KEY_OPBENCH : KEY_PROGBENCH;
            xQueueSend(keys_q, &k, 0);
#endif
        } else if (!strcmp(line, "BEEP")) {
            piezo_tone(PIEZO_HZ, 100);          /* bring-up: does the piezo sound */
        } else if (!strncmp(line, "VOL", 3)) {
            piezo_level = atoi(line + 3);       /* bring-up: VOL 0..4 (piezo.h) */
        }
    }
}

/* KEYS's line for a key down (n) or up (KEY_UP_BASE - n): its number, row, column and face legend (unit 061) */
static void echo_key(int k)
{
    bool up = k <= KEY_UP_BASE;
    int n = up ? KEY_UP_BASE - k : k;
    const km_action *f = km_lookup(n, KM_FACE);
    char name[16];
    const char *legend = f->legend && *f->legend ? f->legend : "-";
    if (f->kind == KM_SOFT) snprintf(name, sizeof name, "SOFT%d", f->arg + 1), legend = name;
    else if (f->kind == KM_SHIFT) legend = f->arg == 1 ? "GOLD" : "BLUE";
    printf("key: %d r%d c%d %s %s\n", n, n / KB_COLS, n % KB_COLS, legend, up ? "up" : "down");
}

/* No light sleep on USB power (unit 023 rule 5): the lock follows PGOOD_N. */
static void usb_lock_follow(bool usb)
{
    if (!usb_lock || usb == usb_locked) return;
    if (usb) esp_pm_lock_acquire(usb_lock);
    else esp_pm_lock_release(usb_lock);
    usb_locked = usb;
}

static void calc_task(void *arg)
{
    (void)arg;
    uint32_t last_gauge = 0;
    render(0);
#ifdef AB_MASK
    printf("BENCH: AB_MASK image, MASK ON masks interrupts under keys and runs; never a product image\n");
#endif
    dev_boot_print();                           /* the reason, once the console is up (068) */
    printf("REVA READY\n");
    for (;;) {
        int k;
        /* unit 023: block until a key, the app's next deadline or the gauge's next sample */
        uint32_t now = now_ms();
        pwr_inputs in0 = {gpio_get_level(PIN_CHG_N), gpio_get_level(PIN_PGOOD_N), soc, false};
        usb_lock_follow(!in0.pgood_n);
        uint32_t ms = app_next_wake(&app, &in0, now);
        if (gdev) {
            uint32_t gone = now - last_gauge, g = gone >= 30000 ? 0 : 30000 - gone;
            if (g < ms) ms = g;
        }
        if (app.soft_off && ms > 1000) ms = 1000;      /* 068: a soft off sleeps once USB goes: PGOOD each second */
        TickType_t wait = ms >= 3600000u ? portMAX_DELAY : pdMS_TO_TICKS(ms);
        bool got = xQueueReceive(keys_q, &k, wait) == pdTRUE;
        now = now_ms();
        if (got && key_echo && ((k >= 0 && k < KB_KEYS) || (k <= KEY_UP_BASE && k > KEY_UP_BASE - KB_KEYS))) echo_key(k);
        if (got && k >= 0) {
            memory_clear_msg = false;
            MASKED(app_key(&app, k, now));
        } else if (got && k <= KEY_UP_BASE) {
            MASKED(app_key_up(&app, KEY_UP_BASE - k, now));
        } else if (got && k == -2) {
            int64_t w;
            if (dev_parse_time(&app, now, time_text, &w)) {
                dev_set_wall(NULL, w);
                app_set_wall(&app, w, now);
                printf("time: set %02d:%02d:%02d\n", (int)(w % 86400 / 3600), (int)(w % 3600 / 60), (int)(w % 60));
            } else {
                printf("time: ? %s (hh:mm or hh:mm:ss)\n", time_text);
            }
        } else if (got && k == KEY_SAVE) {
            save();
        } else if (got && k == KEY_WIPE) {
            printf("wipe: %s\n", flash.ctx && esp_partition_erase_range(flash.ctx, 0, 2 * flash.slot_size) == ESP_OK
                                     ? "both slots erased" : "FAILED");
#ifdef AB_MASK
        } else if (got && k == KEY_SAMPLE_ON) {
            sample_on(sample_period);
        } else if (got && k == KEY_SAMPLE_OFF) {
            if (sample_h) esp_intr_disable(sample_h);
            printf("sample: off\n");
        } else if (got && k == KEY_SAMPLE) {
            sample_report();
        } else if (got && k == KEY_SAMPLE_FPU) {
            volatile float fa = 1.5f, fb = 3.0f;
            fa = fa / fb;
            printf("sample: fpu taken (%d)\n", (int)(fa * 1000.0f));
        } else if (got && (k == KEY_SAMPLE_ROOM || k == KEY_SAMPLE_FULL)) {
            MASKED(sample_ctrl(k == KEY_SAMPLE_FULL));
            printf("sample: %s done\n", k == KEY_SAMPLE_FULL ? "full" : "room");
#endif
        } else if (got && k == KEY_RESTART) {
            fflush(stdout);
            esp_restart();
#ifdef AB_BENCH
        } else if (got && k == KEY_EQNBENCH) {
            eqnbench();
        } else if (got && k == KEY_OPBENCH) {
            opbench();
        } else if (got && k == KEY_PROGBENCH) {
            progbench();
#endif
        } else if (got) {                       /* -1: X, the stand-in's format, then Rev A's own facts */
            dev_dump(calc, &app);
            printf("reva: soc=%d vbat=%lu uV ulp-edges=%lu chg_n=%d pgood_n=%d\n", soc, (unsigned long)vbat_uv,
                   (unsigned long)ulp_edges, gpio_get_level(PIN_CHG_N), gpio_get_level(PIN_PGOOD_N));   /* (061) */
            printf("reva: set radix=%c off=%u vol=%u h24=%d\n", app.set.radix, (unsigned)app.set.off_min,
                   (unsigned)app.set.vol, app.set.h24);   /* unit 061: a setting kept across a reset (bring-up) */
        }
        if (gdev && (uint32_t)(now - last_gauge) >= 30000) {
            gauge_reading g;
            if (gauge_sample(&gbus, &g)) {
                soc = g.percent;
                vbat_uv = g.microvolts;
            }
            last_gauge = now;
        }
        pwr_inputs in = {gpio_get_level(PIN_CHG_N), gpio_get_level(PIN_PGOOD_N), soc, false};
        app_action act;
        MASKED(act = app_tick(&app, &in, now));
        rtc_batt_low = app.batt_low;
        if (app.tone >= 0) {                    /* VOL's new level, or a use's sound (units 022, 027) */
            uint32_t hz, ms;
            dev_tone_of(app.tone_use, PIEZO_HZ, &hz, &ms);
            if (app.tone_use == DEV_USE_APP && app.tone_hz) hz = app.tone_hz, ms = app.tone_ms;   /* an app's (037) */
            piezo_level = app.tone;
            piezo_tone(hz, ms);
            piezo_level = app.set.vol;
            app.tone = -1;
        }
        apps_save();                            /* unit 037 */
        {   /* a program's run starting and stopping, as the stand-in says it (unit 053: accept.txt reads it) */
            static bool was_running;
            if (app.running != was_running) {
                char stop[32];
                ab_stop_text(calc, stop);
                printf(app.running ? "run: started\n" : "run: stopped (%s)\n", stop);
                was_running = app.running;
            }
        }
        {   /* no light sleep while an app ticks (rule 7): the lock held once, released once */
            static bool held;
            bool want = app_ticking(&app);
            if (app_lock && want != held) {
                if (want) esp_pm_lock_acquire(app_lock);
                else esp_pm_lock_release(app_lock);
                held = want;
            }
        }
        if (act == APP_SAVE) save();
        if (act == APP_OFF) {
            save();
            render(0);
            sleep_now();
        }
        if (act == APP_RENDER || got) render(0);
    }
}

static uint32_t clock_cb(void *ctx)
{
    (void)ctx;
    return now_ms();
}

/* SYS: the gauge's percent and voltage, the charger's pins (unit 027 rule 5) */
static uint32_t rng_cb(void *ctx)        /* apps.h v1.2: app_seed, the hardware RNG */
{
    (void)ctx;
    return esp_random();
}

static void sysinfo_cb(void *ctx, app_sysinfo *o)
{
    (void)ctx;
    o->soc = soc;
    o->chg_n = gpio_get_level(PIN_CHG_N);
    o->pgood_n = gpio_get_level(PIN_PGOOD_N);
    o->vbat_uv = vbat_uv;
    dev_sysinfo_common(o, flash.ctx && last_image <= flash.slot_size ? flash.slot_size - last_image : 0,
                       flash.ctx ? flash.slot_size : 0);
}

void app_main(void)
{
    dev_boot_count_up();                        /* 068: the boot line's count, first */
    /* the calc task's stack first, in one internal block (the stand-in failed to find 32 KB once
       its other buffers were in; see standin_main.c) */
    static StaticTask_t calc_tcb;
    StackType_t *calc_stack = heap_caps_malloc(32768, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#ifdef AB_MASK
    sample_lo = (uint32_t)calc_stack;
#endif
    esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
    bool woke = why == ESP_SLEEP_WAKEUP_EXT1 || why == ESP_SLEEP_WAKEUP_TIMER;
    /* the outputs' first levels (pins.c), keeping BOOST_EN and DISP high after a wake */
#ifndef REVA_HEADLESS
    for (int i = 0; i < PIN_BOOT_COUNT; i++) {
        const pin_boot *p = &PIN_BOOT[i];
        if (woke && p->pin == PIN_EXTCOMIN) continue;   /* the ULP's pad since the first boot */
        bool held = woke && (p->pin == PIN_BOOST_EN || p->pin == PIN_DISP);
        gpio_set_direction(p->pin, GPIO_MODE_OUTPUT);
        gpio_set_level(p->pin, held ? 1 : p->level);
        if (woke) gpio_hold_dis(p->pin);
    }
    gpio_deep_sleep_hold_dis();
#endif
    gpio_set_direction(PIN_ON, GPIO_MODE_INPUT);
    for (int r = 0; r < KB_ROWS; r++) {
        gpio_set_direction(ROWS[r], GPIO_MODE_INPUT);
        gpio_set_pull_mode(ROWS[r], GPIO_PULLUP_ONLY);
    }
    gpio_set_direction(PIN_CHG_N, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_PGOOD_N, GPIO_MODE_INPUT);

    shown = heap_caps_malloc(sizeof *shown, MALLOC_CAP_SPIRAM);     /* the frame last sent, 12 KB (apps.h v1.2) */
    lcd_init(!woke);
#ifndef REVA_HEADLESS
    if (!woke) {                                    /* EXTCOMIN: the ULP, from the first boot on, started */
        ESP_ERROR_CHECK(ulp_riscv_load_binary(ulp_main_bin_start, (size_t)(ulp_main_bin_end - ulp_main_bin_start)));
        ulp_set_wakeup_period(0, 500000);           /* after DISP rises: Sharp 6-2's order (review R3) */
        ulp_riscv_cfg_t cfg = ULP_RISCV_DEFAULT_CONFIG();
        ESP_ERROR_CHECK(ulp_riscv_config_and_run(&cfg));
    }
#endif

#ifdef REVA_HEADLESS
    printf("gauge: head-less\n");                 /* (its bus would be the stand-in's RGB lines 40, 41) */
#else
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bc = {.i2c_port = 0, .sda_io_num = PIN_SDA, .scl_io_num = PIN_SCL,
                                  .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7};
    if (i2c_new_master_bus(&bc, &bus) == ESP_OK) {
        i2c_device_config_t d = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = GAUGE_ADDR, .scl_speed_hz = 100000};
        uint16_t version;
        if (i2c_master_bus_add_device(bus, &d, &gdev) == ESP_OK) {
            gbus = (gauge_bus){NULL, g_xfer};
            if (!gauge_init(&gbus, &version)) printf("gauge: not found\n");
            else printf("gauge: version 0x%04x\n", version);           /* bring-up step 8 (unit 061) */
        }
    }
#endif

    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "abstate");
    if (p) flash = (ab_flash){(void *)p, 4096, p->size / 2, fl_erase, fl_write, fl_read};
    const esp_partition_t *q = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x41, "abapps");
    if (q) apps_flash = (ab_flash){(void *)q, 4096, q->size / 2, fl_erase, fl_write, fl_read};
    calc = heap_caps_calloc(1, sizeof *calc, MALLOC_CAP_SPIRAM);
    image = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM);
    if (!calc || !image) {
        printf("reva: out of memory\n");
        return;
    }
    ab_init(calc);
    load();
    app_init(&app, calc, now_ms());
    app.busy = on_busy;
    app.key_ups = true;                         /* the matrix sends releases (unit 030) */
    app.set = loaded;
    piezo_level = app.set.vol;
    {   /* UNDO's two images (unit 021), in PSRAM beside the store's; without them UNDO does nothing */
        uint8_t *u0 = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM), *u1 = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM);
        if (!u0 || !u1) {
            printf("reva: no memory for UNDO\n");
            heap_caps_free(u0);
            heap_caps_free(u1);
            u0 = u1 = NULL;
        }
        app_undo_buffers(&app, u0, u1, IMAGE_CAP);
        app_graph_buffer(&app, heap_caps_malloc(sizeof(app_graph), MALLOC_CAP_SPIRAM));   /* unit 033 */
        app_ctx_buffer(&app, heap_caps_malloc(APP_CTX_MAX, MALLOC_CAP_SPIRAM), APP_CTX_MAX);  /* unit 037 */
        if (why != ESP_SLEEP_WAKEUP_TIMER) {    /* (a refresh wake sleeps again at once: no app, no saves) */
            app_save_buffers(&app, heap_caps_malloc(APP_SAVES_IMG_MAX, MALLOC_CAP_SPIRAM), APP_SAVES_IMG_MAX,
                             heap_caps_malloc(APP_SAVE_MAX, MALLOC_CAP_SPIRAM), APP_SAVE_MAX);   /* apps.h v1.2 */
            apps_buf = heap_caps_malloc(APP_SAVES_IMG_MAX, MALLOC_CAP_SPIRAM);
            if (apps_flash.ctx && apps_buf) ab_store_migrate(&apps_flash, 0x2000, apps_buf, APP_SAVES_IMG_MAX);  /* v1.2:
                                                   abapps was 2 x 8 KB */
            uint32_t n = apps_flash.ctx && apps_buf ? ab_store_load(&apps_flash, apps_buf, APP_SAVES_IMG_MAX) : 0;
            if (n && !app_saves_load(&app, apps_buf, n)) printf("reva: apps' saves refused\n");
        }
    }

    /* unit 027: the clock from the RTC (it ran through deep sleep; unset after the cell was out) */
    app_set_wall(&app, dev_wall_now(), now_ms());
    app.set_wall = dev_set_wall;
    app.clock_ms = clock_cb;
    app.sysinfo = sysinfo_cb;
    app.rng = rng_cb;                           /* apps.h v1.2: app_seed */
    app.batt_low = rtc_batt_low;                /* the latch, kept through OFF (abacus #4238) */
    if (why == ESP_SLEEP_WAKEUP_TIMER) {            /* the refresh: rewrite the image, sleep again */
        render(0);
        sleep_now();
    }
    piezo_init();                               /* after boot: IO46 has been read */
    app_powered_on(&app);                       /* ON's chirp: a first boot or a wake from OFF, not the
                                                   refresh (above) nor a light-sleep wake (no boot) */
    keys_q = xQueueCreate(64, sizeof(int));     /* presses and (unit 030) releases: 32 presses ahead */
    /* unit 023: automatic light sleep. The pins that must keep their level or their pull through it
       are taken out of IDF's sleep isolation, which would otherwise let DISP, BOOST_EN, the LCD's
       lines and the columns float (seen on the stand-in: "isolate all GPIO pins in sleep state") */
#ifndef REVA_HEADLESS                                /* (head-less: awake for the console, keys from it) */
    {
        const int keep[] = {PIN_DISP, PIN_BOOST_EN, PIN_SCS, PIN_SCLK, PIN_SI, PIN_PIEZO, PIN_EN1, PIN_EN2, PIN_ON};
        for (size_t i = 0; i < sizeof keep / sizeof keep[0]; i++) gpio_sleep_sel_dis(keep[i]);
        for (int r = 0; r < KB_ROWS; r++) gpio_sleep_sel_dis(ROWS[r]);
        for (int c = 0; c < KB_COLS; c++) gpio_sleep_sel_dis(COLS[c]);
        esp_pm_config_t pm = {.max_freq_mhz = 240, .min_freq_mhz = 40, .light_sleep_enable = true};
        if (esp_pm_configure(&pm) == ESP_OK) {
            esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "usb", &usb_lock);
            esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "tone", &tone_lock);
            esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "app", &app_lock);
            usb_lock_follow(gpio_get_level(PIN_PGOOD_N) == 0);
        } else {
            printf("reva: no power management (light sleep off)\n");
        }
        key_wake_init();
    }
#endif
    printf("reva: heap internal %u (largest block %u), PSRAM %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));   /* (the stand-in's line, unit 053) */
    if (!calc_stack || !xTaskCreateStaticPinnedToCore(calc_task, "calc", 32768, NULL, 4, calc_stack, &calc_tcb, 1))
        printf("reva: the calc task could not be created\n");
#ifndef REVA_HEADLESS
    xTaskCreatePinnedToCore(scan_task, "scan", 4096, NULL, 6, NULL, 0);
#endif
    xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 3, NULL, 0);
}
