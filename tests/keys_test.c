/* keys_test: the matrix scan against a simulated matrix (unit 018 acceptance). The model: a set
 * of switches closed; driving column c low pulls low the rows of c's closed switches (the diodes
 * keep every other path open); bounce is modelled as a switch read through a scripted pattern. */
#include <stdio.h>
#include <string.h>

#include "keys.h"

typedef struct {
    bool closed[KB_KEYS];
    int col;
} matrix;

static void drive(void *ctx, int col) { ((matrix *)ctx)->col = col; }

static uint8_t rd(void *ctx)
{
    matrix *m = ctx;
    uint8_t rows = 0;
    if (m->col < 0) return 0;
    for (int r = 0; r < KB_ROWS; r++)
        if (m->closed[r * KB_COLS + m->col]) rows |= (uint8_t)(1u << r);
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
    kb_io io = {&m, drive, rd};
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
    printf("keys_test: %d keys alone, %d pairs, %d bounce patterns, rectangle, short tap: %s\n", keys, pairs, bounces,
           bad ? "FAIL" : "all as expected");
    return bad ? 1 : 0;
}
