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

#define I2C_SDA 8
#define I2C_SCL 9
#define TP_INT 4
#define CH422_SYS 0x24                  /* CH422 datasheet 6.1: 48H on the wire */
#define CH422_IO 0x38                   /* 6.3: 70H */
#define EXIO_TP_RST (1u << 1)           /* schematic: CH422G IO1..IO3 */
#define EXIO_DISP (1u << 2)
#define EXIO_LCD_RST (1u << 3)
#define GT911_ADDR 0x5D

enum { EV_KEY = 0, EV_OP, EV_DUMP, EV_SAVE, EV_WIPE, EV_RESTART, EV_TOUCHED, EV_BENCH };

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
}

static void save(void)
{
    size_t n = ab_state_save(calc, NULL, 0);
    if (!flash.ctx || n == 0 || n > IMAGE_CAP) {
        printf("save: not saved (%u bytes)\n", (unsigned)n);
        return;
    }
    ab_state_save(calc, image, IMAGE_CAP);
    int64_t t0 = esp_timer_get_time();
    int rc = ab_store_save(&flash, image, (uint32_t)n);
    printf("save: %u bytes, %s, %lld ms\n", (unsigned)n, rc == 0 ? "ok" : "FAILED",
           (long long)((esp_timer_get_time() - t0) / 1000));
    if (rc == 0) app_saved(&app);
}

static void load(void)
{
    uint32_t n = flash.ctx ? ab_store_load(&flash, image, IMAGE_CAP) : 0;
    abn_status st = n ? ab_state_load(calc, image, n) : ABN_BAD_IMAGE;
    if (st == ABN_OK) {
        printf("load: %u bytes ok\n", (unsigned)n);
    } else {
        ab_memory_clear(calc);          /* state.h: MEMORY CLEAR, shown (UG p.B-3) */
        memory_clear_msg = true;
        printf("load: %s: MEMORY CLEAR\n", n ? "image refused" : "no image");
    }
}

/* ---- drawing ---- */

static si_keys_state keys_drawn = {-1, false, -1};

static void render(int busy)
{
    screen_ui ui;
    app_ui(&app, &ui);
    if (busy) ui.busy = busy;
    if (memory_clear_msg) ui.prompt = "MEMORY CLEAR";
    int back = 1 - front;
    screen_render(calc, &ui, &fb[back]);
    front = back;
    si_keys_state ks = {app.shift, app.prompt_op >= 0, -1};
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
    printf("pixels: gold key %04X, blue key %04X (gold %04X, blue %04X, key %04X)\n", pixel_at(440, 345),
           pixel_at(440, 405), SI_PALETTE[SI_GOLD], SI_PALETTE[SI_BLUE], SI_PALETTE[SI_KEY]);
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
    if (app.running && f->kind == KM_OP && (f->op == AB_CANCEL || f->op == AB_RS)) {
        ab_interrupt(calc);
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
    for (;;) {
        int x = 0, y = 0, n = 0;
        if (tp_read(&x, &y, &n)) {
            bool down = n > 0;
            if (down && !was_down) {
                int key = si_key_at(x, y);
                printf("touch: %d,%d key %d\n", x, y, key);
                if (x2) {
                    x2 = false;                 /* any tap leaves the x2 view */
                } else if (si_on_frame(x, y)) {
                    x2 = true;
                } else if (key >= 0) {
                    key_pressed(key);
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
        } else if (!strcmp(line, "PRGM")) e = (event){EV_OP, AB_PRGM, 0};   /* until the menus: #3866 */
        else if (!strcmp(line, "X")) e.kind = EV_DUMP;
        else if (!strcmp(line, "SAVE")) e.kind = EV_SAVE;
        else if (!strcmp(line, "WIPE")) e.kind = EV_WIPE;
        else if (!strcmp(line, "RESTART")) e.kind = EV_RESTART;
        else if (!strcmp(line, "BENCH")) e.kind = EV_BENCH;
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
        bool got = xQueueReceive(events, &ev, app.running ? 0 : pdMS_TO_TICKS(20)) == pdTRUE;
        uint32_t now = now_ms();
        if (got && ev.kind == EV_KEY) {
            printf("key %d\n", ev.a);
            memory_clear_msg = false;
            app_key(&app, ev.a, now);
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
        } else if (got && ev.kind == EV_BENCH) {
            bench();
        } else if (got && ev.kind == EV_RESTART) {
            fflush(stdout);
            esp_restart();
        }
        app_action act = app_tick(&app, &in, now);
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
            xQueueReceive(events, &ev, portMAX_DELAY);
            exio_set(exio | EXIO_DISP);
            app.off = false;
            pwr_activity(&app.pwr, now_ms());
            printf("on\n");
        }
        if (act == APP_RENDER || got) render(0);
    }
}

void app_main(void)
{
    events = xQueueCreate(32, sizeof(event));
    keys_img = heap_caps_malloc(SI_IMG_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    calc = heap_caps_calloc(1, sizeof *calc, MALLOC_CAP_SPIRAM);
    image = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM);
    if (!keys_img || !calc || !image) {
        printf("standin: out of memory\n");
        return;
    }
    si_init();
    si_keys_state ks = {0, false, -1};
    si_draw_keys(keys_img, &ks);
    keys_drawn = ks;
    board_init();
    screen_init();
    store_init();
    ab_init(calc);
    load();
    app_init(&app, calc, now_ms());
    app.busy = on_busy;
    {   /* UNDO's two images (unit 021), in PSRAM beside the store's; without them UNDO does nothing */
        uint8_t *u0 = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM), *u1 = heap_caps_malloc(IMAGE_CAP, MALLOC_CAP_SPIRAM);
        if (!u0 || !u1) {
            printf("standin: no memory for UNDO\n");
            heap_caps_free(u0);
            heap_caps_free(u1);
            u0 = u1 = NULL;
        }
        app_undo_buffers(&app, u0, u1, IMAGE_CAP);
    }
    printf("standin: heap internal %u (largest block %u), PSRAM %u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    /* the stacks stay internal: the calc task writes flash, when PSRAM is out of reach */
    bool ok = xTaskCreatePinnedToCore(calc_task, "calc", CALC_STACK, NULL, 4, NULL, 1) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(touch_task, "touch", 4096, NULL, 5, NULL, 0) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 3, NULL, 0) == pdPASS;
    if (!ok) printf("standin: a task could not be created (heap internal %u)\n",
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}
