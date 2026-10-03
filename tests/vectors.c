/* vectors: runs a work unit's acceptance vectors against the core.
 *
 *   build/vectors work/001-vectors.txt
 *
 * Line format: ID | source | keys | expectations. Every vector starts from ab_init().
 * Numbers in expectations are compared by this file's own normalizer (sign, significant
 * digits, exponent of the leading digit), not by the engine's parser, so a parser bug
 * cannot hide itself. "=" is exact equality; "~" compares after rounding both sides to
 * 12 significant digits (half away from zero, on the digit strings); "^" means within 8
 * units in the 34th significant digit of the value (unit 003 amendment 1). The expectation
 * ERR means at least one operation in the vector must return an error; without it, any
 * error fails the vector.
 *
 * Exit status: 0 when every expectation passes, 1 when any fails, 2 on a malformed file.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "norm.h"

#define LINE_MAX_ 2048
static const struct { const char *name; ab_op op; } NAMED[] = {
    {"E", AB_KE}, {"CHS", AB_KCHS}, {"BKSP", AB_KBKSP}, {"ENTER", AB_ENTER},
    {"RDN", AB_RDN}, {"RUP", AB_RUP}, {"XY", AB_XY}, {"LASTX", AB_LASTX},
    {"CLX", AB_CLX}, {"CLSTK", AB_CLSTK},
    {"+", AB_ADD}, {"-", AB_SUB}, {"*", AB_MUL}, {"/", AB_DIV},
    {"EXP", AB_EXP}, {"LN", AB_LN}, {"TENX", AB_TENX}, {"LOG", AB_LOG},
    {"IDIV", AB_IDIV}, {"RMDR", AB_RMDR},
    {"SQ", AB_SQ}, {"SQRT", AB_SQRT}, {"INV", AB_INV}, {"POW", AB_POW}, {"XROOT", AB_XROOT},
    {"PI", AB_PI}, {"DEG", AB_DEG}, {"RAD", AB_RAD}, {"GRAD", AB_GRAD},
    {"SIN", AB_SIN}, {"COS", AB_COS}, {"TAN", AB_TAN},
    {"ASIN", AB_ASIN}, {"ACOS", AB_ACOS}, {"ATAN", AB_ATAN},
    {"SINH", AB_SINH}, {"COSH", AB_COSH}, {"TANH", AB_TANH},
    {"ASINH", AB_ASINH}, {"ACOSH", AB_ACOSH}, {"ATANH", AB_ATANH},
    {"PCT", AB_PCT}, {"PCTCHG", AB_PCTCHG},
};

/* Applies one key token. Returns 0, or -1 for an unknown token. */
static int apply_token(ab_calc *c, const char *tok, int *errors)
{
    for (size_t i = 0; i < sizeof NAMED / sizeof NAMED[0]; i++) {
        if (strcmp(tok, NAMED[i].name) == 0) {
            if (ab_do(c, NAMED[i].op) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    for (const char *p = tok; *p; p++)
        if (!isdigit((unsigned char)*p) && *p != '.') return -1;
    for (const char *p = tok; *p; p++) {
        ab_op op = (*p == '.') ? AB_KPOINT : (ab_op)(AB_K0 + (*p - '0'));
        if (ab_do(c, op) != ABN_OK) (*errors)++;
    }
    return 0;
}

static const abn *reg(const ab_calc *c, char r)
{
    switch (r) {
    case 'X': return &c->x;
    case 'Y': return &c->y;
    case 'Z': return &c->z;
    case 'T': return &c->t;
    case 'L': return &c->lastx;
    default: return NULL;
    }
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s VECTORS_FILE\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    char line[LINE_MAX_];
    int lineno = 0, vectors = 0, vectors_ok = 0, checks = 0, checks_ok = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        char *field[4];
        int nf = 0;
        for (char *p = s; nf < 4; nf++) {
            field[nf] = p;
            char *bar = strchr(p, '|');
            if (!bar) { nf++; break; }
            *bar = '\0';
            p = bar + 1;
        }
        if (nf != 4) {
            fprintf(stderr, "%s:%d: expected 4 fields\n", argv[1], lineno);
            return 2;
        }
        char *id = trim(field[0]), *keys = trim(field[2]), *expect = trim(field[3]);

        ab_calc c;
        ab_init(&c);
        int op_errors = 0;
        for (char *tok = strtok(keys, " \t"); tok; tok = strtok(NULL, " \t")) {
            if (apply_token(&c, tok, &op_errors) != 0) {
                fprintf(stderr, "%s:%d: %s: unknown key token '%s'\n", argv[1], lineno, id, tok);
                return 2;
            }
        }

        int want_error = 0;
        char *exps[64];
        int nexp = 0;
        for (char *ex = strtok(expect, " \t"); ex; ex = strtok(NULL, " \t")) {
            if (strcmp(ex, "ERR") == 0) want_error = 1;
            else if (nexp < 64) exps[nexp++] = ex;
        }
        int vec_ok = want_error ? (op_errors > 0) : (op_errors == 0);
        if (!vec_ok) {
            if (want_error) printf("FAIL %s: expected an error, none returned\n", id);
            else printf("FAIL %s: %d operation(s) returned an error\n", id, op_errors);
        }
        for (int k = 0; k < nexp; k++) {
            char *ex = exps[k];
            const abn *r = reg(&c, ex[0]);
            if (!r || (ex[1] != '=' && ex[1] != '~' && ex[1] != '^')) {
                fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", argv[1], lineno, id, ex);
                return 2;
            }
            char got_text[ABN_TEXT_MAX];
            abn_to_text(r, got_text, sizeof got_text);
            norm want, got;
            if (normalize(ex + 2, &want) != 0) {
                fprintf(stderr, "%s:%d: %s: bad number '%s'\n", argv[1], lineno, id, ex + 2);
                return 2;
            }
            int ok = normalize(got_text, &got) == 0;
            if (ok && ex[1] == '~') {
                round_norm(&want, 12);
                round_norm(&got, 12);
            }
            if (ex[1] == '^') ok = ok && norm_within_units(&want, &got, 8);
            else ok = ok && norm_equal(&want, &got);
            checks++;
            if (ok) {
                checks_ok++;
            } else {
                vec_ok = 0;
                printf("FAIL %s: %c expected %s%s, got %s\n", id, ex[0],
                       ex[1] == '=' ? "" : (ex[1] == '~' ? "~" : "^"), ex + 2, got_text);
            }
        }
        vectors++;
        if (vec_ok) vectors_ok++;
    }
    fclose(f);
    printf("%s: %d/%d vectors, %d/%d expectations\n", argv[1], vectors_ok, vectors,
           checks_ok, checks);
    return (vectors_ok == vectors && checks_ok == checks) ? 0 : 1;
}
