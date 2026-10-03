/* msg: unit 018 red stub (the messages arrive with the green commit). */
#include <stddef.h>

#include "calc.h"

const char *ab_status_message(abn_status st)
{
    (void)st;
    return "";
}

const char *ab_error_message(const ab_calc *c)
{
    (void)c;
    return NULL;
}

const char *ab_busy_message(ab_op op)
{
    (void)op;
    return NULL;
}
