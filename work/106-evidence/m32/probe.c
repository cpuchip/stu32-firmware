/* unit 106's 32-bit leg: ab_val against rows.c's val_without_row (its first lines, copied), per data model */
#include <stdio.h>
#include <stddef.h>
#include "calc.h"
typedef struct {
    ab_kind kind;
    abn re, im, p3;
    int len;
} val_without_row;
int main(void)
{
    printf("ab_val %zu, val_without_row %zu, abn %zu (align %zu), offsetof(ab_val, re) %zu, long long align %zu, double align %zu\n",
           sizeof(ab_val), sizeof(val_without_row), sizeof(abn), _Alignof(abn), offsetof(ab_val, re), _Alignof(long long),
           _Alignof(double));
    return 0;
}
