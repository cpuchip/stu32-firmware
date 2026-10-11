/* stackkeys (unit 052): the key numbers that drive the stand-in to the deepest stacks found, for scripts/resources.py,
 * run through the app on the host so each sequence is checked as it is made.
 *
 *   stackkeys prog D SHAPE    047's deepest site: in STU, programs A..S each XEQ the next (19 levels), T runs SOLVE X on
 *                             program Z, whose one line is an equation D deep of SHAPE: g groups, x X^( (around X-5),
 *                             n -(, c a chain of D powers, m 18 groups around a chain of D, s SIN(; then XEQ A
 *   stackkeys cas OP TEXT     048's CAS keys: TEXT typed in STU's Equation mode and stored, then CAS's second page,
 *                             OP (s SOLVE, i ∫DX, f FACTR) in X
 *   stackkeys cas d TEXT LO HI   056's ∫AB the same way, in X, then the bounds LO and HI typed, each with ENTER
 *   stackkeys cas q TEXT G       057's QUOT on CAS's third page, in X, then G typed with ENTER (g GCD the same;
 *   stackkeys cas c TEXT         c CANCL, with no text)
 *
 *   stackkeys full OUT        the "full" saved state for resources.py's second stack state (abacus #6586, #6596): from
 *                             MEMORY CLEAR, through the app's keys, the most equations the list takes, each at its
 *                             longest, every letter A-Z, the statistics registers, the stack and LASTx, in RAD; saved
 *                             with dev_image_save in store.c's slot 0 of the abstate partition (OUT, 512 KB, the
 *                             second slot erased), for esptool at 0x710000 on Rev A, 0x510000 on the stand-in.
 *                             Checked here: the list refuses one more, and the image loads back through the store and
 *                             saves again the same. Programs are not filled: the stack readings' own keys clear
 *                             program memory (CLPGM) first.
 *
 * Prints what the host did, then "keys (entry N, ...): k k k...", N the keys up to the stored line.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "keymap.h"
#include "settings.h"
#include "state.h"
#include "store.h"

static uint32_t now = 1000;
static int seq[8000], nseq;
static ab_calc c;
static app_state a;
static bool record = true;     /* full's keys are not printed: too many to keep */

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
    if (record && nseq >= (int)(sizeof seq / sizeof seq[0])) {
        fprintf(stderr, "stackkeys: too many keys\n");
        exit(1);
    }
    now += 100;
    app_key(&a, k, now);
    if (record) seq[nseq++] = k;
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

static void digits(const char *s)
{
    for (; *s; s++) {
        char d[2] = {*s, 0};
        tap(face(*s == '.' ? "." : d));
    }
}

/* the full state's image in the abstate partition: 512 KB on both boards, two 256 KB slots (the partition's size / 2),
   the image in slot 0 as ab_store_save writes it into an erased partition */
#define PART_SIZE 0x80000u
#define SLOT_SIZE (PART_SIZE / 2)
static uint8_t part[PART_SIZE];

static int f_erase(void *x, uint32_t off, uint32_t len) { (void)x; memset(part + off, 0xFF, len); return 0; }
static int f_write(void *x, uint32_t off, const void *src, uint32_t len)
{
    (void)x;
    memcpy(part + off, src, len);
    return 0;
}
static int f_read(void *x, uint32_t off, void *dst, uint32_t len) { (void)x; memcpy(dst, part + off, len); return 0; }

static int full(const char *out)
{
    static uint8_t img[SLOT_SIZE], back[SLOT_SIZE];
    static ab_calc c2;
    record = false;
    stu();
    blue("2");                                  /* the angle menu, RAD */
    tap(1);
    tap(face("C"));
    char num[16];
    for (int v = 0; v < 26; v++) {              /* each letter k/7 (34 digits), k = 1..26 */
        snprintf(num, sizeof num, "%d", v + 1);
        digits(num);
        tap(face("ENTER"));
        digits("7");
        tap(face("\xc3\xb7"));
        tap(face("STO"));
        tap(letter('A' + v));
    }
    for (int i = 1; i <= 40; i++) {             /* 40 points: y = i/3, x = i */
        snprintf(num, sizeof num, "%d", i);
        digits(num);
        tap(face("ENTER"));
        digits("3");
        tap(face("\xc3\xb7"));
        digits(num);
        tap(face("\xce\xa3+"));
    }
    digits("1.5");                              /* the stack: T Z Y X, and LASTx from the last operation */
    tap(face("ENTER"));
    digits("2.5");
    tap(face("ENTER"));
    digits("3.5");
    tap(face("ENTER"));
    digits("11");
    tap(face("ENTER"));
    digits("4");
    tap(face("\xc3\xb7"));
    gold("0");                                  /* EQN: each equation the longest sum of letters the list stores */
    int before = -1, longest = 0;
    char text[AB_EQN_LEN * 2];
    while (c.eqn_count != before) {
        before = c.eqn_count;
        int n = 0;
        for (int terms = 1;; terms++) {         /* A+B+...: the most terms whose text fits */
            int len = 0;
            for (int k = 0; k < terms; k++) len += 1 + (k > 0);
            if (len >= AB_EQN_LEN) break;
            n = terms;
        }
        text[0] = 0;
        for (int k = 0; k < n; k++) {
            char t[3] = {k ? '+' : 0, (char)('A' + (k + c.eqn_count) % 26), 0};
            strcat(text, k ? t : t + 1);
        }
        type_text(text);
        tap(face("ENTER"));
        if (c.eqn_count != before && c.eqn_cur >= 0 && (int)strlen(c.eqn[c.eqn_cur]) > longest)
            longest = (int)strlen(c.eqn[c.eqn_cur]);
        if (c.eqn_typing) {                     /* refused and still typed: C leaves it */
            tap(face("C"));
            tap(face("C"));
        }
    }
    tap(face("C"));
    int sto = 0;
    for (int v = 0; v < 26; v++) sto += !abn_is_zero(&c.var[AB_VAR_A + v].re);
    printf("full: %d equations (a 67th refused: the count stayed)%s, the longest %d bytes; letters set %d of 26; "
           "angle %s; statistics n %s\n", c.eqn_count, "", longest, sto,
           c.angle == AB_ANGLE_RAD ? "RAD" : "not RAD", abn_is_zero(&c.stat[0]) ? "0" : "set");
    size_t n = dev_image_save(&c, &a.set, img, sizeof img);
    if (!n || n > SLOT_SIZE - AB_STORE_HEADER) {
        fprintf(stderr, "stackkeys: the full image did not save (%zu bytes)\n", n);
        return 1;
    }
    memset(part, 0xFF, sizeof part);
    ab_flash fl = {NULL, 4096, SLOT_SIZE, f_erase, f_write, f_read};
    if (ab_store_save(&fl, img, (uint32_t)n) != 0) {
        fprintf(stderr, "stackkeys: the store refused the image\n");
        return 1;
    }
    uint32_t got = ab_store_load(&fl, back, sizeof back);   /* the board's load path, from the partition */
    dev_settings s2;
    ab_init(&c2);
    if (got != n || memcmp(back, img, n) || dev_image_load(&c2, &s2, back, got) != ABN_OK) {
        fprintf(stderr, "stackkeys: the full image does not load back\n");
        return 1;
    }
    size_t n2 = dev_image_save(&c2, &s2, back, sizeof back);
    if (n2 != n || memcmp(back, img, n)) {
        fprintf(stderr, "stackkeys: the full image loaded and saved again differs\n");
        return 1;
    }
    FILE *f = fopen(out, "wb");
    if (!f || fwrite(part, 1, sizeof part, f) != sizeof part || fclose(f)) {
        fprintf(stderr, "stackkeys: can't write %s\n", out);
        return 1;
    }
    printf("full: image %zu bytes in slot 0, loads back and saves again the same; %s, %u bytes, for abstate "
           "(Rev A 0x710000, the stand-in 0x510000)\n", n, out, PART_SIZE);
    return 0;
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
    bool poly = argc == 5 && !strcmp(argv[1], "cas") && (!strcmp(argv[2], "q") || !strcmp(argv[2], "g"));
    if (argc == 3 && !strcmp(argv[1], "full")) {
        ab_init(&c);
        app_init(&a, &c, now);
        return full(argv[2]);
    }
    bool cas3 = argc > 2 && !strcmp(argv[1], "cas") && strchr("qgc", argv[2][0]);
    if ((argc != 4 && !def && !poly) || (strcmp(argv[1], "prog") && strcmp(argv[1], "cas"))) {
        fprintf(stderr, "usage: stackkeys prog D SHAPE | stackkeys cas OP TEXT | stackkeys cas d TEXT LO HI | "
                        "stackkeys full OUT\n");
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
        if (cas3) {                             /* 057: on to the third page */
            tap(5);
            tap(argv[2][0] == 'q' ? 0 : argv[2][0] == 'g' ? 1 : 2);
        } else
            tap(argv[2][0] == 's' ? 0 : argv[2][0] == 'i' ? 1 : argv[2][0] == 'f' ? 2 : 3);
        tap(letter('X'));
        if (poly) {                             /* 057: g, ENTER */
            type_text(argv[4]);
            tap(face("ENTER"));
        }
        if (cas3) printf("result: %s\n", c.eqn_cur >= 0 ? c.eqn[c.eqn_cur] : "-");
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
