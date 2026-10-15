/* arena: the wide exact numbers (core/xnum.h, core/bigint.h, 3 KB each) that a computation needs
 * for its duration, taken from one fixed pool instead of each function keeping its own statics
 * (unit 010b). The core is single-threaded and its exact paths nest a few calls deep, so the pool
 * only needs to hold the most numbers ever live at once (measured: see AB_ARENA_WIDE).
 *
 * Use: a function marks the pool, takes every number it needs in one call, and releases the mark
 * on every return (the core wraps such functions so that one place releases). A take that would
 * overrun the pool takes nothing and returns false; the caller reports ABN_RANGE and changes
 * nothing. Numbers are not cleared when taken. */
#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>

#include "xnum.h"

/* The pool's size in wide numbers (about 3 KB each: 73.5 KB). Measured peak live over every vector
   file and every fuzz and accuracy suite (AB_ARENA_PEAK=1 makes the runners report it): 18, the
   correlation coefficient (6 registers, 5 working values, 6 for the exact ratio's square root, 1
   inside long division), which is also the most a reading of the call tree allows; 24 leaves 6
   spare for later units. Below 18 the statistics fail (a mutation run confirmed). */
#define AB_ARENA_WIDE 24

int ar_mark(void);
void ar_release(int mark);
/* Takes n xnums (n pointer-to-pointer arguments). */
bool ar_xn(int n, ...);
/* Takes n bigints (n pointer-to-pointer arguments). */
bool ar_bi(int n, ...);
/* The most numbers live at once since start (for the measurement). */
int ar_peak(void);

/* CAS 004: Casimir's working memory, beside the pool (so placed with it: PSRAM on the devices); only the calc task's
   CAS step uses it, and nothing in it outlives the step. 8,192 B (the unit's budget) until the a4dd723 repin, then
   8,800 B (Michael's arena card, cas-arena-8800): in RAD X×ATAN(X) needs 8,592 B and SIN(X)+1÷(X^2-1) 8,760 (048's I17,
   I18); in DEG and GRAD X×ATAN(X) needs all 8,800 (048's I23). */
#define AB_CAS_ARENA 8800
#ifdef AB_CASIM
extern unsigned char ab_cas_arena[AB_CAS_ARENA];
#endif

#endif
