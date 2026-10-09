/* stackkeys (unit 052): the key numbers that drive the stand-in to the deepest stacks found, for scripts/resources.py,
 * run through the app on the host so each sequence is checked as it is made.
 *
 *   stackkeys prog D SHAPE    047's deepest site: in STU, programs A..S each XEQ the next (19 levels), T runs SOLVE X on
 *                             program Z, whose one line is an equation D deep of SHAPE: g groups, x X^( (around X-5),
 *                             n -(, c a chain of D powers, m 18 groups around a chain of D, s SIN(; then XEQ A
 *   stackkeys cas OP TEXT     048's CAS keys: TEXT typed in STU's Equation mode and stored, then CAS's second page,
 *                             OP (s SOLVE, i ∫DX, f FACTR) in X
 *   stackkeys cas d TEXT LO HI   056's ∫AB the same way, in X, then the bounds LO and HI typed, each with ENTER
 *
 * Prints what the host did, then "keys (entry N, ...): k k k...", N the keys up to the stored line.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "keymap.h"

static uint32_t now = 1000;
static int seq[8000], nseq;
static ab_calc c;
static app_state a;

static int face(const char *l)
{
    for (int k = 0; k < KB_KEYS; k++)
        if (km_lookup(k, KM_FACE)->kind != KM_NONE && strcmp(km_lookup(k, KM_FACE)->legend, l) == 0) return k;
    fprintf(stderr, "stackkeys: no key %s\n", l);
    exit(1);
}

static int shift_key(int s)
{
    for (int k = 0; k < KB_KEYS; k++)
        if (km_lookup(k, KM_FACE)->kind == KM_SHIFT && km_lookup(k, KM_FACE)->arg == s) return k;
    fprintf(stderr, "stackkeys: no shift %d\n", s);
    exit(1);
}

static int letter(char ch)
{
    for (int k = 0; k < KB_KEYS; k++)
        if (km_letter_of(k)->kind == KL_LETTER && km_letter_of(k)->value == ch - 'A') return k;
    fprintf(stderr, "stackkeys: no letter %c\n", ch);
    exit(1);
}

static void tap(int k)
{
    if (nseq >= (int)(sizeof seq / sizeof seq[0])) {
        fprintf(stderr, "stackkeys: too many keys\n");
        exit(1);
    }
    now += 100;
    app_key(&a, k, now);
    seq[nseq++] = k;
}

static void gold(const char *l)
{
    tap(shift_key(1));
    tap(face(l));
}

static void blue(const char *l)
{
    tap(shift_key(2));
    tap(face(l));
}

static void rcl(char v)
{
    tap(face("RCL"));
    tap(letter(v));
}

#define MINUS "\xe2\x88\x92"
#define POW "y\xcb\xa3"

/* the equation line, typed in EQN TYPE: its third soft key is ( ) (the pair, the cursor inside), its second steps out */
static void shape(char sh, int d)
{
    switch (sh) {
    case 'g':
        for (int i = 0; i < d; i++) tap(2);
        rcl('X');
        for (int i = 0; i < d; i++) tap(1);
        break;
    case 'x':
        for (int i = 0; i < d; i++) {
            rcl('X');
            tap(face(POW));
            tap(2);
        }
        rcl('X');
        tap(face(MINUS));
        tap(face("5"));
        for (int i = 0; i < d; i++) tap(1);
        break;
    case 'n':
        for (int i = 0; i < d; i++) {
            tap(face(MINUS));
            tap(2);
        }
        rcl('X');
        for (int i = 0; i < d; i++) tap(1);
        break;
    case 'c':
        for (int i = 0; i < d; i++) {
            rcl('X');
            tap(face(POW));
        }
        rcl('X');
        break;
    case 'm':
        for (int i = 0; i < 18; i++) tap(2);
        for (int i = 0; i < d; i++) {
            rcl('X');
            tap(face(POW));
        }
        rcl('X');
        for (int i = 0; i < 18; i++) tap(1);
        break;
    case 's':
        for (int i = 0; i < d; i++) tap(face("SIN"));
        rcl('X');
        for (int i = 0; i < d; i++) tap(1);
        break;
    default:
        fprintf(stderr, "stackkeys: no shape %c\n", sh);
        exit(1);
    }
}

/* TEXT typed in Equation mode: digits . × ÷ + - ^ ( ) = A-Z, SIN( ("(" is EQN LIST's sixth key until typing starts) */
static void type_text(const char *t)
{
    while (*t) {
        if (!strncmp(t, "SIN(", 4)) {
            tap(face("SIN"));
            t += 4;
            continue;
        }
        if (!strncmp(t, "\xc3\x97", 2) || !strncmp(t, "\xc3\xb7", 2)) {
            char op[3] = {t[0], t[1], 0};
            tap(face(op));
            t += 2;
            continue;
        }
        char ch = *t++, d[2] = {ch, 0};
        if (ch >= '0' && ch <= '9') tap(face(d));
        else if (ch == '.') tap(face("."));
        else if (ch == '+') tap(face("+"));
        else if (ch == '-') tap(face(MINUS));
        else if (ch == '^') tap(face(POW));
        else if (ch == '(') tap(c.eqn_typing ? 2 : 5);
        else if (ch == ')') tap(1);
        else if (ch == '=') tap(3);
        else if (ch >= 'A' && ch <= 'Z') rcl(ch);
        else {
            fprintf(stderr, "stackkeys: can't type %c\n", ch);
            exit(1);
        }
    }
}

static void stu(void)
{
    tap(face("C"));
    tap(face("C"));
    tap(shift_key(2));                          /* MODE, STU */
    tap(face("ENTER"));
    tap(2);
    tap(face("C"));
}

static void print_keys(int mark, const char *then)
{
    printf("keys (entry %d, then %s):", mark, then);
    for (int i = 0; i < nseq; i++) printf(" %d", seq[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    bool def = argc == 6 && !strcmp(argv[1], "cas") && !strcmp(argv[2], "d");
    if ((argc != 4 && !def) || (strcmp(argv[1], "prog") && strcmp(argv[1], "cas"))) {
        fprintf(stderr, "usage: stackkeys prog D SHAPE | stackkeys cas OP TEXT | stackkeys cas d TEXT LO HI\n");
        return 2;
    }
    ab_init(&c);
    app_init(&a, &c, now);
    stu();
    if (!strcmp(argv[1], "prog")) {
        int d = atoi(argv[2]);
        gold("9");                              /* PRGM, then CLPGM */
        tap(0);
        gold("9");
        tap(5);
        for (int i = 0; i < 19; i++) {         /* LBL A..S: XEQ the next, RTN */
            gold("XEQ");
            tap(letter('A' + i));
            tap(face("XEQ"));
            tap(letter('A' + i + 1));
            tap(face("ENTER"));
            blue("XEQ");
        }
        gold("XEQ");                            /* LBL T: FN= Z, 1 STO X, 2, SOLVE X, RTN */
        tap(letter('T'));
        gold("9");
        tap(3);
        tap(letter('Z'));
        tap(face("1"));
        tap(face("STO"));
        tap(letter('X'));
        tap(face("2"));
        gold("7");
        tap(letter('X'));
        blue("XEQ");
        gold("XEQ");                            /* LBL Z: the equation line, RTN */
        tap(letter('Z'));
        gold("0");
        shape(argv[3][0], d);
        tap(face("ENTER"));
        for (int k = 0; k < AB_PEQN_MAX; k++)
            if (c.peqn_used[k]) printf("line: %s\n", c.peqn[k]);
        blue("XEQ");
        gold("9");                              /* out of program entry */
        tap(0);
        printf("program lines %d\n", c.prog_len);
        int mark = nseq;
        tap(face("XEQ"));
        tap(letter('A'));
        tap(face("ENTER"));
        print_keys(mark, "XEQ A");
    } else {
        gold("0");                              /* EQN */
        type_text(argv[3]);
        tap(face("ENTER"));
        printf("stored: %s\n", c.eqn_cur >= 0 ? c.eqn[c.eqn_cur] : "-");
        int mark = nseq;
        tap(4);                                 /* CAS, its second page, the key, X */
        tap(5);
        tap(argv[2][0] == 's' ? 0 : argv[2][0] == 'i' ? 1 : argv[2][0] == 'f' ? 2 : 3);
        tap(letter('X'));
        if (def) {                              /* 056: the lower bound, ENTER, the upper, ENTER */
            type_text(argv[4]);
            tap(face("ENTER"));
            type_text(argv[5]);
            tap(face("ENTER"));
            printf("result: %s\n", c.eqn_cur >= 0 ? c.eqn[c.eqn_cur] : "-");
        }
        print_keys(mark, def ? "the bounds' last ENTER" : "the CAS key");
    }
    return 0;
}
