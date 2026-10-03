/* keymap_test: the generated keymap against a reading of the layout made by hand (layout/stu32-v0.json,
 * row by row), and against the core: every op a key names exists, every key that asks for an
 * argument gets one of the kind its op takes, every legend can be drawn in our face. */
#include <stdio.h>
#include <string.h>

#include "calc.h"
#include "keymap.h"
#include "screen.h"

static int bad, checks;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok && bad++ < 40) printf("keymap_test: %s\n", what);
}

typedef struct {
    int row, col, shift;
    km_kind kind;
    int op, arg, prompt;
    const char *legend;
} want;

#define OPK(r, c, s, op, l) {r, c, s, KM_OP, op, 0, 0, l}
#define VARK(r, c, s, op, l) {r, c, s, KM_PROMPT, op, 0, KM_ARG_VAR, l}
#define MENU(r, c, s, l) {r, c, s, KM_MENU, 0, -1, 0, l}  /* a menu not defined yet */

static const want W[] = {
    /* row 1 */
    OPK(1, 0, KM_FACE, AB_SQRT, "\xe2\x88\x9ax"), OPK(1, 0, KM_GOLD, AB_SQ, "x\xc2\xb2"), OPK(1, 0, KM_BLUE, AB_ABS, "ABS"),
    OPK(1, 1, KM_FACE, AB_EXP, "e\xcb\xa3"), OPK(1, 1, KM_GOLD, AB_TENX, "10\xcb\xa3"), MENU(1, 1, KM_BLUE, "EXP"),
    OPK(1, 2, KM_FACE, AB_LN, "LN"), OPK(1, 2, KM_GOLD, AB_LOG, "LOG"), MENU(1, 2, KM_BLUE, "HYP"),
    OPK(1, 3, KM_FACE, AB_POW, "y\xcb\xa3"), OPK(1, 3, KM_GOLD, AB_XROOT, "\xcb\xa3\xe2\x88\x9ay"), MENU(1, 3, KM_BLUE, "POW"),
    OPK(1, 4, KM_FACE, AB_INV, "1/x"), OPK(1, 4, KM_GOLD, AB_PCT, "%"), OPK(1, 4, KM_BLUE, AB_PCTCHG, "%CHG"),
    OPK(1, 5, KM_FACE, AB_SIGMAPLUS, "\xce\xa3+"), OPK(1, 5, KM_GOLD, AB_SIGMAMINUS, "\xce\xa3\xe2\x88\x92"),
    MENU(1, 5, KM_BLUE, "STAT"),
    /* row 2 */
    VARK(2, 0, KM_FACE, AB_STO, "STO"), VARK(2, 0, KM_GOLD, AB_VIEW, "VIEW"), MENU(2, 0, KM_BLUE, "VARS"),
    VARK(2, 1, KM_FACE, AB_RCL, "RCL"), MENU(2, 1, KM_GOLD, "CONST"), MENU(2, 1, KM_BLUE, "CONV"),
    OPK(2, 2, KM_FACE, AB_RDN, "R\xe2\x86\x93"), OPK(2, 2, KM_GOLD, AB_RUP, "R\xe2\x86\x91"), MENU(2, 2, KM_BLUE, "STACK"),
    OPK(2, 3, KM_FACE, AB_SIN, "SIN"), OPK(2, 3, KM_GOLD, AB_ASIN, "ASIN"), MENU(2, 3, KM_BLUE, "ANGLE"),
    OPK(2, 4, KM_FACE, AB_COS, "COS"), OPK(2, 4, KM_GOLD, AB_ACOS, "ACOS"), OPK(2, 4, KM_BLUE, AB_TORAD, "\xe2\x86\x92RAD"),
    OPK(2, 5, KM_FACE, AB_TAN, "TAN"), OPK(2, 5, KM_GOLD, AB_ATAN, "ATAN"), OPK(2, 5, KM_BLUE, AB_TODEG, "\xe2\x86\x92" "DEG"),
    /* row 3 (ENTER is two units wide: five keys) */
    OPK(3, 0, KM_FACE, AB_ENTER, "ENTER"), OPK(3, 0, KM_GOLD, AB_LASTX, "LASTx"), MENU(3, 0, KM_BLUE, "MODE"),
    OPK(3, 1, KM_FACE, AB_XY, "x\xe2\x86\x94y"), MENU(3, 1, KM_GOLD, "TEST"), MENU(3, 1, KM_BLUE, "BASE"),
    OPK(3, 2, KM_FACE, AB_KCHS, "+/\xe2\x88\x92"), OPK(3, 2, KM_GOLD, AB_ABS, "ABS"), MENU(3, 2, KM_BLUE, "CMPLX"),
    OPK(3, 3, KM_FACE, AB_KE, "E"), OPK(3, 3, KM_GOLD, AB_PI, "\xcf\x80"), MENU(3, 3, KM_BLUE, "FRAC"),
    OPK(3, 4, KM_FACE, AB_KBKSP, "\xe2\x86\x90"), MENU(3, 4, KM_GOLD, "CLEAR"),
    {3, 4, KM_BLUE, KM_DEVICE, 0, KM_DEV_UNDO, 0, "UNDO"},
    /* row 4 */
    {4, 0, KM_FACE, KM_PROMPT, AB_XEQ, 0, KM_ARG_TARGET, "XEQ"}, {4, 0, KM_GOLD, KM_PROMPT, AB_LBL, 0, KM_ARG_LABEL, "LBL"},
    OPK(4, 0, KM_BLUE, AB_RTN, "RTN"),
    OPK(4, 1, KM_FACE, AB_K7, "7"), MENU(4, 1, KM_GOLD, "SOLVE"), MENU(4, 1, KM_BLUE, "GRAPH"),
    OPK(4, 2, KM_FACE, AB_K8, "8"), MENU(4, 2, KM_GOLD, "\xe2\x88\xab"), MENU(4, 2, KM_BLUE, "TABLE"),
    OPK(4, 3, KM_FACE, AB_K9, "9"), {4, 3, KM_GOLD, KM_MENU, 0, 0, 0, "PRGM"} /* menu 0 (#3877) */, MENU(4, 3, KM_BLUE, "TUTOR"),
    OPK(4, 4, KM_FACE, AB_DIV, "\xc3\xb7"), OPK(4, 4, KM_GOLD, AB_IDIV, "INT\xc3\xb7"), OPK(4, 4, KM_BLUE, AB_RMDR, "Rmdr"),
    /* row 5 */
    {5, 0, KM_FACE, KM_SHIFT, 0, 1, 0, ""},
    OPK(5, 1, KM_FACE, AB_K4, "4"), {5, 1, KM_GOLD, KM_PROMPT, AB_GTO, 0, KM_ARG_TARGET, "GTO"}, VARK(5, 1, KM_BLUE, AB_ISG, "ISG"),
    OPK(5, 2, KM_FACE, AB_K5, "5"), VARK(5, 2, KM_GOLD, AB_DSE, "DSE"), MENU(5, 2, KM_BLUE, "x?0"),
    OPK(5, 3, KM_FACE, AB_K6, "6"), MENU(5, 3, KM_GOLD, "x?y"), MENU(5, 3, KM_BLUE, "FLAGS"),
    OPK(5, 4, KM_FACE, AB_MUL, "\xc3\x97"), OPK(5, 4, KM_GOLD, AB_FACT, "n!"), MENU(5, 4, KM_BLUE, "PROB"),
    /* row 6 */
    {6, 0, KM_FACE, KM_SHIFT, 0, 2, 0, ""},
    OPK(6, 1, KM_FACE, AB_K1, "1"), MENU(6, 1, KM_GOLD, "UNITS"), MENU(6, 1, KM_BLUE, "MEM"),
    OPK(6, 2, KM_FACE, AB_K2, "2"), MENU(6, 2, KM_GOLD, "DISP"), MENU(6, 2, KM_BLUE, "\xe2\x88\xa1MODE"),
    OPK(6, 3, KM_FACE, AB_K3, "3"), MENU(6, 3, KM_GOLD, "RADIX"), MENU(6, 3, KM_BLUE, "SETUP"),
    OPK(6, 4, KM_FACE, AB_SUB, "\xe2\x88\x92"), OPK(6, 4, KM_GOLD, AB_RAND, "RAND"), OPK(6, 4, KM_BLUE, AB_SEED, "SEED"),
    /* row 7 */
    OPK(7, 0, KM_FACE, AB_CANCEL, "C"), {7, 0, KM_GOLD, KM_DEVICE, 0, KM_DEV_OFF, 0, "OFF"}, MENU(7, 0, KM_BLUE, "SYS"),
    OPK(7, 1, KM_FACE, AB_K0, "0"), OPK(7, 1, KM_GOLD, AB_EQN, "EQN") /* the provisional toggle (#3867) */, MENU(7, 1, KM_BLUE, "APPS"),
    OPK(7, 2, KM_FACE, AB_KPOINT, "."), OPK(7, 2, KM_GOLD, AB_KCOMMA, ","),
    OPK(7, 2, KM_BLUE, AB_FDISP, "\xe2\x86\x92" "FRAC"),    /* the Fraction-display toggle (ch.5; abacus #3822) */
    OPK(7, 3, KM_FACE, AB_RS, "R/S"), OPK(7, 3, KM_GOLD, AB_PSE, "PSE"), OPK(7, 3, KM_BLUE, AB_SST, "SST"),
    OPK(7, 4, KM_FACE, AB_ADD, "+"), MENU(7, 4, KM_GOLD, "SUMS"), MENU(7, 4, KM_BLUE, "L.R."),
};

int main(void)
{
    static ab_calc c;
    char m[200];
    int defined = 0, undefined = 0;

    /* the hand reading, entry by entry */
    for (size_t i = 0; i < sizeof W / sizeof W[0]; i++) {
        const want *w = &W[i];
        const km_action *a = km_lookup(w->row * KB_COLS + w->col, w->shift);
        snprintf(m, sizeof m, "key %d,%d shift %d: kind %d op %d arg %d prompt %d '%.20s'", w->row, w->col, w->shift,
                 a->kind, a->op, a->arg, a->prompt, a->legend ? a->legend : "(null)");
        expect(a->kind == w->kind && a->op == w->op && a->arg == w->arg && a->prompt == w->prompt && a->legend &&
                   strcmp(a->legend, w->legend) == 0,
               m);
    }
    /* the soft row, the C key as ON, keys that are not there */
    for (int k = 0; k < 6; k++) {
        const km_action *a = km_lookup(k, KM_FACE);
        expect(a->kind == KM_SOFT && a->arg == k, "the soft row: soft keys 0..5");
        expect(km_lookup(k, KM_GOLD)->kind == KM_NONE && km_lookup(k, KM_BLUE)->kind == KM_NONE, "soft keys: no shifts");
    }
    expect(km_lookup(KB_ON_KEY, KM_FACE)->op == AB_CANCEL, "the ON key (keys.h) is C");
    for (int k = 0; k < KB_KEYS; k++) {
        bool there = kb_exists(k / KB_COLS, k % KB_COLS);
        for (int s = 0; s < 3; s++) {
            const km_action *a = km_lookup(k, s);
            expect(a->legend != NULL, "every slot has a legend string");
            if (!there) expect(a->kind == KM_NONE, "a key that is not there does nothing");
            if (a->kind == KM_UNDEFINED) undefined++;
            else if (a->kind != KM_NONE) defined++;
            /* every legend in our face (the soft keys show menu names) */
            if (a->legend && screen_missing(a->legend)) {
                snprintf(m, sizeof m, "key %d shift %d: '%.20s' has a character the face lacks", k, s, a->legend);
                expect(0, m);
            }
        }
        expect(!there || km_lookup(k, KM_FACE)->kind != KM_NONE, "every key does something unshifted");
    }
    expect(km_lookup(-1, 0)->kind == KM_NONE && km_lookup(KB_KEYS, 0)->kind == KM_NONE &&
               km_lookup(0, 3)->kind == KM_NONE, "out of range: none");

    /* against the core: an op a key names is a real op, and a key that asks for an argument gets
       one its op takes (keyed in program entry: the line is recorded and named) */
    for (int k = 0; k < KB_KEYS; k++)
        for (int s = 0; s < 3; s++) {
            const km_action *a = km_lookup(k, s);
            if (a->kind != KM_OP && a->kind != KM_PROMPT) continue;
            expect(a->op >= 0 && a->op <= AB_MODEL, "an op in range");
            if (a->kind != KM_PROMPT) continue;
            int arg = a->prompt == KM_ARG_VAR ? AB_VAR_A : a->prompt == KM_ARG_LABEL ? 2 : 1 * 1000 + 1;
            ab_init(&c);
            ab_do(&c, AB_PRGM);
            ab_do_arg(&c, AB_LBL, 1);           /* B, the target B001 */
            int before = c.prog_len;
            abn_status st = ab_do_arg(&c, (ab_op)a->op, arg);
            char t[64];
            bool ok = st == ABN_OK && c.prog_len == before + 1 && ab_prog_line(&c, c.pc, NULL, t, sizeof t) > 0;
            snprintf(m, sizeof m, "key %d shift %d (%s): its argument kind is not its op's", k, s, a->legend);
            expect(ok, m);
        }

    /* the menus (#3877): PRGM's six soft keys; no other menu defined */
    {
        const km_menu *pm = km_menu_at(0);
        expect(pm && !strcmp(pm->name, "PRGM"), "menu 0 is PRGM");
        expect(km_menu_at(1) == NULL && km_menu_at(-1) == NULL, "only PRGM is defined");
        if (pm) {
            static const struct { km_kind kind; int op, prompt; const char *legend; } P[6] = {
                {KM_OP, AB_PRGM, 0, "PRGM"}, {KM_PROMPT, AB_LBL, KM_ARG_LABEL, "LBL"},
                {KM_PROMPT, AB_INPUT, KM_ARG_VAR, "INPUT"}, {KM_OP, AB_SST, 0, "SST"},
                {KM_OP, AB_BST, 0, "BST"}, {KM_OP, AB_CLPGM, 0, "CLPGM"}};
            for (int i = 0; i < 6; i++) {
                const km_action *it = &pm->item[i];
                snprintf(m, sizeof m, "PRGM menu soft key %d: %s", i + 1, P[i].legend);
                expect(it->kind == P[i].kind && it->op == P[i].op && it->prompt == P[i].prompt && it->legend &&
                           !strcmp(it->legend, P[i].legend) && screen_missing(it->legend) == 0 && screen_cells(it->legend) <= 5,
                       m);
            }
            /* a prompting item takes its argument in program entry, as a key's does */
            ab_init(&c);
            ab_do(&c, AB_PRGM);
            int before = c.prog_len;
            ab_do_arg(&c, (ab_op)pm->item[2].op, km_prompt_arg(pm->item[2].prompt, km_letter_of(12)));
            char t[32] = "";
            ab_prog_line(&c, c.pc, NULL, t, sizeof t);
            expect(c.prog_len == before + 1 && !strcmp(t, "INPUT G"), "the menu's INPUT then G: INPUT G");
        }
        for (int k = 0; k < KB_KEYS; k++)
            for (int sh = 0; sh < 3; sh++) {
                const km_action *a = km_lookup(k, sh);
                if (a->kind == KM_MENU && a->arg >= 0) expect(km_menu_at(a->arg) != NULL, "a menu key names a defined menu");
            }
    }

    /* the letter layer, as abacus sent it (#3826): key, then letter or role */
    {
        static const struct { int key; const char *what; } L[] = {
            {6, "A"}, {7, "B"}, {8, "C"}, {9, "D"}, {10, "E"}, {11, "F"}, {12, "G"}, {13, "H"}, {14, "I"},
            {15, "J"}, {16, "K"}, {17, "L"}, {18, "M"}, {19, "N"}, {20, "O"}, {21, "P"}, {22, "Q"}, {24, "R"},
            {25, "S"}, {26, "T"}, {27, "U"}, {28, "/"}, {31, "V"}, {32, "W"}, {33, "X"}, {34, "*"}, {37, "Y"},
            {38, "Z"}, {39, "(I)"}, {40, "-"}, {42, "ESC"}, {43, "(J)"}, {46, "+"},
        };
        int seen[KB_KEYS] = {0}, letters = 0, used[26] = {0};
        for (size_t i = 0; i < sizeof L / sizeof L[0]; i++) {
            const km_letter *l = km_letter_of(L[i].key);
            const char *w = L[i].what;
            bool ok;
            seen[L[i].key] = 1;
            if (strlen(w) == 1 && w[0] >= 'A' && w[0] <= 'Z') ok = l->kind == KL_LETTER && l->value == w[0] - 'A';
            else if (!strcmp(w, "(I)")) ok = l->kind == KL_IND_I;
            else if (!strcmp(w, "(J)")) ok = l->kind == KL_IND_J;
            else if (!strcmp(w, "ESC")) ok = l->kind == KL_ESC;
            else ok = l->kind == KL_ARITH && l->value == (w[0] == '+' ? AB_ADD : w[0] == '-' ? AB_SUB : w[0] == '*' ? AB_MUL : AB_DIV);
            snprintf(m, sizeof m, "letter layer: key %d should be %s (kind %d value %d)", L[i].key, w, l->kind, l->value);
            expect(ok, m);
        }
        for (int k = 0; k < KB_KEYS; k++) {
            const km_letter *l = km_letter_of(k);
            if (!seen[k]) {
                snprintf(m, sizeof m, "letter layer: key %d should type nothing", k);
                expect(l->kind == KL_NONE, m);
            }
            if (l->kind == KL_LETTER && l->value >= 0 && l->value < 26) letters += !used[l->value]++;
            if (l->kind != KL_NONE) expect(kb_exists(k / KB_COLS, k % KB_COLS), "a letter on a key that is not there");
        }
        expect(letters == 26, "the letter layer: 26 distinct letters");
        /* the arithmetic keys are the keymap's own + - x / (so STO + reads as on the 35s) */
        for (int k = 0; k < KB_KEYS; k++)
            if (km_letter_of(k)->kind == KL_ARITH)
                expect(km_lookup(k, KM_FACE)->kind == KM_OP && km_lookup(k, KM_FACE)->op == km_letter_of(k)->value,
                       "an arithmetic letter-layer key is that arithmetic key");
        expect(km_letter_of(-1)->kind == KL_NONE && km_letter_of(KB_KEYS)->kind == KL_NONE, "letters out of range: none");
    }
    /* STO and RCL with arithmetic (UG p.3-6, 3-7); nothing else takes it */
    {
        static const int A[4] = {AB_ADD, AB_SUB, AB_MUL, AB_DIV};
        static const int S[4] = {AB_STOADD, AB_STOSUB, AB_STOMUL, AB_STODIV};
        static const int R[4] = {AB_RCLADD, AB_RCLSUB, AB_RCLMUL, AB_RCLDIV};
        for (int i = 0; i < 4; i++) {
            expect(km_arith_op(AB_STO, A[i]) == S[i] && km_arith_op(AB_RCL, A[i]) == R[i], "STO/RCL with arithmetic");
            expect(km_arith_op(AB_VIEW, A[i]) == -1 && km_arith_op(AB_XEQ, A[i]) == -1, "no arithmetic for VIEW, XEQ");
        }
        expect(km_arith_op(AB_STO, AB_SIN) == -1, "STO with SIN: nothing");
    }
    /* every letter-layer key, under every prompt, gives an argument the core takes: each prompt op
       keyed with it in program entry records a named line ("STO G" from key 12) */
    {
        static const int OPS[] = {AB_STO, AB_RCL, AB_VIEW, AB_ISG, AB_DSE, AB_LBL, AB_XEQ, AB_GTO};
        static const int KIND[] = {KM_ARG_VAR, KM_ARG_VAR, KM_ARG_VAR, KM_ARG_VAR, KM_ARG_VAR, KM_ARG_LABEL,
                                   KM_ARG_TARGET, KM_ARG_TARGET};
        int lines = 0;
        for (int k = 0; k < KB_KEYS; k++) {
            const km_letter *l = km_letter_of(k);
            if (l->kind != KL_LETTER && l->kind != KL_IND_I && l->kind != KL_IND_J) continue;
            for (int o = 0; o < 8; o++) {
                int arg = km_prompt_arg(KIND[o], l);
                if (KIND[o] == KM_ARG_LABEL && l->kind != KL_LETTER) {
                    expect(arg == -1, "a label cannot be (I) or (J)");
                    continue;
                }
                ab_init(&c);
                ab_do(&c, AB_PRGM);
                for (int lab = 0; lab < 26 && OPS[o] != AB_LBL; lab++) ab_do_arg(&c, AB_LBL, lab);  /* targets exist */
                int before = c.prog_len;
                char t[64] = "";
                abn_status st = ab_do_arg(&c, (ab_op)OPS[o], arg);
                bool ok = arg >= 0 || arg == AB_TGT_IND_I || arg == AB_TGT_IND_J;
                ok = ok && st == ABN_OK && c.prog_len == before + 1 && ab_prog_line(&c, c.pc, NULL, t, sizeof t) > 0;
                snprintf(m, sizeof m, "key %d with op %d: argument %d not taken (%s)", k, OPS[o], arg, t);
                expect(ok, m);
                lines += ok;
            }
        }
        ab_init(&c);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, km_lookup(12, KM_FACE)->op, km_prompt_arg(KM_ARG_VAR, km_letter_of(12)));
        char t[32];
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(strcmp(t, "STO G") == 0, "STO then key 12 (letter G): STO G");
        ab_do_arg(&c, km_arith_op(AB_STO, km_letter_of(46)->value), km_prompt_arg(KM_ARG_VAR, km_letter_of(39)));
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(strcmp(t, "STO+ (I)") == 0, "STO, +, then key 39 ((I)): STO+ (I)");
        ab_do_arg(&c, AB_STO, km_prompt_arg(KM_ARG_VAR, km_letter_of(43)));
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(strcmp(t, "STO (J)") == 0, "STO then key 43 ((J)): STO (J)");
        ab_do_arg(&c, AB_LBL, km_prompt_arg(KM_ARG_LABEL, km_letter_of(12)));
        ab_do_arg(&c, AB_XEQ, km_prompt_arg(KM_ARG_TARGET, km_letter_of(12)));
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(strcmp(t, "XEQ G001") == 0, "XEQ then key 12: XEQ G001 (a label's first line, UG p.13-10)");
        ab_do_arg(&c, AB_GTO, km_prompt_arg(KM_ARG_TARGET, km_letter_of(39)));
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(strcmp(t, "GTO (I)") == 0, "GTO then key 39: GTO (I)");
        snprintf(m, sizeof m, "letter-layer arguments taken: %d", lines);
        expect(lines == 26 * 8 + 2 * 7, m);
    }

    printf("keymap_test: %d/%d checks; %d actions, %d undefined (listed above)\n", checks - bad, checks, defined,
           undefined);
    for (int k = 0; k < KB_KEYS; k++)
        for (int s = 0; s < 3; s++)
            if (km_lookup(k, s)->kind == KM_UNDEFINED)
                printf("keymap_test: undefined: key %d,%d shift %d '%s'\n", k / KB_COLS, k % KB_COLS, s, km_lookup(k, s)->legend);
    return bad != 0;
}
