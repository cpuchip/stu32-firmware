/* const: the physical constants (unit 004); values and sources in const.c. */
#ifndef CONST_H
#define CONST_H

#include "abn.h"

int ab_const_count(void);
int ab_const_index(const char *name);           /* -1 if there is no such constant */
const char *ab_const_name(int i);
abn_status ab_const_value(abn *out, int i);

#endif
