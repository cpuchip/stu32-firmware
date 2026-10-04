/* const: the physical constants (unit 004); values and sources in const.c. */
#ifndef CONST_H
#define CONST_H

#include "abn.h"

int ab_const_count(void);
int ab_const_index(const char *name);           /* -1 if there is no such constant */
const char *ab_const_name(int i);              /* the token: ASCII, as the vectors spell it */
const char *ab_const_symbol(int i);            /* the symbol as the guide prints it (UTF-8), for the
                                                  device's CONST menu (abacus #3958); NULL if none */
abn_status ab_const_value(abn *out, int i);

#endif
