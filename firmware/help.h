/* HELP (unit 075, Michael's stu-keys idea, abacus #8736): blue 9, then any key: that key's legend and what it does
   now, in a page, the key never executed. The text is data, help/help.txt, generated into help_text.c by
   scripts/gen_help.py; tests/help_test.c checks every reachable action has an entry, every entry is reachable,
   every line fits, and the entries that vary by mode do. */
#ifndef HELP_H
#define HELP_H

#include <stdint.h>

#include "keymap.h"

typedef enum { HELP_OP = 1, HELP_MENU, HELP_DEV, HELP_SHIFT, HELP_APP, HELP_CONST, HELP_SOFT, HELP_NONE,
               HELP_UNDEF, HELP_MORE, HELP_GRAPHPAD, HELP_CMPLXPAIR } help_kind;
/* (HELP_MORE: a paged menu's sixth key; HELP_GRAPHPAD: a digit while a graph is open; HELP_CMPLXPAIR: an operator
   while 33s mode's CMPLX prefix waits: the keys app_key takes before their own action, fresh eyes) */

typedef struct {
    uint8_t kind;           /* help_kind */
    int16_t op;             /* HELP_OP: the ab_op (KM_OP, KM_PROMPT, KM_CONFIRM); HELP_DEV: KM_DEV_*; HELP_SHIFT: 1, 2 */
    int16_t arg;            /* HELP_OP: the argument it is for, -1 any */
    const char *text;       /* its lines, '\n' between, each perhaps prefixed with modes (33: 35: STU: 33,35:);
                               HELP_MENU's start with the menu's name and HELP_MENU_END (12 B an entry on the S3) */
} help_entry;

extern const help_entry help_table[];
extern const int help_table_n;
extern const int help_mode_list[];  /* indices into help_table, -1 ended: entries that must vary by mode */

#define HELP_MENU_END '\x1f'       /* ends a menu entry's name, before its lines */
#define HELP_LINE_CELLS 31          /* a page's line: 31 cells at scale 2 (the view's lines, 378 px) */
#define HELP_LINE_BYTES 48

/* The entry for an action: an op's with its argument first, then the op's for any argument; NULL none. */
const help_entry *help_find(int kind, int op, int arg, const char *menu);
const help_entry *help_for_action(const km_action *act);
/* Its lines in mode model (33, 35 or AB_MODEL_STU): those whose prefix names it, if any does, else the plain ones,
   prefixes taken off and {} replaced by legend; up to max into out; the count. */
int help_lines(const help_entry *e, int model, const char *legend, char out[][HELP_LINE_BYTES], int max);

#endif
