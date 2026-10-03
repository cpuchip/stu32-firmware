/* fmt_vectors: runs number-formatting vectors against the core's formatter.
 *
 *   build/fmt_vectors work/002-vectors.txt
 *
 * Line format: ID | source | value | MODE n | options | expected text. The value is read
 * by the engine (abn_from_text); options are w=<width> (default 22), radix=. or ,
 * (default .), sep=on|off (default on). The expected text is compared as an exact string.
 *
 * Exit status: 0 when every vector passes, 1 when any fails, 2 on a malformed file.
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"

#include "fmt.h"

#define LINE_MAX_ 4096

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static int parse_mode(char *s, ab_display *dsp)
{
    ab_fmt *f = &dsp->dec;
    char name[8], kind[4] = "P";
    int n = 0, got = sscanf(s, "%7s %d %3s", name, &n, kind);
    if (got < 1) return -1;
    if (strcmp(name, "BASE") == 0) {
        /* BASE 16|8|2|10: the base display (unit 008) */
        if (got < 2 || (n != 16 && n != 8 && n != 2 && n != 10)) return -1;
        dsp->base = n;
        return 0;
    }
    if (strcmp(name, "FRAC") == 0) {
        /* FRAC c P|F|X: Fraction display, /c, most precise / factors / fixed (unit 007) */
        if (got < 2 || n < 1) return -1;
        dsp->fdisp = true;
        dsp->c = (unsigned)n;
        dsp->frac_format = kind[0] == 'F' ? 1 : (kind[0] == 'X' ? 2 : 0);
        return 0;
    }
    if (strcmp(name, "FIX") == 0) f->mode = AB_FIX;
    else if (strcmp(name, "SCI") == 0) f->mode = AB_SCI;
    else if (strcmp(name, "ENG") == 0) f->mode = AB_ENG;
    else if (strcmp(name, "ALL") == 0) f->mode = AB_ALL;
    else return -1;
    if (f->mode != AB_ALL && got != 2) return -1;
    f->n = n;
    return 0;
}

static int parse_options(char *s, ab_display *dsp)
{
    ab_fmt *f = &dsp->dec;
    for (char *tok = strtok(s, " \t"); tok; tok = strtok(NULL, " \t")) {
        if (strncmp(tok, "dec=", 4) == 0) {
            /* the decimal format a fraction falls back to: dec=FIX4, dec=SCI2, dec=ALL */
            const char *m = tok + 4;
            if (strncmp(m, "FIX", 3) == 0) f->mode = AB_FIX;
            else if (strncmp(m, "SCI", 3) == 0) f->mode = AB_SCI;
            else if (strncmp(m, "ENG", 3) == 0) f->mode = AB_ENG;
            else if (strcmp(m, "ALL") == 0) f->mode = AB_ALL;
            else return -1;
            f->n = f->mode == AB_ALL ? 0 : atoi(m + 3);
            continue;
        }
        if (strncmp(tok, "polar=", 6) == 0) {
            /* complex numbers as r theta a in the given angle unit (unit 009) */
            dsp->polar = true;
            dsp->angle = strcmp(tok + 6, "rad") == 0 ? 1 : strcmp(tok + 6, "grad") == 0 ? 2 : 0;
            continue;
        }
        if (strncmp(tok, "w=", 2) == 0) f->width = atoi(tok + 2);
        else if (strcmp(tok, "radix=.") == 0) f->radix = '.';
        else if (strcmp(tok, "radix=,") == 0) f->radix = ',';
        else if (strcmp(tok, "sep=on") == 0) f->separator = true;
        else if (strcmp(tok, "sep=off") == 0) f->separator = false;
        else return -1;
    }
    return 0;
}

/* Runs one format-vectors file from fp (the caller opens and closes it). Returns 0 when all
 * pass, 1 on a failure, 2 on a malformed file. */
int fmt_vectors_run(const char *name, FILE *fp)
{
    char line[LINE_MAX_];
    int lineno = 0, total = 0, passed = 0;
    while (fgets(line, sizeof line, fp)) {
        lineno++;
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        char *field[6];
        int nf = 0;
        for (char *p = s; nf < 6; nf++) {
            field[nf] = p;
            char *bar = strchr(p, '|');
            if (!bar) { nf++; break; }
            *bar = '\0';
            p = bar + 1;
        }
        if (nf != 6) {
            fprintf(stderr, "%s:%d: expected 6 fields\n", name, lineno);
            return 2;
        }
        char *id = trim(field[0]), *value = trim(field[2]), *want = trim(field[5]);
        ab_display dsp = {{AB_ALL, 0, 22, '.', true}, false, 4095, 0, 10, false, 0};
        abn v, vim;
        bool cpx = false;
        /* a value "[a,b,c]" is a vector (unit 010) */
        abn vel[3];
        int vn = 0;
        if (value[0] == '[') {
            size_t vl = strlen(value);
            if (vl < 3 || value[vl - 1] != ']') {
                fprintf(stderr, "%s:%d: %s: bad vector value\n", name, lineno, id);
                return 2;
            }
            value[vl - 1] = '\0';
            for (char *q = strtok(value + 1, ","); q; q = strtok(NULL, ",")) {
                if (vn == 3 || abn_from_text(&vel[vn++], q) != ABN_OK) {
                    fprintf(stderr, "%s:%d: %s: bad vector value\n", name, lineno, id);
                    return 2;
                }
            }
            value = (char *)"0";
        }
        /* a value "a i b" is complex (unit 009) */
        char *ip = strchr(value, 'i');
        if (ip) {
            *ip = '\0';
            cpx = true;
            if (abn_from_text(&vim, ip + 1) != ABN_OK) {
                fprintf(stderr, "%s:%d: %s: bad complex value\n", name, lineno, id);
                return 2;
            }
        }
        if (abn_from_text(&v, value) != ABN_OK || parse_mode(trim(field[3]), &dsp) != 0 ||
            parse_options(trim(field[4]), &dsp) != 0) {
            fprintf(stderr, "%s:%d: %s: malformed vector\n", name, lineno, id);
            return 2;
        }
        /* an expected text may end in " ^" (above) or " v" (below): the accuracy indicator */
        int want_ind = 0;
        size_t wl = strlen(want);
        if (wl > 2 && want[wl - 2] == ' ' && (want[wl - 1] == '^' || want[wl - 1] == 'v')) {
            want_ind = want[wl - 1] == '^' ? 1 : -1;
            want[wl - 2] = '\0';
        }
        char got[512];
        int ind = 0;
        int len = vn ? ab_format_vector(vel, vn, &dsp, got, sizeof got)
                : cpx ? ab_format_complex(&v, &vim, &dsp, got, sizeof got)
                      : ab_format_display(&v, &dsp, got, sizeof got, &ind);
        total++;
        /* a base pattern is returned whole (unit 008 rule 6); every other text fits the width */
        bool fits = dsp.base != 10 || (cpx || vn ? len : (int)strlen(got)) <= dsp.dec.width;
        if (len >= 0 && strcmp(got, want) == 0 && fits && ind == want_ind) {
            passed++;
        } else {
            printf("FAIL %s: expected \"%s\"%s, got \"%s\"%s (returned %d)\n", id, want,
                   want_ind > 0 ? " ^" : want_ind < 0 ? " v" : "", got, ind > 0 ? " ^" : ind < 0 ? " v" : "", len);
        }
    }
    printf("%s: %d/%d vectors\n", name, passed, total);
    if (getenv("AB_ARENA_PEAK")) fprintf(stderr, "arena peak %d\n", ar_peak());   /* unit 010b measurement */
    return passed == total ? 0 : 1;
}

#ifndef AB_DEVICE
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s VECTORS_FILE\n", argv[0]);
        return 2;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror(argv[1]);
        return 2;
    }
    int r = fmt_vectors_run(argv[1], fp);
    fclose(fp);
    return r;
}
#endif
