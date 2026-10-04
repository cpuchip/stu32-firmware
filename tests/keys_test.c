/* keys_test: the matrix scan against a simulated matrix (unit 018 acceptance). The model: a set
 * of switches closed; driving column c low pulls low the rows of c's closed switches (the diodes
 * keep every other path open); bounce is modelled as a switch read through a scripted pattern. */
#include <stdio.h>
#include <string.h>

#include "keys.h"

typedef struct {
    bool closed[KB_KEYS];
    int col;
    bool on_wire;           /* Rev A pass 6: the ON switch on its own pin (IO0), not in the matrix */
    bool stray_r7c0;        /* a contact at the matrix's old ON position, which must be ignored then */
} matrix;

static void drive(void *ctx, int col) { ((matrix *)ctx)->col = col; }

static bool rd_on(void *ctx)
{
    return ((matrix *)ctx)->closed[KB_ON_KEY];
}

static uint8_t rd(void *ctx)
{
    matrix *m = ctx;
    uint8_t rows = 0;
    if (m->col < 0) return 0;
    for (int r = 0; r < KB_ROWS; r++)
        if (m->closed[r * KB_COLS + m->col] && !(m->on_wire && r * KB_COLS + m->col == KB_ON_KEY))
            rows |= (uint8_t)(1u << r);
    if (m->stray_r7c0 && m->col == KB_ON_KEY % KB_COLS) rows |= (uint8_t)(1u << (KB_ON_KEY / KB_COLS));
    return rows;
}

static int bad;

static void expect(bool ok, const char *what, int a, int b)
{
    if (!ok && bad++ < 20) printf("keys_test: %s (%d, %d)\n", what, a, b);
}

int main(void)
{
    static matrix m;
    kb_io io = {&m, drive, rd, NULL};      /* ON in the matrix: the wiring before Rev A pass 6 */
    kb_state k;
    int keys = 0, pairs = 0, bounces = 0;
    /* every key alone: one event, the right number, none repeated while held */
    for (int key = 0; key < KB_KEYS; key++) {
        if (!kb_exists(key / KB_COLS, key % KB_COLS)) continue;
        keys++;
        kb_init(&k);
        memset(&m, 0, sizeof m);
        m.closed[key] = true;
        for (int t = 0; t < 50; t++) kb_scan(&k, &io);
        expect(kb_next(&k) == key, "a key alone gives its own event", key, 0);
        expect(kb_next(&k) == -1, "a held key gives one event", key, 0);
        m.closed[key] = false;
        for (int t = 0; t < 50; t++) kb_scan(&k, &io);
        expect(kb_next(&k) == -1, "a release gives no event", key, 0);
    }
    /* every pair, pressed one after the other and held together: two events in that order and no
       third (no ghost: a shared row or column makes no phantom key) */
    for (int a = 0; a < KB_KEYS; a++)
        for (int b = 0; b < KB_KEYS; b++) {
            if (a == b || !kb_exists(a / KB_COLS, a % KB_COLS) || !kb_exists(b / KB_COLS, b % KB_COLS)) continue;
            pairs++;
            kb_init(&k);
            memset(&m, 0, sizeof m);
            m.closed[a] = true;
            for (int t = 0; t < 20; t++) kb_scan(&k, &io);
            m.closed[b] = true;
            for (int t = 0; t < 20; t++) kb_scan(&k, &io);
            expect(kb_next(&k) == a && kb_next(&k) == b && kb_next(&k) == -1, "a pair gives two events in order", a, b);
        }
    /* three keys at the corners of a rectangle: a matrix without diodes would see the fourth */
    {
        int r1 = 1, r2 = 4, c1 = 0, c2 = 3;
        kb_init(&k);
        memset(&m, 0, sizeof m);
        m.closed[r1 * KB_COLS + c1] = m.closed[r1 * KB_COLS + c2] = m.closed[r2 * KB_COLS + c1] = true;
        for (int t = 0; t < 30; t++) kb_scan(&k, &io);
        int n = 0;
        while (kb_next(&k) >= 0) n++;
        expect(n == 3 && !k.down[r2 * KB_COLS + c2], "three keys of a rectangle: no ghost at the fourth", n, 0);
    }
    /* bounce: contact chatter shorter than the debounce gives one event; a release with chatter
       gives none; pressing again gives a second */
    for (unsigned seed = 1; seed <= 200; seed++) {
        int key = 7 * KB_COLS + 2;
        unsigned s = seed;
        kb_init(&k);
        memset(&m, 0, sizeof m);
        bounces++;
        for (int t = 0; t < 4; t++) {                   /* chatter: never 5 the same in a row */
            s = s * 1103515245u + 12345u;
            m.closed[key] = (s >> 16) & 1;
            kb_scan(&k, &io);
            m.closed[key] = !m.closed[key];
            kb_scan(&k, &io);
        }
        m.closed[key] = true;
        for (int t = 0; t < 30; t++) kb_scan(&k, &io);
        int first = kb_next(&k);
        for (int t = 0; t < 4; t++) {                   /* release chatter */
            m.closed[key] = t & 1;
            kb_scan(&k, &io);
        }
        m.closed[key] = false;
        for (int t = 0; t < 30; t++) kb_scan(&k, &io);
        int none = kb_next(&k);
        m.closed[key] = true;
        for (int t = 0; t < 30; t++) kb_scan(&k, &io);
        int second = kb_next(&k);
        expect(first == key && none == -1 && second == key && kb_next(&k) == -1, "bounce: one event per press", (int)seed, first);
    }
    /* a tap shorter than the debounce is no key */
    kb_init(&k);
    memset(&m, 0, sizeof m);
    m.closed[3] = true;
    for (int t = 0; t < KB_DEBOUNCE - 1; t++) kb_scan(&k, &io);
    m.closed[3] = false;
    for (int t = 0; t < 30; t++) kb_scan(&k, &io);
    expect(kb_next(&k) == -1, "a tap shorter than the debounce is no key", 3, 0);
    expect(keys == 43, "the layout has 43 keys", keys, 0);

    /* Rev A pass 6 (abacus decision 34): C (ON, key 42) on its own input, IO0, debounced like the
       matrix keys; the matrix's row 7 column 0 is no longer a key */
    {
        kb_io io_on = {&m, drive, rd, rd_on};
        int on_pairs = 0;
        kb_init(&k);
        memset(&m, 0, sizeof m);
        m.on_wire = true;
        m.closed[KB_ON_KEY] = true;
        for (int t = 0; t < 50; t++) kb_scan(&k, &io_on);
        expect(kb_next(&k) == KB_ON_KEY && kb_next(&k) == -1, "ON from its own input: one event", KB_ON_KEY, 0);
        m.closed[KB_ON_KEY] = false;
        for (int t = 0; t < 50; t++) kb_scan(&k, &io_on);
        expect(kb_next(&k) == -1, "ON released: no event", KB_ON_KEY, 0);
        kb_init(&k);
        memset(&m, 0, sizeof m);
        m.on_wire = true;
        m.closed[KB_ON_KEY] = true;
        for (int t = 0; t < KB_DEBOUNCE - 1; t++) kb_scan(&k, &io_on);
        m.closed[KB_ON_KEY] = false;
        for (int t = 0; t < 30; t++) kb_scan(&k, &io_on);
        expect(kb_next(&k) == -1, "ON tapped shorter than the debounce: no key", KB_ON_KEY, 0);
        /* with every matrix key, both orders: two events in order */
        for (int b = 0; b < KB_KEYS; b++) {
            if (b == KB_ON_KEY || !kb_exists(b / KB_COLS, b % KB_COLS)) continue;
            for (int order = 0; order < 2; order++) {
                int first = order ? b : KB_ON_KEY, second = order ? KB_ON_KEY : b;
                kb_init(&k);
                memset(&m, 0, sizeof m);
                m.on_wire = true;
                m.closed[first] = true;
                for (int t = 0; t < 20; t++) kb_scan(&k, &io_on);
                m.closed[second] = true;
                for (int t = 0; t < 20; t++) kb_scan(&k, &io_on);
                expect(kb_next(&k) == first && kb_next(&k) == second && kb_next(&k) == -1, "ON with a matrix key", first,
                       second);
                on_pairs++;
            }
        }
        expect(on_pairs == 2 * 42, "ON paired with each of the 42 matrix keys, both orders", on_pairs, 0);
        /* a stray contact at the matrix's old ON position is ignored */
        kb_init(&k);
        memset(&m, 0, sizeof m);
        m.on_wire = true;
        m.stray_r7c0 = true;
        for (int t = 0; t < 50; t++) kb_scan(&k, &io_on);
        expect(kb_next(&k) == -1 && !k.down[KB_ON_KEY], "row 7 column 0 of the matrix: not a key now", 0, 0);
        /* and without its own input, ON is still the matrix's (the old wiring) */
        kb_init(&k);
        memset(&m, 0, sizeof m);
        m.stray_r7c0 = true;
        for (int t = 0; t < 50; t++) kb_scan(&k, &io);
        expect(kb_next(&k) == KB_ON_KEY, "no ON input: key 42 from the matrix as before", 0, 0);
    }
    printf("keys_test: %d keys alone, %d pairs, %d bounce patterns, rectangle, short tap: %s\n", keys, pairs, bounces,
           bad ? "FAIL" : "all as expected");
    return bad ? 1 : 0;
}
