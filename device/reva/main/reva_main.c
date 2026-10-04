/* reva_main: the calculator on Rev A (unit 018 scopes 2-7). Built, not yet run: Rev A is being
 * ordered. Everything it does above the pins is the portable firmware the host tests prove
 * (app.c, keymap.c, screen.c, ls027.c, keys.c, power.c, gauge.c, store.c, pins.c) and the stand-in
 * runs; this file is the pins.
 *
 * - The LS027 (docs/hw facts 1.3-1.6): SPI mode 0, least significant bit first, 1 MHz (max 2),
 *   SCS high around each transfer (setup 3 us, hold 1 us); only the rows that changed are sent.
 *   Power on (Sharp 6-2, p.11): BOOST_EN up (5 V), clear the pixel memory, DISP up, 30 us before
 *   SCS. EXTCOMIN is the ULP's (ulp/main.c): 1 Hz, awake and asleep.
 * - Deep sleep keeps the image: BOOST_EN and DISP are held high, the ULP keeps inverting COM. The
 *   ON key wakes the chip: its column (COL0, IO10) is held low and its row (ROW7, IO9) wakes on low
 *   (both RTC GPIOs, facts 2.5). Sharp asks that a still image not stand two hours unrefreshed
 *   (p.4): a timer wakes the chip every 100 minutes to rewrite it, then it sleeps again.
 * - Keys (keys.h): a scan every millisecond, columns driven low one at a time (open drain), rows
 *   read on their pull-ups. A key held at power-up (the ON press that woke it) makes no event.
 * - Power (power.h, gauge.h): the MAX17048 read every 30 s; CHG_N and PGOOD_N; BOOT levels from
 *   pins.c, except BOOST_EN and DISP after a deep-sleep wake (they were held high).
 * - The console: K<n> presses key n, X prints the state (for bring-up, as on the stand-in). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
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
#include "pins.h"
#include "screen.h"
#include "state.h"
#include "store.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

#define REFRESH_US (100ull * 60 * 1000000)     /* under Sharp's two hours (p.4) */
#define IMAGE_CAP (256u * 1024u)
#define TX_CAP (2 + LS027_H * (LS027_ROW_BYTES + 2))

static const int ROWS[KB_ROWS] = PIN_ROWS, COLS[KB_COLS] = PIN_COLS;
static spi_device_handle_t lcd;
static uint8_t *tx;
static ls027_frame shown, next;         /* what the panel holds, and what it should */
static bool shown_valid;
static QueueHandle_t keys_q;
static ab_calc *calc;
static app_state app;
static uint8_t *image;
static ab_flash flash;
static gauge_bus gbus;
static i2c_master_dev_handle_t gdev;
static int soc = -1;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ---- the LS027 ---- */

static void lcd_send(const uint8_t *b, size_t n)
{
    spi_transaction_t t = {.length = n * 8, .tx_buffer = b};
    spi_device_transmit(lcd, &t);
}

static void lcd_init(bool cold)
{
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
        memset(&shown, 0, sizeof shown);
        shown_valid = true;
        gpio_set_level(PIN_DISP, 1);
        esp_rom_delay_us(30);
    }
}

static void lcd_update(void)
{
    bool dirty[LS027_H];
    int n_dirty = 0;
    for (int y = 0; y < LS027_H; y++) {
        dirty[y] = !shown_valid || memcmp(shown.px[y], next.px[y], LS027_ROW_BYTES) != 0;
        n_dirty += dirty[y];
    }
    if (!n_dirty) return;
    size_t n = ls027_encode(&next, dirty, tx, TX_CAP);
    if (n <= TX_CAP) lcd_send(tx, n);
    shown = next;
    shown_valid = true;
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

static void scan_task(void *arg)
{
    (void)arg;
    static kb_state ks;
    kb_io io = {NULL, kb_drive, kb_read};
    kb_init(&ks);
    for (int i = 0; i <= KB_DEBOUNCE; i++) kb_scan(&ks, &io);   /* keys held at power-up: no event */
    while (kb_next(&ks) >= 0) {
    }
    for (;;) {
        kb_scan(&ks, &io);
        for (int k; (k = kb_next(&ks)) >= 0;) {
            const km_action *f = km_lookup(k, KM_FACE);
            if (app.running && f->kind == KM_OP && (f->op == AB_CANCEL || f->op == AB_RS)) {
                ab_interrupt(calc);                         /* at once; the press is spent */
                continue;
            }
            xQueueSend(keys_q, &k, 0);
        }
        vTaskDelay(1);
    }
}

/* ---- the gauge, the store ---- */

static bool g_xfer(void *ctx, uint8_t addr, const uint8_t *w, size_t wn, uint8_t *r, size_t rn)
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

static void save(void)
{
    size_t n = ab_state_save(calc, NULL, 0);
    if (!flash.ctx || !n || n > IMAGE_CAP) return;
    ab_state_save(calc, image, IMAGE_CAP);
    if (ab_store_save(&flash, image, (uint32_t)n) == 0) app_saved(&app);
}

static bool memory_clear_msg;

static void load(void)
{
    uint32_t n = flash.ctx ? ab_store_load(&flash, image, IMAGE_CAP) : 0;
    if (!n || ab_state_load(calc, image, n) != ABN_OK) {
        ab_memory_clear(calc);
        memory_clear_msg = true;
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
    screen_render(calc, &ui, &next);
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
    gpio_hold_en(PIN_BOOST_EN);
    gpio_hold_en(PIN_DISP);
    for (int c = 0; c < KB_COLS; c++) gpio_set_direction(COLS[c], GPIO_MODE_INPUT);
    gpio_set_direction(COLS[0], GPIO_MODE_OUTPUT);
    gpio_set_level(COLS[0], 0);
    gpio_hold_en(COLS[0]);
    gpio_deep_sleep_hold_en();
    esp_sleep_enable_ext0_wakeup(ROWS[7], 0);
    esp_sleep_enable_timer_wakeup(REFRESH_US);
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
        if (line[0] == 'K') {
            int k = atoi(line + 1);
            xQueueSend(keys_q, &k, 0);
        } else if (line[0] == 'X') {
            int k = -1;
            xQueueSend(keys_q, &k, 0);
        }
    }
}

static void calc_task(void *arg)
{
    (void)arg;
    uint32_t last_gauge = 0;
    render(0);
    printf("REVA READY\n");
    for (;;) {
        int k;
        bool got = xQueueReceive(keys_q, &k, app.running ? 0 : pdMS_TO_TICKS(20)) == pdTRUE;
        uint32_t now = now_ms();
        if (got && k >= 0) {
            memory_clear_msg = false;
            app_key(&app, k, now);
        } else if (got) {
            char stop[32];
            ab_stop_text(calc, stop);
            printf("state: stop=%s lines=%d running=%d soc=%d ulp-edges=%lu\n", stop, calc->prog_len, app.running, soc,
                   (unsigned long)ulp_edges);
        }
        if (gdev && (uint32_t)(now - last_gauge) >= 30000) {
            gauge_reading g;
            if (gauge_sample(&gbus, &g)) soc = g.percent;
            last_gauge = now;
        }
        pwr_inputs in = {gpio_get_level(PIN_CHG_N), gpio_get_level(PIN_PGOOD_N), soc, false};
        app_action act = app_tick(&app, &in, now);
        if (act == APP_SAVE) save();
        if (act == APP_OFF) {
            save();
            render(0);
            sleep_now();
        }
        if (act == APP_RENDER || got) render(0);
    }
}

void app_main(void)
{
    esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
    bool woke = why == ESP_SLEEP_WAKEUP_EXT0 || why == ESP_SLEEP_WAKEUP_TIMER;
    /* the outputs' first levels (pins.c), keeping BOOST_EN and DISP high after a wake */
    for (int i = 0; i < PIN_BOOT_COUNT; i++) {
        const pin_boot *p = &PIN_BOOT[i];
        if (woke && p->pin == PIN_EXTCOMIN) continue;   /* the ULP's pad since the first boot */
        bool held = woke && (p->pin == PIN_BOOST_EN || p->pin == PIN_DISP);
        gpio_set_direction(p->pin, GPIO_MODE_OUTPUT);
        gpio_set_level(p->pin, held ? 1 : p->level);
        if (woke) gpio_hold_dis(p->pin);
    }
    gpio_deep_sleep_hold_dis();
    if (woke) gpio_hold_dis(COLS[0]);
    for (int r = 0; r < KB_ROWS; r++) {
        gpio_set_direction(ROWS[r], GPIO_MODE_INPUT);
        gpio_set_pull_mode(ROWS[r], GPIO_PULLUP_ONLY);
    }
    gpio_set_direction(PIN_CHG_N, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_PGOOD_N, GPIO_MODE_INPUT);

    if (!woke) {                                    /* EXTCOMIN: the ULP, from the first boot on */
        ESP_ERROR_CHECK(ulp_riscv_load_binary(ulp_main_bin_start, (size_t)(ulp_main_bin_end - ulp_main_bin_start)));
        ulp_set_wakeup_period(0, 500000);
        ulp_riscv_cfg_t cfg = ULP_RISCV_DEFAULT_CONFIG();
        ESP_ERROR_CHECK(ulp_riscv_config_and_run(&cfg));
    }
    lcd_init(!woke);

    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bc = {.i2c_port = 0, .sda_io_num = PIN_SDA, .scl_io_num = PIN_SCL,
                                  .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7};
    if (i2c_new_master_bus(&bc, &bus) == ESP_OK) {
        i2c_device_config_t d = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = GAUGE_ADDR, .scl_speed_hz = 100000};
        uint16_t version;
        if (i2c_master_bus_add_device(bus, &d, &gdev) == ESP_OK) {
            gbus = (gauge_bus){NULL, g_xfer};
            if (!gauge_init(&gbus, &version)) printf("gauge: not found\n");
        }
    }

    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "abstate");
    if (p) flash = (ab_flash){(void *)p, 4096, p->size / 2, fl_erase, fl_write, fl_read};
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

    if (why == ESP_SLEEP_WAKEUP_TIMER) {            /* the refresh: rewrite the image, sleep again */
        render(0);
        sleep_now();
    }
    keys_q = xQueueCreate(32, sizeof(int));
    xTaskCreatePinnedToCore(calc_task, "calc", 32768, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(scan_task, "scan", 4096, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 3, NULL, 0);
}
