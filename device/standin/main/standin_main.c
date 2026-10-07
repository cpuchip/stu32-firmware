/* standin_main: the calculator on the 4.3" S3 board standing in for Rev A (unit 018).
 *
 * Board facts, read from Waveshare's own documentation for the ESP32-S3-Touch-LCD-4.3 (SKU 25948;
 * schematic sheet 1's GPIO table and CH422G wiring) and the parts' datasheets: the CH422 (its
 * commands are I2C addresses: 0x24 system parameters, 0x38 the IO pins), Goodix's GT911 datasheet
 * and Programming Guide Rev.10 (address 0x5D with INT low through reset; registers 0x8140 product
 * id, 0x814E status, 0x814F the first point; register addresses sent high byte first, measured on
 * this board), the ST7262 datasheet p.52 (typical RGB timing). No vendor demo or BSP code.
 * Measured with a probe before this was written: the I2C scan, the GT911's id and resolution, 61
 * frames a second at these timings, 8 MB of octal PSRAM.
 *
 * Tasks: the touch task polls the GT911 and queues the key under a press (and interrupts a run on
 * C or R/S at once); the console task queues K<n> lines as key n, and a few commands; the calc
 * task runs app.c (keys, slices, power, saves) and draws into one frame while the RGB interrupt
 * reads the other (si_line from the bounce-buffer callback, IRAM-safe while flash is written). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app.h"
#include "calc.h"
#include "keymap.h"
#include "screen.h"
#include "standin.h"
#include "state.h"
#include "store.h"
#include "dev_common.h"
#include "arena.h"
#include "fn.h"

#define I2C_SDA 8
#define I2C_SCL 9
#define TP_INT 4
#define CH422_SYS 0x24                  /* CH422 datasheet 6.1: 48H on the wire */
#define CH422_IO 0x38                   /* 6.3: 70H */
#define EXIO_TP_RST (1u << 1)           /* schematic: CH422G IO1..IO3 */
#define EXIO_DISP (1u << 2)
#define EXIO_LCD_RST (1u << 3)
#define GT911_ADDR 0x5D

enum { EV_KEY = 0, EV_OP, EV_DUMP, EV_SAVE, EV_WIPE, EV_RESTART, EV_TOUCHED, EV_BENCH, EV_TIME, EV_POWBENCH,
       EV_KEYUP, EV_TBLBENCH, EV_GRBENCH, EV_FRAMEBENCH,
       EV_POLBENCH, EV_ZOOMBENCH, EV_CASBENCH, EV_DIFFBENCH };                          /* EV_KEYUP: a the key released (unit 030: SHOW, held) */
static char time_text[16];             /* TIME's argument, for the calc task (unit 027) */

typedef struct {
    int kind, a, b;                     /* EV_KEY: a the key; EV_OP: a core op a with argument b */
} event;

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t ch_sys, ch_io, tp;
static uint8_t exio;
static QueueHandle_t events;

static ls027_frame fb[2];               /* internal RAM: the interrupt reads them */
static volatile int front;
static volatile bool x2;
static uint8_t *keys_img;

static ab_calc *calc;                   /* in PSRAM: 100 KB */
static app_state app;
static uint8_t *image;                  /* the state image for the store, in PSRAM */
#define IMAGE_CAP (256u * 1024u)
#define CALC_STACK 32768                /* the core's deepest use measured 27 KB (unit 016) */
static ab_flash flash;
static ab_flash apps_flash;             /* unit 037: the apps' saves, a store of their own (abapps) */
static bool memory_clear_msg;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ---- the board ---- */

static i2c_master_dev_handle_t i2c_dev(uint16_t addr, uint32_t hz)
{
    i2c_device_config_t d = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr, .scl_speed_hz = hz};
    i2c_master_dev_handle_t h;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &d, &h));
    return h;
}

static esp_err_t exio_set(uint8_t v)
{
    exio = v;
    return i2c_master_transmit(ch_io, &v, 1, 100);
}

static void board_init(void)
{
    i2c_master_bus_config_t bc = {.i2c_port = 0, .sda_io_num = I2C_SDA, .scl_io_num = I2C_SCL,
                                  .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
                                  .flags.enable_internal_pullup = true};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc, &bus));
    ch_sys = i2c_dev(CH422_SYS, 100000);
    ch_io = i2c_dev(CH422_IO, 100000);
    uint8_t oe = 0x01;                  /* IO_OE: IO7..IO0 are outputs */
    ESP_ERROR_CHECK(i2c_master_transmit(ch_sys, &oe, 1, 100));
    /* both resets low with the GT911's INT held low, then high: the GT911 takes 0x5D (GT911
       datasheet 6.1); the backlight comes on with them */
    gpio_set_direction(TP_INT, GPIO_MODE_OUTPUT);
    gpio_set_level(TP_INT, 0);
    ESP_ERROR_CHECK(exio_set(0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(exio_set(EXIO_TP_RST | EXIO_DISP | EXIO_LCD_RST));
    vTaskDelay(pdMS_TO_TICKS(60));
    gpio_set_direction(TP_INT, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(100));
    tp = i2c_dev(GT911_ADDR, 400000);
    uint8_t reg[2] = {0x81, 0x40}, id[4] = {0};
    esp_err_t e = i2c_master_transmit_receive(tp, reg, 2, id, 4, 100);
    printf("touch: GT911 id '%.3s' (%s)\n", (char *)id, esp_err_to_name(e));
}

/* the GT911's first point, if it has new data (Programming Guide 3.3, 5): false when not ready */
static bool tp_read(int *x, int *y, int *n)
{
    uint8_t reg[2] = {0x81, 0x4E}, st = 0, p[8];
    if (i2c_master_transmit_receive(tp, reg, 2, &st, 1, 50) != ESP_OK || !(st & 0x80)) return false;
    *n = st & 0x0F;
    if (*n > 0) {
        uint8_t preg[2] = {0x81, 0x4F};
        if (i2c_master_transmit_receive(tp, preg, 2, p, 8, 50) != ESP_OK) return false;
        *x = p[1] | p[2] << 8;
        *y = p[3] | p[4] << 8;
    }
    uint8_t clr[3] = {0x81, 0x4E, 0x00};  /* the host clears the status after reading */
    i2c_master_transmit(tp, clr, 3, 50);
    return true;
}

static bool IRAM_ATTR on_bounce(esp_lcd_panel_handle_t p, void *buf, int pos_px, int len_bytes, void *ctx)
{
    (void)p;
    (void)ctx;
    si_view v = {&fb[front], keys_img, x2};
    int y = pos_px / SI_W, n = len_bytes / (SI_W * 2);
    for (int i = 0; i < n; i++) si_line(&v, y + i, (uint16_t *)buf + i * SI_W);
    return false;
}

static void screen_init(void)
{
    esp_lcd_rgb_panel_config_t pc = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {.pclk_hz = 25000000, .h_res = SI_W, .v_res = SI_H,
                    .hsync_pulse_width = 4, .hsync_back_porch = 8, .hsync_front_porch = 8,
                    .vsync_pulse_width = 4, .vsync_back_porch = 8, .vsync_front_porch = 8,
                    .flags.pclk_active_neg = 1},
        .data_width = 16, .bits_per_pixel = 16, .bounce_buffer_size_px = SI_W * 10,
        .hsync_gpio_num = 46, .vsync_gpio_num = 3, .de_gpio_num = 5, .pclk_gpio_num = 7, .disp_gpio_num = -1,
        /* RGB565: B3..B7, G2..G7, R3..R7 (schematic sheet 1's table) */
        .data_gpio_nums = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40},
        .flags.no_fb = 1,
    };
    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&pc, &panel));
    esp_lcd_rgb_panel_event_callbacks_t cb = {.on_bounce_empty = on_bounce};
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cb, NULL));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
}

/* ---- the store on the abstate partition (two slots, store.h) ---- */

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

static void store_init(void)
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "abstate");
    if (!p) {
        printf("store: no abstate partition\n");
        return;
    }
    flash = (ab_flash){(void *)p, 4096, p->size / 2, fl_erase, fl_write, fl_read};
    const esp_partition_t *q = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x41, "abapps");
    if (q) apps_flash = (ab_flash){(void *)q, 4096, q->size / 2, fl_erase, fl_write, fl_read};
    else printf("store: no abapps partition\n");
}

/* unit 037: the apps' saves (apart from the calculator's image), stored when they change */
static uint8_t *apps_buf;               /* apps.h v1.2: the saves' image for the store, in PSRAM (APP_SAVES_IMG_MAX) */

static void apps_save(void)
{
    uint8_t *b = apps_buf;
    if (!app.apps_dirty || !apps_flash.ctx || !b) return;
    size_t n = app_saves_image(&app, b, APP_SAVES_IMG_MAX);
    int rc = n ? ab_store_save(&apps_flash, b, (uint32_t)n) : -1;
    printf("apps: saves %u bytes, %s\n", (unsigned)n, rc == 0 ? "ok" : "FAILED");
    if (rc == 0) app.apps_dirty = false;
}

static void apps_load(void)
{
    uint8_t *b = apps_buf;
    uint32_t n = apps_flash.ctx && b ? ab_store_load(&apps_flash, b, APP_SAVES_IMG_MAX) : 0;
    printf("apps: %s\n", n && app_saves_load(&app, b, n) ? "saves loaded" : "no saves");
}

static uint32_t last_image;             /* the last image saved: SYS FLASH's room left (unit 027) */

/* the state image and the device's settings after it (unit 022, settings.h) */
static void save(void)
{
    size_t n = dev_image_save(calc, &app.set, NULL, 0);
    if (!flash.ctx || n == 0 || n > IMAGE_CAP) {
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

static dev_settings loaded;             /* the settings load() found, for the app once it is set up */

static void load(void)
{
    uint32_t n = flash.ctx ? ab_store_load(&flash, image, IMAGE_CAP) : 0;
    abn_status st = n ? dev_image_load(calc, &loaded, image, n) : ABN_BAD_IMAGE;
    if (st == ABN_OK) {
        printf("load: %u bytes ok\n", (unsigned)n);
    } else {
        ab_memory_clear(calc);          /* state.h: MEMORY CLEAR, shown (UG p.B-3) */
        dev_settings_default(&loaded);  /* and the settings' defaults */
        memory_clear_msg = true;
        printf("load: %s: MEMORY CLEAR\n", n ? "image refused" : "no image");
    }
}

/* ---- drawing ---- */

static si_keys_state keys_drawn = {-1, false, -1, false};

static void render(int busy)
{
    screen_ui ui;
    app_ui(&app, &ui);
    if (busy) ui.busy = busy;
    if (memory_clear_msg) ui.prompt = "MEMORY CLEAR";
    int back = 1 - front;
    if (!app_draw(&app, &fb[back])) screen_render(calc, &ui, &fb[back]);   /* an app's frame (unit 037) */
    front = back;
    si_keys_state ks = {app.shift, app.prompt_op >= 0, -1, calc->base == 16};
    if (memcmp(&ks, &keys_drawn, sizeof ks)) {
        si_draw_keys(keys_img, &ks);
        keys_drawn = ks;
    }
}

static void on_busy(void *ctx, int op)
{
    (void)ctx;
    render(op);
}

/* the composed colour at a screen pixel, as the RGB interrupt sends it (for checking by console) */
static uint16_t pixel_at(int x, int y)
{
    static uint16_t line[SI_W];
    si_view v = {&fb[front], keys_img, x2};
    si_line(&v, y, line);
    return line[x];
}

static void dump(void)
{
    char x[96];
    ab_display d = {{AB_ALL, 0, 40, '.', false}, false, 4095, 0, 10, false, 0};
    int ind;
    if (ab_format_display(&calc->x.re, &d, x, sizeof x, &ind) < 0) snprintf(x, sizeof x, "?");
    char stop[32];
    ab_stop_text(calc, stop);
    const char *m = ab_error_message(calc);
    printf("state: X=%s stop=%s lines=%d prgm=%d running=%d shift=%d prompt=%s msg=%s heap=%u stack-free=%u\n", x, stop,
           calc->prog_len, calc->prgm, app.running, app.shift, app.prompt_op >= 0 ? app.prompt_text : "-", m ? m : "-",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)uxTaskGetStackHighWaterMark(NULL));
    {   /* unit 027: the status band's clock and a SYS view's first line, as the screen has them */
        static screen_ui ui;
        app_ui(&app, &ui);
        printf("ui: clock=%s%c view=%s soft1=%s\n", ui.clock, ui.ampm ? ui.ampm : ' ', ui.view[0] ? ui.view[0] : "-",
               ui.soft[0] && ui.soft[0][0] ? ui.soft[0] : "-");
        printf("app: %s\n", app_running(&app) ? app.app->name : "-");     /* unit 037 */
        printf("ui: show=%d %s%s%s\n", ui.show_n, ui.show_n ? ui.show[0] : "-", ui.show_n > 1 ? "/" : "",
               ui.show_n > 1 ? ui.show[1] : "");     /* unit 030: SHOW held */
    }
    printf("pixels: gold key %04X, blue key %04X (gold %04X, blue %04X, key %04X)\n", pixel_at(440, 345),
           pixel_at(440, 405), SI_PALETTE[SI_GOLD], SI_PALETTE[SI_BLUE], SI_PALETTE[SI_KEY]);
}

/* POWBENCH (unit 028): y^x's time on the S3, the engine's pow (the route before 028 for a non-integer
   exponent) beside fn_pow (028's: exact squaring, else e^(x ln y) in hp), typical and worst cases. */
/* Unit 032 rule 7: a table's screenful (four rows) of a 190-byte STU equation, timed, on a calculator of
   its own (in PSRAM) so the user's state is untouched. */
static void tblbench(void)
{
    static const char *const E[][2] = {
        {"sums", "X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X"
                 "+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X+X"},
        {"sines", "SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+"
                  "SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+X"},
        {"powers", "X^2.5+X^3.5+X^4.5+X^5.5+X^6.5+X^7.5+X^8.5+X^9.5+X^1.5+X^2.25+X^3.25+X^4.25+X^5.25+X^6.25+"
                   "X^7.25+X^8.25+X^9.25+X^1.25+X^2.75+X^3.75+X^4.75+X^5.75+X^6.75+X^7.75+X^8.75+X^9"},
    };
    ab_calc *t = heap_caps_calloc(1, sizeof *t, MALLOC_CAP_SPIRAM);
    if (!t) {
        printf("tblbench: no memory\n");
        return;
    }
    for (size_t i = 0; i < sizeof E / sizeof E[0]; i++) {
        ab_init(t);
        ab_do_arg(t, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(t, E[i][1]);
        ab_do(t, AB_EQN);
        ab_do(t, AB_K2);
        ab_do_arg(t, AB_STO, AB_VAR_A + 23);
        abn_status go = ab_do(t, AB_TABLE);
        const int reps = 5;
        abn x;
        ab_val y;
        abn_status st = ABN_OK;
        int64_t t0 = esp_timer_get_time();
        for (int r = 0; r < reps; r++)
            for (int k = -3; k <= 0; k++) st = ab_table_row(t, k, &x, &y);
        int64_t t1 = esp_timer_get_time();
        printf("tblbench: %-7s %3u bytes  a screenful (4 rows) %lld us  (GO %d, row %d)\n", E[i][0],
               (unsigned)strlen(t->eqn[t->eqn_cur]), (long long)(t1 - t0) / reps, (int)go, (int)st);
    }
    heap_caps_free(t);
}

/* Unit 033 rule 8: a full plot (400 samples and their rows), AUTO and Y set, for a typical equation and a
   long one, on a calculator of its own (PSRAM), timed. */
static void grbench(void)
{
    static const char *const E[][2] = {
        {"typical", "X^3-6\xc3\x97X^2+4\xc3\x97X+12"},
        {"sines", "SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+"
                  "SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+SIN(X)+X"},
        {"powers", "X^2.5+X^3.5+X^4.5+X^5.5+X^6.5+X^7.5+X^8.5+X^9.5+X^1.5+X^2.25+X^3.25+X^4.25+X^5.25+X^6.25+"
                   "X^7.25+X^8.25+X^9.25+X^1.25+X^2.75+X^3.75+X^4.75+X^5.75+X^6.75+X^7.75+X^8.75+X^9"},
    };
    ab_calc *t = heap_caps_calloc(1, sizeof *t, MALLOC_CAP_SPIRAM);
    app_graph *g = heap_caps_malloc(sizeof *g, MALLOC_CAP_SPIRAM);
    if (!t || !g) {
        printf("grbench: no memory\n");
        return;
    }
    for (size_t i = 0; i < sizeof E / sizeof E[0]; i++) {
        ab_init(t);
        ab_do_arg(t, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(t, E[i][1]);
        ab_do(t, AB_EQN);
        if (i > 0) {                            /* the powers need X > 0: 0.5..10 for all three past the first */
            ab_do(t, AB_KPOINT); ab_do(t, AB_K5); ab_do_arg(t, AB_XMIN, 0);
        }
        abn_status go = ab_do(t, AB_GRAPH);
        int64_t t0 = esp_timer_get_time();
        bool okb[AB_GRAPH_W];
        int fin = 0;
        for (int k = 0; k < AB_GRAPH_W; k++) {
            abn x;
            ab_val v;
            okb[k] = ab_graph_sample(t, k, &x, &v) == ABN_OK;
            g->y[k] = v.re;
            fin += okb[k];
        }
        abn lo, hi;
        ab_graph_fit(g->y, okb, AB_GRAPH_W, &lo, &hi);
        for (int k = 0; k < AB_GRAPH_W; k++)
            if (okb[k]) g->row[k] = (int16_t)ab_graph_row(&g->y[k], &lo, &hi);
        int64_t t1 = esp_timer_get_time();
        printf("grbench: %-8s %3u bytes  a full plot (400 samples, fit, rows) %lld ms  (GO %d, %d finite)\n", E[i][0],
               (unsigned)strlen(t->eqn[t->eqn_cur]), (long long)(t1 - t0) / 1000, (int)go, fin);
    }
    heap_caps_free(g);
    heap_caps_free(t);
}

static void powbench(void)
{
    static const char *const C[][3] = {
        {"typical", "2.5", "3.7"},
        {"typical", "123.456", "-0.31"},
        {"integer 37 (exact)", "1.0001", "37"},
        {"integer 100000", "1.000001", "100000"},
        {"half 1000.5", "1.01", "1000.5"},
        {"near 1, 1E35", "1.000000000000000000000000000000001", "1E35"},
        {"near 1, 1E20", "0.999999999999999997", "-3.3E20"},
        {"edge base", "9.9E6144", "0.999"},
        {"large result", "7", "7000.5"},
    };
    for (size_t i = 0; i < sizeof C / sizeof C[0]; i++) {
        abn y, x, r;
        abn_from_text(&y, C[i][1]);
        abn_from_text(&x, C[i][2]);
        const int reps = 50;
        int64_t t0 = esp_timer_get_time();
        abn_status s1 = ABN_OK, s2 = ABN_OK;
        for (int k = 0; k < reps; k++) s1 = abn_pow(&r, &y, &x);
        int64_t t1 = esp_timer_get_time();
        for (int k = 0; k < reps; k++) {
            int m = ar_mark();
            s2 = fn_pow(&r, &y, &x);
            ar_release(m);
        }
        int64_t t2 = esp_timer_get_time();
        printf("powbench: %-20s %s^%s  engine %lld us (st %d)  fn_pow %lld us (st %d)\n", C[i][0], C[i][1], C[i][2],
               (long long)(t1 - t0) / reps, (int)s1, (long long)(t2 - t1) / reps, (int)s2);
    }
    printf("powbench: done\n");
}

/* ZOOMBENCH (unit 040): the typical cubic, Y fitted; after each zoom the zoom's own time (a fitted Y is sampled
   to hold it) and a full redraw's (400 samples, the fit where Y is fitted, the rows); then each tool's time at the
   cursor. */
static void zoombench(void)
{
    static const struct { const char *name; int kind; } Z[] = {
        {"ZIN", AB_Z_IN}, {"ZOUT", AB_Z_OUT}, {"CNTR", AB_Z_CNTR}, {"HZIN", AB_Z_HIN}, {"VZOUT", AB_Z_VOUT},
        {"ZDECI", AB_Z_DECI}, {"ZINTG", AB_Z_INTG}, {"ZSQR", AB_Z_SQR}, {"ZTRIG", AB_Z_TRIG}, {"ZLAST", AB_Z_LAST},
        {"ZDFLT", AB_Z_DFLT}, {"ZAUTO", AB_Z_AUTO}};
    static const struct { const char *name; int kind; } F[] = {
        {"ROOT", AB_F_ROOT}, {"SLOPE", AB_F_SLOPE}, {"EXTR", AB_F_EXTR}, {"F(X)", AB_F_FX}, {"TANL", AB_F_TANL}};
    ab_calc *t = heap_caps_calloc(1, sizeof *t, MALLOC_CAP_SPIRAM);
    app_graph *g = heap_caps_malloc(sizeof *g, MALLOC_CAP_SPIRAM);
    if (!t || !g) {
        printf("zoombench: no memory\n");
        return;
    }
    ab_init(t);
    ab_do_arg(t, AB_MODEL, AB_MODEL_STU);
    ab_eqn_add(t, "X^3-6\xc3\x97X^2+4\xc3\x97X+12");
    ab_do(t, AB_EQN);
    abn_status go = ab_do(t, AB_GRAPH);
    {   /* the first plot, its fit known to the core as the app makes it known (040 review) */
        bool okb[AB_GRAPH_W];
        int64_t t0 = esp_timer_get_time();
        for (int k = 0; k < AB_GRAPH_W; k++) {
            abn x;
            ab_val v;
            okb[k] = ab_graph_sample(t, k, &x, &v) == ABN_OK;
            g->y[k] = v.re;
        }
        abn lo, hi;
        ab_graph_fit(g->y, okb, AB_GRAPH_W, &lo, &hi);
        ab_graph_fit_known(t, &lo, &hi);
        printf("zoombench: GO      the first plot %lld ms (Y fitted, its fit then known)\n", (long long)(esp_timer_get_time() - t0) / 1000);
    }
    for (size_t i = 0; i < sizeof Z / sizeof Z[0]; i++) {
        int64_t t0 = esp_timer_get_time();
        abn_status zs = ab_do_arg(t, AB_GZOOM, Z[i].kind);
        int64_t t1 = esp_timer_get_time();
        bool okb[AB_GRAPH_W];
        for (int k = 0; k < AB_GRAPH_W; k++) {
            abn x;
            ab_val v;
            okb[k] = ab_graph_sample(t, k, &x, &v) == ABN_OK;
            g->y[k] = v.re;
        }
        abn lo = t->gr_ymin, hi = t->gr_ymax;
        if (t->gr_auto) ab_graph_fit(g->y, okb, AB_GRAPH_W, &lo, &hi);
        for (int k = 0; k < AB_GRAPH_W; k++)
            if (okb[k]) g->row[k] = (int16_t)ab_graph_row(&g->y[k], &lo, &hi);
        int64_t t2 = esp_timer_get_time();
        printf("zoombench: %-6s the zoom %lld ms, a full redraw %lld ms (st %d, Y %s)\n", Z[i].name,
               (long long)(t1 - t0) / 1000, (long long)(t2 - t1) / 1000, (int)zs, t->gr_auto ? "fitted" : "set");
    }
    ab_do_arg(t, AB_GMOVE, 16);                 /* the cursor 10 right of home, x about 0.5 */
    for (size_t i = 0; i < sizeof F / sizeof F[0]; i++) {
        int64_t t0 = esp_timer_get_time();
        abn_status fs = ab_do_arg(t, AB_GFCN, F[i].kind);
        int64_t t1 = esp_timer_get_time();
        printf("zoombench: %-6s %lld ms (st %d)\n", F[i].name, (long long)(t1 - t0) / 1000, (int)fs);
        ab_do_arg(t, AB_GMOVE, 5);
        ab_do_arg(t, AB_GMOVE, 16);
    }
    {   /* unit 040c: EXTR at the slope's noise floor (abacus's A00468: x e^(-x/0.8) on 0..5, the cursor at x
           near 0.79), the check's path on the device: its time, its answer (0.8 within 1E-21) and the stack */
        ab_init(t);
        ab_do_arg(t, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(t, "X\xc3\x97" "EXP(0-X\xc3\xb7" "0.8)");
        ab_do(t, AB_EQN);
        t->gr_xmin = abn_from_int(0), t->gr_xmax = abn_from_int(5);
        ab_do(t, AB_GRAPH);
        for (int k = 0; k < 13; k++) ab_do_arg(t, AB_GMOVE, 14);
        for (int k = 0; k < 9; k++) ab_do_arg(t, AB_GMOVE, 4);
        int64_t t0 = esp_timer_get_time();
        abn_status fs = ab_do_arg(t, AB_GFCN, AB_F_EXTR);
        int64_t t1 = esp_timer_get_time();
        char r[64] = "-";
        if (t->gr_res_kind) abn_to_text(&t->gr_res, r, sizeof r);
        printf("zoombench: EXTR040c %lld ms (st %d) x %s stack-free %u\n", (long long)(t1 - t0) / 1000, (int)fs, r,
               (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
    printf("zoombench: done (GO %d)\n", (int)go);
    heap_caps_free(g);
    heap_caps_free(t);
}

/* DIFFBENCH (apps.h v1.2 item 1, abacus #4734): ls027_dirty's time on the S3 for a whole frame against the frame last
   sent, as Rev A keeps it (in PSRAM) and in internal RAM for comparison: no line changed (every row compared to its
   end), and every line changed (each row's first byte differs). Microseconds a call, over 100. */
static void diffbench(void)
{
    static ls027_frame inner_a;                 /* (internal: the stand-in's own frames are) */
    ls027_frame *pa = heap_caps_malloc(sizeof *pa, MALLOC_CAP_SPIRAM), *pb = heap_caps_malloc(sizeof *pb, MALLOC_CAP_SPIRAM);
    if (!pa || !pb) {
        printf("diffbench: no PSRAM\n");
        free(pa), free(pb);
        return;
    }
    static bool d[LS027_H];
    for (int k = 0; k < 4; k++) {
        bool psram = k < 2, all = k & 1;
        ls027_frame *shown = psram ? pa : &inner_a, *next = &fb[front];
        memcpy(shown, next, sizeof *shown);
        if (all)
            for (int y = 0; y < LS027_H; y++) shown->px[y][0] ^= 0xFF;
        int n = 0;
        int64_t t0 = esp_timer_get_time();
        for (int r = 0; r < 100; r++) n = ls027_dirty(shown, next, d);
        int64_t us = esp_timer_get_time() - t0;
        printf("diffbench: shown in %s, %s: %d rows dirty, %lld.%02lld us a call\n", psram ? "PSRAM" : "internal RAM",
               all ? "every line changed" : "no line changed", n, us / 100, us % 100);
    }
    free(pa), free(pb);
}

/* CASBENCH (CAS 004 rule 6): each Casimir step's time on the S3 over heavy shapes (near the 191-byte slot, deep
   nests, big expansions, the depth guard), in STU, from the calc task, as the keys reach it: status, the result's
   length, milliseconds, and the task's stack still free. */
static void casbench(void)
{
    static const struct { const char *name; int op, var; const char *eqn, *repl; } K[] = {
        {"D/DX nest", AB_CASD, 23, "SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(SIN(X))))))))))))))))))))", NULL},
        {"D/DX product", AB_CASD, 23, "X" "\xc3\x97" "SIN(X)" "\xc3\x97" "COS(X)" "\xc3\x97" "EXP(X)" "\xc3\x97" "LN(X)" "\xc3\x97"
                                      "TAN(X)" "\xc3\x97" "SQRT(X)" "\xc3\x97" "ASIN(X)" "\xc3\x97" "ATAN(X)" "\xc3\x97" "SINH(X)", NULL},
        {"D/DX product 6", AB_CASD, 23, "X" "\xc3\x97" "SIN(X)" "\xc3\x97" "COS(X)" "\xc3\x97" "EXP(X)" "\xc3\x97" "LN(X)" "\xc3\x97" "TAN(X)", NULL},
        {"D/DX quotient", AB_CASD, 23, "(X^3+2\xc3\x97X^2-X+1)\xc3\xb7(X^2+1)^3\xc3\x97SQRT(X^2+X+1)\xc3\xb7LN(X^2+2)", NULL},
        {"SIMP sum", AB_CASSIMP, 0, "X+2\xc3\x97X+3\xc3\x97Y+4\xc3\x97X\xc3\x97Y+5\xc3\x97Y\xc3\x97X+6\xc3\x97X^2+7\xc3\x97X\xc3\x97X-8\xc3\x97Y^2+9\xc3\x97Y\xc3\x97Y+X\xc3\xb7X+(X+Y)\xc3\x97(X+Y)-(X-Y)^2+SIN(X)^2+COS(X)^2", NULL},
        {"EXPND (X+Y+1)^5", AB_CASEXP, 0, "(X+Y+1)^5", NULL},
        {"EXPND (X+1)^9(Y+1)^3", AB_CASEXP, 0, "(X+1)^9\xc3\x97(Y+1)^3", NULL},
        {"COLCT (X+Y+Z)^4", AB_CASCOL, 23, "(X+Y+Z)^4", NULL},
        {"SUBST poly", AB_CASSUB, 23, "X^5+X^4+X^3+X^2+X+1", "(Y+1)^2"},
        {"guard X^X^..60", AB_CASSIMP, 0, NULL, NULL},
    };
    char chain[200];
    ab_calc *t = heap_caps_calloc(1, sizeof *t, MALLOC_CAP_SPIRAM);
    if (!t) {
        printf("casbench: no memory\n");
        return;
    }
    for (int i = 0; i < 60; i++) chain[2 * i] = 'X', chain[2 * i + 1] = '^';
    chain[119] = '\0';
    for (size_t k = 0; k < sizeof K / sizeof K[0]; k++) {
        ab_init(t);
        ab_do_arg(t, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(t, K[k].eqn ? K[k].eqn : chain);
        int n = t->eqn_count;
        int64_t t0 = esp_timer_get_time();
        abn_status st = ab_do_arg(t, (ab_op)K[k].op, K[k].var);
        if (K[k].op == AB_CASSUB && st == ABN_OK) {
            snprintf(t->ebuf, sizeof t->ebuf, "%s", K[k].repl);
            t->ecur = (int)strlen(t->ebuf);
            st = ab_do(t, AB_ENTER);
        }
        int64_t t1 = esp_timer_get_time();
        printf("casbench: %-22s %lld us st %d added %d len %d stack-free %u\n", K[k].name, (long long)(t1 - t0),
               (int)st, t->eqn_count - n, t->eqn_count > n ? (int)strlen(t->eqn[t->eqn_cur]) : 0,
               (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
    printf("casbench: done heap %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    heap_caps_free(t);
}

/* POLBENCH (unit 034 rule 3): ->POL's time on the S3, before (the engine's hypot and atan2, and in DEG the one
   product, which was fn_topol's route) and after (fn_topol: hp_hypot, hp_atan2 at 56 digits rounded once); and
   ->REC's in RAD, fn_torec now. Non-integer arguments (a benchmark lesson of unit 032). */
static void polbench(void)
{
    static const char *const C[][3] = {
        {"typical", "3.7", "2.5"},
        {"typed, quadrant 2", "-923.37", "72.68"},
        {"34 digits", "1.234567890123456789012345678901234", "-9.876543210987654321098765432109876"},
        {"near an axis", "6.3553389498124625953E-755", "-8.9764145955087792958E2042"},
        {"range ends", "7.6297677379534555E6140", "-2.0816875126460719E6144"},
    };
    for (size_t i = 0; i < sizeof C / sizeof C[0]; i++) {
        abn x, y, r, th, d;
        abn_from_text(&x, C[i][1]);
        abn_from_text(&y, C[i][2]);
        abn_from_text(&d, "57.29577951308232087679815481410517");
        const int reps = 50;
        int64_t t0 = esp_timer_get_time();
        for (int n = 0; n < reps; n++) {
            abn_hypot(&r, &x, &y);
            abn_atan2(&th, &y, &x);
            abn_mul(&th, &th, &d);
        }
        int64_t t1 = esp_timer_get_time();
        abn_status s1 = ABN_OK, s2 = ABN_OK;
        for (int n = 0; n < reps; n++) {
            int m = ar_mark();
            s1 = fn_topol(&r, &th, &x, &y, FN_DEG);
            ar_release(m);
        }
        int64_t t2 = esp_timer_get_time();
        for (int n = 0; n < reps; n++) {
            int m = ar_mark();
            s2 = fn_torec(&r, &th, &x, &y, FN_RAD);
            ar_release(m);
        }
        int64_t t3 = esp_timer_get_time();
        printf("polbench: %-18s before %lld us  ->POL DEG %lld us (st %d)  ->REC RAD %lld us (st %d)\n", C[i][0],
               (long long)(t1 - t0) / reps, (long long)(t2 - t1) / reps, (int)s1, (long long)(t3 - t2) / reps, (int)s2);
    }
    printf("polbench: done\n");
}

/* BENCH: what PSRAM costs and gives (decision 34's ECC question: measure before turning it on). A
   second calculator in PSRAM, as Rev A's two-calculator state would be; a 2,000-line program run in
   it (its arena pool in PSRAM too); 200 saves and loads of the state image in PSRAM. */
static void bench(void)
{
    ab_calc *c2 = heap_caps_calloc(1, sizeof *c2, MALLOC_CAP_SPIRAM);
    if (!c2) {
        printf("bench: no room for a second calculator\n");
        return;
    }
    ab_init(c2);
    ab_do(c2, AB_PRGM);
    ab_do_arg(c2, AB_LBL, 1);
    ab_do(c2, AB_K1); ab_do(c2, AB_K0); ab_do(c2, AB_K0); ab_do(c2, AB_K0);
    ab_do_arg(c2, AB_STO, 'N' - 'A');
    ab_do_arg(c2, AB_DSE, 'N' - 'A');
    ab_do_arg(c2, AB_GTO, 1 * 1000 + 4);
    ab_do(c2, AB_RTN);
    ab_do(c2, AB_PRGM);
    int64_t t0 = esp_timer_get_time();
    ab_do_arg(c2, AB_XEQ, 1 * 1000);
    int64_t t1 = esp_timer_get_time();
    size_t n = 0;
    for (int i = 0; i < 200; i++) n = ab_state_save(c2, image, IMAGE_CAP);
    int64_t t2 = esp_timer_get_time();
    for (int i = 0; i < 200 && n; i++) ab_state_load(c2, image, n);
    int64_t t3 = esp_timer_get_time();
    printf("bench: program %lld us (lines %ld), save %lld us, load %lld us (%u bytes); PSRAM total %u free %u; calc %u bytes\n",
           (long long)(t1 - t0), c2->run_lines, (long long)(t2 - t1) / 200, (long long)(t3 - t2) / 200, (unsigned)n,
           (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)sizeof *c2);
    heap_caps_free(c2);
}

/* ---- the tasks ---- */

/* A key from the touch panel or the console. C or R/S during a run interrupt it here and now
   (the calc task is inside a slice); that press is spent, so only its activity is queued: queued
   as a key it would reach app_key after the run stopped and start it again (R/S resumes). */
static void key_pressed(int key)
{
    const km_action *f = km_lookup(key, KM_FACE);
    if ((app.running || app.computing) && f->kind == KM_OP && (f->op == AB_CANCEL || f->op == AB_RS)) {
        app_interrupt(&app);
        event e = {EV_TOUCHED, key, 0};
        xQueueSend(events, &e, 0);
        return;
    }
    event e = {EV_KEY, key, 0};
    xQueueSend(events, &e, 0);
}

static void touch_task(void *arg)
{
    (void)arg;
    bool was_down = false;
    int down_key = -1;                          /* the key the finger went down on, for its release */
    for (;;) {
        int x = 0, y = 0, n = 0;
        if (tp_read(&x, &y, &n)) {
            bool down = n > 0;
            if (!down && was_down && down_key >= 0) {
                event e = {EV_KEYUP, down_key, 0};
                xQueueSend(events, &e, 0);
                down_key = -1;
            }
            if (down && !was_down) {
                int key = si_key_at(x, y);
                printf("touch: %d,%d key %d\n", x, y, key);
                if (x2) {
                    x2 = false;                 /* any tap leaves the x2 view */
                } else if (si_on_frame(x, y)) {
                    x2 = true;
                } else if (key >= 0) {
                    key_pressed(key);
                    down_key = key;
                }
            }
            was_down = down;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void console_task(void *arg)
{
    (void)arg;
    char line[64];
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
        event e = {-1, 0, 0};
        if (line[0] == 'K') {
            key_pressed(atoi(line + 1));
            continue;
        } else if (line[0] == 'U' && line[1] >= '0' && line[1] <= '9') {   /* U<n>: key n released (unit 030) */
            e = (event){EV_KEYUP, atoi(line + 1), 0};
        } else if (!strcmp(line, "PRGM")) e = (event){EV_OP, AB_PRGM, 0};   /* until the menus: #3866 */
        else if (!strcmp(line, "X")) e.kind = EV_DUMP;
        else if (!strcmp(line, "SAVE")) e.kind = EV_SAVE;
        else if (!strcmp(line, "WIPE")) e.kind = EV_WIPE;
        else if (!strcmp(line, "RESTART")) e.kind = EV_RESTART;
        else if (!strcmp(line, "BENCH")) e.kind = EV_BENCH;
        else if (!strcmp(line, "POWBENCH")) e.kind = EV_POWBENCH;
        else if (!strcmp(line, "TBLBENCH")) e.kind = EV_TBLBENCH;  /* unit 032 rule 7: a screenful's time */
        else if (!strcmp(line, "GRBENCH")) e.kind = EV_GRBENCH;    /* unit 033 rule 8: a full plot's time */
        else if (!strcmp(line, "FRAMEBENCH")) e.kind = EV_FRAMEBENCH;  /* unit 037 rule 6: full frames a second */
        else if (!strcmp(line, "POLBENCH")) e.kind = EV_POLBENCH;  /* unit 034 rule 3: ->POL before and after */
        else if (!strcmp(line, "ZOOMBENCH")) e.kind = EV_ZOOMBENCH;  /* unit 040: a redraw after each zoom */
        else if (!strcmp(line, "CASBENCH")) e.kind = EV_CASBENCH;    /* CAS 004: each step's time, the stack */
        else if (!strcmp(line, "DIFFBENCH")) e.kind = EV_DIFFBENCH;  /* apps.h v1.2 item 1: the line diff's time */
        else if (!strncmp(line, "TIME ", 5)) {      /* TIME hh:mm[:ss] (unit 027 rule 4) */
            snprintf(time_text, sizeof time_text, "%.15s", line + 5);
            e.kind = EV_TIME;
        }
        if (e.kind >= 0) xQueueSend(events, &e, portMAX_DELAY);
        else if (line[0]) printf("console: ? %s\n", line);
    }
}

static void calc_task(void *arg)
{
    (void)arg;
    pwr_inputs in = {true, true, -1, false};    /* no charger pins or gauge on the stand-in */
    bool was_running = false;
    render(0);
    printf("STANDIN READY\n");
    for (;;) {
        event ev;
        /* unit 023: block until an event or the app's next deadline (on USB it never light-sleeps:
           rule 5, and its RGB panel needs the clocks) */
        uint32_t ms = app_next_wake(&app, &in, now_ms());
        TickType_t wait = ms >= 3600000u ? portMAX_DELAY : pdMS_TO_TICKS(ms);
        bool got = xQueueReceive(events, &ev, wait) == pdTRUE;
        uint32_t now = now_ms();
        if (got && ev.kind == EV_KEY) {
            printf("key %d\n", ev.a);
            memory_clear_msg = false;
            app_key(&app, ev.a, now);
        } else if (got && ev.kind == EV_KEYUP) {
            printf("key %d up\n", ev.a);
            app_key_up(&app, ev.a, now);
        } else if (got && ev.kind == EV_TOUCHED) {
            printf("key %d: interrupted the run\n", ev.a);
            app.last_key_ms = now;              /* activity, and nothing more */
            pwr_activity(&app.pwr, now);
        } else if (got && ev.kind == EV_OP) {
            ab_do_arg(calc, (ab_op)ev.a, ev.b);
            app.dirty = true;
            printf("op %d: prgm=%d\n", ev.a, calc->prgm);
        } else if (got && ev.kind == EV_DUMP) {
            dump();
        } else if (got && ev.kind == EV_SAVE) {
            save();
        } else if (got && ev.kind == EV_WIPE) {
            printf("wipe: %s\n", flash.ctx && esp_partition_erase_range(flash.ctx, 0, 2 * flash.slot_size) == ESP_OK
                                     ? "both slots erased" : "FAILED");
        } else if (got && ev.kind == EV_FRAMEBENCH) {
            /* unit 037 rule 6: the stand-in's full-frame path (the app's draw, or the calculator's screen,
               into the frame, then its blit to the panel), 50 frames timed */
            int64_t t0 = esp_timer_get_time();
            for (int i = 0; i < 50; i++) render(0);
            int64_t t1 = esp_timer_get_time();
            printf("framebench: %s, 50 frames in %lld ms: %.1f frames a second\n", app_running(&app) ? app.app->name : "calculator",
                   (long long)(t1 - t0) / 1000, 50.0e6 / (double)(t1 - t0));
        } else if (got && ev.kind == EV_GRBENCH) {
            grbench();
        } else if (got && ev.kind == EV_TBLBENCH) {
            tblbench();
        } else if (got && ev.kind == EV_POWBENCH) {
            powbench();
        } else if (got && ev.kind == EV_POLBENCH) {
            polbench();
        } else if (got && ev.kind == EV_ZOOMBENCH) {
            zoombench();
        } else if (got && ev.kind == EV_CASBENCH) {
            casbench();
        } else if (got && ev.kind == EV_DIFFBENCH) {
            diffbench();
        } else if (got && ev.kind == EV_BENCH) {
            bench();
        } else if (got && ev.kind == EV_TIME) {
            int64_t w;
            if (dev_parse_time(&app, now, time_text, &w)) {
                dev_set_wall(NULL, w);
                app_set_wall(&app, w, now);
                printf("time: set %02d:%02d:%02d\n", (int)(w % 86400 / 3600), (int)(w % 3600 / 60), (int)(w % 60));
            } else {
                printf("time: ? %s (hh:mm or hh:mm:ss)\n", time_text);
            }
        } else if (got && ev.kind == EV_RESTART) {
            fflush(stdout);
            esp_restart();
        }
        app_action act = app_tick(&app, &in, now);
        if (app.tone >= 0) {                    /* no buzzer here: the console says what would sound */
            if (app.tone_use == DEV_USE_APP) printf("tone: app %u Hz %u ms level %d\n", app.tone_hz, app.tone_ms, app.tone);
            else printf("tone: use %d level %d\n", app.tone_use, app.tone);
            app.tone = -1;
        }
        apps_save();                            /* unit 037 */
        if (app.running != was_running) {
            char stop[32];
            ab_stop_text(calc, stop);
            printf(app.running ? "run: started\n" : "run: stopped (%s)\n", stop);
            was_running = app.running;
        }
        if (act == APP_SAVE) {
            save();
        } else if (act == APP_OFF) {
            save();
            printf("off\n");
            exio_set(exio & ~EXIO_DISP);        /* the stand-in's off: backlight off until a press */
            do xQueueReceive(events, &ev, portMAX_DELAY);
            while (ev.kind == EV_KEYUP);        /* a release (SHOW held past the off time) does not wake it */
            exio_set(exio | EXIO_DISP);
            app.off = false;
            pwr_activity(&app.pwr, now_ms());
            printf("on\n");
        }
        if (act == APP_RENDER || got) render(0);
    }
}

static uint32_t clock_cb(void *ctx)
{
    (void)ctx;
    return now_ms();
}

/* SYS: the stand-in has no gauge and no charger pins; it is on USB */
static uint32_t rng_cb(void *ctx)        /* apps.h v1.2: app_seed, the hardware RNG */
{
    (void)ctx;
    return esp_random();
}

static void sysinfo_cb(void *ctx, app_sysinfo *o)
{
    (void)ctx;
    o->soc = -1;
    o->chg_n = true;
    o->pgood_n = false;
    o->vbat_uv = 0;
    dev_sysinfo_common(o, flash.ctx && last_image <= flash.slot_size ? flash.slot_size - last_image : 0,
                       flash.ctx ? flash.slot_size : 0);
}

void app_main(void)
{
    /* the calc task's stack first, before the keyboard image and the panel's buffers split the
       internal heap: 32 KB in one block (it failed at boot once those had: largest block 31 KB) */
    static StaticTask_t calc_tcb;
    StackType_t *calc_stack = heap_caps_malloc(CALC_STACK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    events = xQueueCreate(32, sizeof(event));
    keys_img = heap_caps_malloc(SI_IMG_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    calc = heap_caps_calloc(1, sizeof *calc, MALLOC_CAP_SPIRAM);
    image = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM);
    if (!keys_img || !calc || !image) {
        printf("standin: out of memory\n");
        return;
    }
    si_init();
    si_keys_state ks = {0, false, -1, false};
    si_draw_keys(keys_img, &ks);
    keys_drawn = ks;
    board_init();
    screen_init();
    store_init();
    ab_init(calc);
    load();
    app_init(&app, calc, now_ms());
    app.busy = on_busy;
    app.set = loaded;
    /* unit 027: the clock from the RTC (unset after the cell was out: before 2025), the callbacks,
       and ON's chirp: every boot of the stand-in is a power-on */
    app_set_wall(&app, dev_wall_now(), now_ms());
    app.set_wall = dev_set_wall;
    app.clock_ms = clock_cb;
    app.sysinfo = sysinfo_cb;
    app.rng = rng_cb;                           /* apps.h v1.2: app_seed */
    app_powered_on(&app);
    {   /* UNDO's two images (unit 021), in PSRAM beside the store's; without them UNDO does nothing */
        uint8_t *u0 = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM), *u1 = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM);
        if (!u0 || !u1) {
            printf("standin: no memory for UNDO\n");
            heap_caps_free(u0);
            heap_caps_free(u1);
            u0 = u1 = NULL;
        }
        app_undo_buffers(&app, u0, u1, IMAGE_CAP);
        app_graph_buffer(&app, heap_caps_malloc(sizeof(app_graph), MALLOC_CAP_SPIRAM));   /* unit 033 */
        app_ctx_buffer(&app, heap_caps_malloc(APP_CTX_MAX, MALLOC_CAP_SPIRAM), APP_CTX_MAX);  /* unit 037 */
        app_save_buffers(&app, heap_caps_malloc(APP_SAVES_IMG_MAX, MALLOC_CAP_SPIRAM), APP_SAVES_IMG_MAX,
                         heap_caps_malloc(APP_SAVE_MAX, MALLOC_CAP_SPIRAM), APP_SAVE_MAX);   /* apps.h v1.2 */
        apps_buf = heap_caps_malloc(APP_SAVES_IMG_MAX, MALLOC_CAP_SPIRAM);
        apps_load();
    }
    printf("standin: heap internal %u (largest block %u), PSRAM %u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    /* the stacks stay internal: the calc task writes flash, when PSRAM is out of reach */
    bool ok = calc_stack && xTaskCreateStaticPinnedToCore(calc_task, "calc", CALC_STACK, NULL, 4, calc_stack, &calc_tcb, 1);
    ok = ok && xTaskCreatePinnedToCore(touch_task, "touch", 4096, NULL, 5, NULL, 0) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 3, NULL, 0) == pdPASS;
    if (!ok) printf("standin: a task could not be created (heap internal %u)\n",
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}
