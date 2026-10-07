/* keys: the matrix scan and debounce (see keys.h). */
#include <string.h>

#include "keys.h"

bool kb_exists(int row, int col)
{
    if (row < 0 || row >= KB_ROWS || col < 0 || col >= KB_COLS) return false;
    return row <= 2 ? true : col < 5;
}

void kb_init(kb_state *k)
{
    memset(k, 0, sizeof *k);
}

static void push(uint8_t *q, int qh, int *qn, int key)
{
    if (*qn == 32) return;                      /* full: the oldest stay, the newest is lost */
    q[(qh + *qn) % 32] = (uint8_t)key;
    (*qn)++;
}

/* one pass's reading of a key, through its debounce integrator */
static void sample(kb_state *k, int key, bool now)
{
    if (now && k->count[key] < KB_DEBOUNCE) k->count[key]++;
    else if (!now && k->count[key] > 0) k->count[key]--;
    if (!k->down[key] && k->count[key] == KB_DEBOUNCE) {
        k->down[key] = true;
        push(k->q, k->qh, &k->qn, key);
    } else if (k->down[key] && k->count[key] == 0) {
        k->down[key] = false;
        push(k->uq, k->uqh, &k->uqn, key);
    }
}

void kb_scan(kb_state *k, const kb_io *io)
{
    for (int c = 0; c < KB_COLS; c++) {
        io->drive(io->ctx, c);
        uint8_t rows = io->read(io->ctx);
        for (int r = 0; r < KB_ROWS; r++) {
            int key = r * KB_COLS + c;
            if (!kb_exists(r, c)) continue;
            if (key == KB_ON_KEY && io->read_on) continue;     /* off the matrix (Rev A pass 6) */
            sample(k, key, rows >> r & 1);
        }
    }
    io->drive(io->ctx, -1);
    if (io->read_on) sample(k, KB_ON_KEY, io->read_on(io->ctx));   /* after the columns, each pass */
}

bool kb_idle(const kb_state *k)
{
    for (int key = 0; key < KB_KEYS; key++)
        if (k->down[key] || k->count[key]) return false;
    return true;
}

static int pop(const uint8_t *q, int *qh, int *qn)
{
    int key;
    if (*qn == 0) return -1;
    key = q[*qh];
    *qh = (*qh + 1) % 32;
    (*qn)--;
    return key;
}

int kb_next(kb_state *k)
{
    return pop(k->q, &k->qh, &k->qn);
}

int kb_next_up(kb_state *k)
{
    return pop(k->uq, &k->uqh, &k->uqn);
}
