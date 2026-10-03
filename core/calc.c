/* calc: the RPN stack. STUB: the oracle lands first. */
#include "calc.h"

void ab_init(ab_calc *c)
{
    c->x = c->y = c->z = c->t = c->lastx = abn_zero();
    c->lift = true;
    c->entry.active = false;
    c->entry.len = 0;
}

abn_status ab_do(ab_calc *c, ab_op op) { (void)c; (void)op; return ABN_OK; }
