/* keys: the STU-32 key matrix (unit 018 scope 2; docs/hw/keys.py, circuit.py). 8 rows (ROW0..7 on
 * IO1, IO2, IO4..IO9, 100k pull-ups) by 6 columns (COL0..5 on IO10..IO15); each key is ROWr ->
 * diode (anode at the row) -> switch -> COLc, so driving one column low pulls the rows of its
 * pressed keys low, and the diodes stop any ghost. Portable C: the device gives the two I/O calls;
 * the host test gives a simulated matrix.
 *
 * kb_scan is one pass over the columns (call it every millisecond). A key's change is taken once
 * it has read the same KB_DEBOUNCE passes running; each press becomes a key-down event, in the
 * order taken (within one pass, by column then row, then C from its own pin when it has one).
 * Keys are numbered row * 6 + column; the layout has 43 (rows 0-2 six keys, rows 3-7 five:
 * stu32-v0.json). From Rev A pass 6 (decision 34) C, key 42, is on its own pin (IO0) and the
 * matrix's row 7 column 0 is no longer a key: read_on gives it. */
#ifndef KEYS_H
#define KEYS_H

#include <stdbool.h>
#include <stdint.h>

#define KB_ROWS 8
#define KB_COLS 6
#define KB_KEYS (KB_ROWS * KB_COLS)
#define KB_DEBOUNCE 5               /* passes (ms at 1 kHz) */
#define KB_ON_KEY (7 * KB_COLS + 0) /* C / ON: row 7 (IO9, an RTC GPIO: the deep-sleep wake) */

typedef struct {
    void *ctx;
    void (*drive)(void *ctx, int col);  /* drive column col low and release the others; -1 all */
    uint8_t (*read)(void *ctx);         /* the rows: bit r set when ROWr reads low (pressed) */
    bool (*read_on)(void *ctx);         /* Rev A pass 6 (decision 34): C (ON, key 42) on its own pin,
                                           IO0, true when pressed; NULL: key 42 is in the matrix */
} kb_io;

typedef struct {
    uint8_t count[KB_KEYS];             /* debounce integrators, 0..KB_DEBOUNCE */
    bool down[KB_KEYS];                 /* the debounced state */
    uint8_t q[32];                      /* key-down events not yet taken */
    int qh, qn;
} kb_state;

/* Whether (row, col) is a key of the layout. */
bool kb_exists(int row, int col);

void kb_init(kb_state *k);
void kb_scan(kb_state *k, const kb_io *io);
/* The next key-down event (row * 6 + col), or -1. */
int kb_next(kb_state *k);
/* Whether the scan may stop (unit 023): every key reads up and every debounce integrator has
   settled at 0, so kb_scan would change nothing until a key goes down. */
bool kb_idle(const kb_state *k);

#endif
