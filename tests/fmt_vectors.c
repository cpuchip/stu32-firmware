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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmt.h"

#define LINE_MAX_ 4096

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static int parse_mode(char *s, ab_fmt *f)
{
    char name[8];
    int n = 0, got = sscanf(s, "%7s %d", name, &n);
    if (got < 1) return -1;
    if (strcmp(name, "FIX") == 0) f->mode = AB_FIX;
    else if (strcmp(name, "SCI") == 0) f->mode = AB_SCI;
    else if (strcmp(name, "ENG") == 0) f->mode = AB_ENG;
    else if (strcmp(name, "ALL") == 0) f->mode = AB_ALL;
    else return -1;
    if (f->mode != AB_ALL && got != 2) return -1;
    f->n = n;
    return 0;
}

static int parse_options(char *s, ab_fmt *f)
{
    for (char *tok = strtok(s, " \t"); tok; tok = strtok(NULL, " \t")) {
        if (strncmp(tok, "w=", 2) == 0) f->width = atoi(tok + 2);
        else if (strcmp(tok, "radix=.") == 0) f->radix = '.';
        else if (strcmp(tok, "radix=,") == 0) f->radix = ',';
        else if (strcmp(tok, "sep=on") == 0) f->separator = true;
        else if (strcmp(tok, "sep=off") == 0) f->separator = false;
        else return -1;
    }
    return 0;
}

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
            fprintf(stderr, "%s:%d: expected 6 fields\n", argv[1], lineno);
            return 2;
        }
        char *id = trim(field[0]), *value = trim(field[2]), *want = trim(field[5]);
        ab_fmt f = {AB_ALL, 0, 22, '.', true};
        abn v;
        if (abn_from_text(&v, value) != ABN_OK || parse_mode(trim(field[3]), &f) != 0 ||
            parse_options(trim(field[4]), &f) != 0) {
            fprintf(stderr, "%s:%d: %s: malformed vector\n", argv[1], lineno, id);
            return 2;
        }
        char got[512];
        int len = ab_format(&v, &f, got, sizeof got);
        total++;
        if (len >= 0 && strcmp(got, want) == 0 && (int)strlen(got) <= f.width) {
            passed++;
        } else {
            printf("FAIL %s: expected \"%s\", got \"%s\" (returned %d)\n", id, want, got, len);
        }
    }
    fclose(fp);
    printf("%s: %d/%d vectors\n", argv[1], passed, total);
    return passed == total ? 0 : 1;
}
