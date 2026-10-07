/* keymap_test: the generated keymap against a reading of the layout made by hand (layout/stu32-v0.json,
 * row by row), and against the core: every op a key names exists, every key that asks for an
 * argument gets one of the kind its op takes, every legend can be drawn in our face. */
#include <stdio.h>
#include <string.h>

#include "calc.h"
#include "const.h"
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
#define MENU(r, c, s, l) {r, c, s, KM_MENU, 0, -1, 0, l}  /* a menu by its legend (see below) */

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
    OPK(3, 4, KM_FACE, AB_KBKSP, "\xe2\x86\x90"), {3, 4, KM_GOLD, KM_MENU, 0, 3, 0, "CLEAR"} /* menu 3 (unit 021) */,
    {3, 4, KM_BLUE, KM_DEVICE, 0, KM_DEV_UNDO, 0, "UNDO"},
    /* row 4 */
    {4, 0, KM_FACE, KM_PROMPT, AB_XEQ, 0, KM_ARG_TARGET, "XEQ"}, {4, 0, KM_GOLD, KM_PROMPT, AB_LBL, 0, KM_ARG_LABEL, "LBL"},
    OPK(4, 0, KM_BLUE, AB_RTN, "RTN"),
    OPK(4, 1, KM_FACE, AB_K7, "7"), {4, 1, KM_GOLD, KM_PROMPT, AB_SOLVE, 0, KM_ARG_VAR, "SOLVE"} /* unit 019 rule 4 */,
    MENU(4, 1, KM_BLUE, "GRAPH"),
    OPK(4, 2, KM_FACE, AB_K8, "8"), {4, 2, KM_GOLD, KM_PROMPT, AB_INTEG, 0, KM_ARG_VAR, "\xe2\x88\xab"} /* rule 4 */,
    MENU(4, 2, KM_BLUE, "TABLE"),
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
    OPK(6, 2, KM_FACE, AB_K2, "2"), {6, 2, KM_GOLD, KM_MENU, 0, 1, 0, "DISP"} /* menu 1 (#3901) */, {6, 2, KM_BLUE, KM_MENU, 0, 2, 0, "\xe2\x88\xa1MODE"} /* menu 2 (decision 33) */,
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
        /* a menu: the one its legend names, or none defined yet (-1); which are defined is
           checked below, by name, so the generator's order of menus is free */
        bool arg_ok = w->kind == KM_MENU ? a->arg == -1 || (km_menu_at(a->arg) && !strcmp(km_menu_at(a->arg)->name, w->legend))
                                         : a->arg == w->arg;
        expect(a->kind == w->kind && a->op == w->op && arg_ok && a->prompt == w->prompt && a->legend &&
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
            expect(a->op >= 0 && a->op <= AB_OP_LAST, "an op in range");
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
        const km_menu *dm = km_menu_at(1);
        expect(dm && !strcmp(dm->name, "DISP"), "menu 1 is DISP (#3901)");
        const km_menu *am = km_menu_at(2);
        expect(am && !strcmp(am->name, "\xe2\x88\xa1MODE"), "menu 2 is the angle mode (decision 33)");
        if (am) {
            expect(am->item[0].kind == KM_OP && am->item[0].op == AB_DEG && !strcmp(am->item[0].legend, "DEG") &&
                       am->item[1].kind == KM_OP && am->item[1].op == AB_RAD && !strcmp(am->item[1].legend, "RAD") &&
                       am->item[2].kind == KM_OP && am->item[2].op == AB_GRAD && !strcmp(am->item[2].legend, "GRAD"),
                   "the angle-mode menu: DEG, RAD, GRAD");
            expect(am->item[3].kind == KM_NONE && am->item[4].kind == KM_NONE && am->item[5].kind == KM_NONE &&
                       am->item[3].legend && am->item[5].legend, "its soft keys 4-6 empty");
        }
        expect(km_menu_at(-1) == NULL, "no menu at -1");
        /* unit 019: the drafted menus, item by item (work/019-menus.md rule 1's table) */
        {
#define O(op, arg, l) {KM_OP, op, arg, 0, l}
#define P(op, k, l) {KM_PROMPT, op, 0, k, l}
#define N {KM_NONE, 0, 0, 0, ""}
#define T(rel, l) {KM_OP, AB_TEST, rel, 0, l}
            static const struct { const char *name; km_action item[6]; } D[] = {
                {"HYP", {O(AB_SINH, 0, "SINH"), O(AB_COSH, 0, "COSH"), O(AB_TANH, 0, "TANH"), O(AB_ASINH, 0, "ASINH"),
                         O(AB_ACOSH, 0, "ACOSH"), O(AB_ATANH, 0, "ATANH")}},
                {"STAT", {O(AB_XBAR, 0, "x\xcc\x84"), O(AB_YBAR, 0, "y\xcc\x84"), O(AB_SX, 0, "sx"), O(AB_SY, 0, "sy"),
                          O(AB_PSX, 0, "\xcf\x83x"), O(AB_PSY, 0, "\xcf\x83y")}},
                {"L.R.", {O(AB_XHAT, 0, "x\xcc\x82"), O(AB_YHAT, 0, "y\xcc\x82"), O(AB_CORR, 0, "r"), O(AB_SLOPE, 0, "m"),
                          O(AB_YINT, 0, "b"), O(AB_XWBAR, 0, "x\xcc\x84w")}},
                {"SUMS", {O(AB_NSUM, 0, "n"), O(AB_SUMX, 0, "\xce\xa3x"), O(AB_SUMY, 0, "\xce\xa3y"),
                          O(AB_SUMX2, 0, "\xce\xa3x\xc2\xb2"), O(AB_SUMY2, 0, "\xce\xa3y\xc2\xb2"), O(AB_SUMXY, 0, "\xce\xa3xy")}},
                {"PROB", {O(AB_NCR, 0, "Cn,r"), O(AB_NPR, 0, "Pn,r"), O(AB_FACT, 0, "n!"), O(AB_RAND, 0, "RAND"),
                          O(AB_SEED, 0, "SEED"), N}},
                {"FLAGS", {P(AB_SF, KM_ARG_DIGITS, "SF"), P(AB_CF, KM_ARG_DIGITS, "CF"), P(AB_FS, KM_ARG_DIGITS, "FS?"),
                           O(AB_BEEP, 0, "BEEP"), N, N}},   /* unit 027: BEEP, ours */
                {"x?y", {T(0, "x\xe2\x89\xa0y?"), T(1, "x\xe2\x89\xa4y?"), T(2, "x<y?"), T(3, "x>y?"), T(4, "x\xe2\x89\xa5y?"),
                         T(5, "x=y?")}},
                {"x?0", {T(6, "x\xe2\x89\xa0" "0?"), T(7, "x\xe2\x89\xa4" "0?"), T(8, "x<0?"), T(9, "x>0?"),
                         T(10, "x\xe2\x89\xa5" "0?"), T(11, "x=0?")}},
                {"TEST", {T(5, "x=y?"), T(0, "x\xe2\x89\xa0y?"), T(11, "x=0?"), T(6, "x\xe2\x89\xa0" "0?"),
                          P(AB_FS, KM_ARG_DIGITS, "FS?"), T(2, "x<y?")}},
                /* abacus #3958: the short forms, the box marking a submenu (no ▸) */
                {"BASE", {O(AB_DEC, 0, "DEC"), O(AB_HEX, 0, "HEX"), O(AB_OCT, 0, "OCT"), O(AB_BIN, 0, "BIN"),
                          {KM_MENU, 0, -2, 0, "LOGIC"}, {KM_MENU, 0, -3, 0, "SUFX"}}},
                {"LOGIC", {O(AB_AND, 0, "AND"), O(AB_OR, 0, "OR"), O(AB_XOR, 0, "XOR"), O(AB_NOT, 0, "NOT"),
                           O(AB_NAND, 0, "NAND"), O(AB_NOR, 0, "NOR")}},
                {"SUFFIX", {O(AB_SUF, 10, "d"), O(AB_SUF, 16, "h"), O(AB_SUF, 8, "o"), O(AB_SUF, 2, "b"), N, N}},
                {"CMPLX", {O(AB_KI, 0, "i"), O(AB_KTHETA, 0, "\xce\xb8"), O(AB_ARG, 0, "ARG"), O(AB_KVEC, 0, "["), N, N}},
                {"FRAC", {O(AB_FDISP, 0, "FDISP"), O(AB_SETC, 0, "/c"), O(AB_RCLC, 0, "RCL/c"), O(AB_FRACFMT, 0, "PREC"),
                          O(AB_FRACFMT, 1, "FCTR"), O(AB_FRACFMT, 2, "FIXED")}},
                /* abacus #3958: EXP as drafted (LN(1+x) shortened to fit five cells) */
                {"EXP", {O(AB_TWOX, 0, "2\xcb\xa3"), O(AB_LOG2, 0, "LOG\xe2\x82\x82"), O(AB_EXPM1, 0, "e\xcb\xa3\xe2\x88\x92" "1"),
                         O(AB_LN1P, 0, "LN1+x"), N, N}},
                {"CONV", {O(AB_TOKG, 0, "\xe2\x86\x92kg"), O(AB_TOLB, 0, "\xe2\x86\x92lb"), O(AB_TOC, 0, "\xe2\x86\x92\xc2\xb0" "C"),
                          O(AB_TOF, 0, "\xe2\x86\x92\xc2\xb0" "F"), O(AB_TOCM, 0, "\xe2\x86\x92" "cm"), O(AB_TOIN, 0, "\xe2\x86\x92in")}},
                {"UNITS", {O(AB_TOL, 0, "\xe2\x86\x92l"), O(AB_TOGAL, 0, "\xe2\x86\x92gal"), O(AB_TOKM, 0, "\xe2\x86\x92km"),
                           O(AB_TOMILE, 0, "\xe2\x86\x92mile"), N, N}},
                {"ANGLE", {O(AB_TOPOL, 0, "\xe2\x86\x92POL"), O(AB_TOREC, 0, "\xe2\x86\x92REC"), O(AB_HMS, 0, "\xe2\x86\x92HMS"),
                           O(AB_HMSINV, 0, "HMS\xe2\x86\x92"), O(AB_TODEG, 0, "\xe2\x86\x92" "DEG"), O(AB_TORAD, 0, "\xe2\x86\x92RAD")}},
                {"VARS", {P(AB_XVAR, KM_ARG_VAR, "x\xe2\x86\x94"), P(AB_VIEW, KM_ARG_VAR, "VIEW"), O(AB_CLVARS, 0, "CLR"), N, N, N}},
                {"STACK", {O(AB_CLSTK, 0, "CLSTK"), O(AB_LASTX, 0, "LASTx"), O(AB_RUP, 0, "R\xe2\x86\x91"), O(AB_XY, 0, "x\xe2\x86\x94y"),
                           O(AB_SHOW, 0, "SHOW"), N}},     /* unit 030: SHOW at F5, held */
                /* unit 029: STU at F3, the entry RPN and ALG at F4 and F5 (ALG shown in STU only: the app's) */
                {"MODE", {O(AB_MODEL, 33, "33s"), O(AB_MODEL, 35, "35s"), O(AB_MODEL, AB_MODEL_STU, "STU"), O(AB_ENTRYM, 0, "RPN"),
                          O(AB_ENTRYM, 1, "ALG"), N}},
                /* unit 032: TABLE on blue 8, and the open table's bar */
                {"TABLE", {P(AB_TVAR, KM_ARG_VAR, "VAR"), O(AB_TSTART, 0, "START"), O(AB_TSTEP, 0, "STEP"), O(AB_TABLE, 0, "GO"), N, N}},
                /* unit 033: GRAPH on blue 7, and the open graph's bar */
                {"GRAPH", {O(AB_XMIN, 0, "XMIN"), O(AB_XMAX, 0, "XMAX"), O(AB_YMIN, 0, "YMIN"), O(AB_YMAX, 0, "YMAX"),
                           O(AB_GAUTO, 0, "AUTO"), O(AB_GRAPH, 0, "GO")}},
                {"GRAPH BAR", {O(AB_GLEFT, 0, "\xe2\x97\x80"), O(AB_GRIGHT, 0, "\xe2\x96\xb6"), N, N, N, N}},
                {"TABLE BAR", {O(AB_TUP, 0, "\xe2\x96\xb2"), O(AB_TDOWN, 0, "\xe2\x96\xbc"), N, N, N, N}},
                {"ALG LINE", {O(AB_KLEFT, 0, "\xe2\x97\x80"), O(AB_KRIGHT, 0, "\xe2\x96\xb6"), O(AB_KPAREN, 0, "( )"), N,
                              O(AB_HUP, 0, "\xe2\x96\xb2"), O(AB_HDOWN, 0, "\xe2\x96\xbc")}},
                /* unit 022: the device's settings, a KM_DEVICE action's op its value */
                {"RADIX", {{KM_DEVICE, '.', KM_DEV_RADIX, 0, "."}, {KM_DEVICE, ',', KM_DEV_RADIX, 0, ","},
                           {KM_DEVICE, 1, KM_DEV_GROUP, 0, "1,000"}, {KM_DEVICE, ' ', KM_DEV_GROUP, 0, "1 000"},
                           {KM_DEVICE, 0, KM_DEV_GROUP, 0, "none"}, N}},
                {"SETUP", {{KM_DEVICE, 0, KM_DEV_OFFTIME, 0, "OFF"}, {KM_DEVICE, 0, KM_DEV_VOL, 0, "VOL"},
                           {KM_MENU, 0, -4, 0, "BEEP"}, {KM_MENU, 0, -6, 0, "CLOCK"}, N, N}},
                /* unit 027: the buzzer's uses (the op a DEV_USE_* bit), two pages; CLOCK; SYS's views */
                {"BEEP", {{KM_DEVICE, 1, KM_DEV_USE, 0, "CLICK"}, {KM_DEVICE, 2, KM_DEV_USE, 0, "ERROR"},
                          {KM_DEVICE, 4, KM_DEV_USE, 0, "PRGM"}, {KM_DEVICE, 8, KM_DEV_USE, 0, "BATT"},
                          {KM_DEVICE, 16, KM_DEV_USE, 0, "ON"}, {KM_MENU, 0, -5, 0, "\xe2\x96\xb8"}}},
                {"BEEP2", {{KM_DEVICE, 32, KM_DEV_USE, 0, "DONE"}, {KM_DEVICE, 0, KM_DEV_USEALL, 0, "ALL"}, N, N, N,
                           {KM_MENU, 0, -4, 0, "\xe2\x96\xb8"}}},
                {"CLOCK", {{KM_DEVICE, 0, KM_DEV_CLOCKSET, 0, "SET"}, {KM_DEVICE, 0, KM_DEV_H24, 0, "12/24"}, N, N, N, N}},
                {"SYS", {{KM_DEVICE, 1, KM_DEV_VIEW, 0, "BATT"}, {KM_DEVICE, 2, KM_DEV_VIEW, 0, "VER"},
                         {KM_DEVICE, 3, KM_DEV_VIEW, 0, "FLASH"}, N, N, N}},
                {"POW", {O(AB_IP, 0, "IP"), O(AB_FP, 0, "FP"), O(AB_INTG, 0, "INTG"), O(AB_RND, 0, "RND"), O(AB_SGN, 0, "SGN"),
                         O(AB_ABS, 0, "ABS")}},
            };
#undef O
#undef P
#undef N
#undef T
            for (size_t d = 0; d < sizeof D / sizeof D[0]; d++) {
                const km_menu *mn = NULL;
                int at = -1;
                for (int i = 0; km_menu_at(i); i++)
                    if (!strcmp(km_menu_at(i)->name, D[d].name)) mn = km_menu_at(at = i);
                snprintf(m, sizeof m, "menu %s is defined", D[d].name);
                expect(mn != NULL, m);
                if (!mn) continue;
                for (int i = 0; i < 6; i++) {
                    const km_action *it = &mn->item[i], *want = &D[d].item[i];
                    bool same = it->kind == want->kind && it->op == want->op && it->prompt == want->prompt &&
                                !strcmp(it->legend, want->legend);
                    if (want->kind == KM_MENU) {    /* a submenu by name: -2 LOGIC, -3 SUFFIX, -4 BEEP, -5 BEEP2,
                                                       -6 CLOCK */
                        static const char *const SUB[] = {"LOGIC", "SUFFIX", "BEEP", "BEEP2", "CLOCK"};
                        const km_menu *sub = km_menu_at(it->arg);
                        same = same && sub && !strcmp(sub->name, SUB[-want->arg - 2]);
                    } else {
                        same = same && it->arg == want->arg;
                    }
                    snprintf(m, sizeof m, "menu %s soft key %d is %s", D[d].name, i + 1, want->legend);
                    expect(same, m);
                }
                /* every menu but the submenus (BASE's two, BEEP's pages, CLOCK) has its key */
                bool keyed = false;
                for (int k = 0; k < KB_KEYS; k++)
                    for (int sh = 0; sh < 3; sh++)
                        if (km_lookup(k, sh)->kind == KM_MENU && km_lookup(k, sh)->arg == at) keyed = true;
                snprintf(m, sizeof m, "menu %s is on its key", D[d].name);
                bool sub = !strcmp(D[d].name, "LOGIC") || !strcmp(D[d].name, "SUFFIX") || !strcmp(D[d].name, "BEEP") ||
                           !strcmp(D[d].name, "BEEP2") || !strcmp(D[d].name, "CLOCK") ||
                           !strcmp(D[d].name, "ALG LINE") || !strcmp(D[d].name, "TABLE BAR") ||
                           !strcmp(D[d].name, "GRAPH BAR");  /* reached by a menu;
                                                                  the app's bars (units 029, 032) */
                expect(keyed == !sub, m);
            }
            /* CONST pages the core's constants (rule 3); SOLVE and integrate ask for the variable (rule 4) */
            const km_menu *km = NULL;
            for (int i = 0; km_menu_at(i); i++)
                if (!strcmp(km_menu_at(i)->name, "CONST")) km = km_menu_at(i);
            expect(km && km->paged == AB_CONST && km_lookup(13, KM_GOLD)->kind == KM_MENU && km_menu_at(km_lookup(13, KM_GOLD)->arg) == km,
                   "CONST (gold RCL) pages the constants");
            const km_action *so = km_lookup(25, KM_GOLD), *in = km_lookup(26, KM_GOLD);
            expect(so->kind == KM_PROMPT && so->op == AB_SOLVE && so->prompt == KM_ARG_VAR && !strcmp(so->legend, "SOLVE"),
                   "SOLVE (gold 7) asks for a variable, then solves");
            expect(in->kind == KM_PROMPT && in->op == AB_INTEG && in->prompt == KM_ARG_VAR && !strcmp(in->legend, "\xe2\x88\xab"),
                   "integrate (gold 8) asks for a variable, then integrates");
            /* rule 5: these stay not defined */
            static const char *const LATER[] = {"MEM", "TUTOR", "APPS"};   /* RADIX, SETUP: unit 022; EXP #3958; SYS: unit 027; TABLE: 032 */
            for (size_t i = 0; i < sizeof LATER / sizeof LATER[0]; i++) {
                bool found = false, undefined = true;
                for (int k = 0; k < KB_KEYS; k++)
                    for (int sh = 1; sh < 3; sh++)
                        if (!strcmp(km_lookup(k, sh)->legend, LATER[i])) {
                            found = true;
                            undefined = km_lookup(k, sh)->kind == KM_MENU && km_lookup(k, sh)->arg == -1;
                        }
                snprintf(m, sizeof m, "%s stays not defined (rule 5)", LATER[i]);
                expect(found && undefined, m);
            }
        }
        /* abacus #3958: every soft legend and every constant's symbol draws with no hollow box, fits its
           box (five cells at scale 2, screen.c) and is distinct within its menu (or page) */
        for (int i = 0; km_menu_at(i); i++) {
            const km_menu *mn = km_menu_at(i);
            for (int j = 0; j < 6; j++) {
                const char *l = mn->item[j].legend;
                if (!l[0]) continue;
                snprintf(m, sizeof m, "menu %s: the legend %s draws whole and fits five cells", mn->name, l);
                expect(screen_missing(l) == 0 && screen_cells(l) <= 5, m);
                for (int k = 0; k < j; k++) {
                    snprintf(m, sizeof m, "menu %s: soft keys %d and %d differ", mn->name, k + 1, j + 1);
                    expect(strcmp(mn->item[k].legend, l) != 0, m);
                }
            }
        }
        for (int i = 0; i < ab_const_count(); i++) {
            const char *l = ab_const_symbol(i);
            snprintf(m, sizeof m, "constant %d: its symbol draws whole and fits five cells", i);
            expect(l && screen_missing(l) == 0 && screen_cells(l) <= 5, m);
            for (int k = 0; l && k < i; k++) {
                snprintf(m, sizeof m, "constants %d and %d have different symbols", k, i);
                expect(strcmp(ab_const_symbol(k), l) != 0, m);
            }
        }
        /* the labels app.c makes (PGM, EQN, the confirmation, SETUP's values, CONST's more) */
        static const char *const APPL[] = {"PGM", "EQN", "Y", "N", "OFF10", "OFF30", "VOL 4", "more"};
        for (size_t i = 0; i < sizeof APPL / sizeof APPL[0]; i++)
            expect(screen_missing(APPL[i]) == 0 && screen_cells(APPL[i]) <= 5, "an app label fits");
        /* CLEAR (unit 019's draft order with unit 021's ALL and CLVARx) */
        const km_menu *cm = km_menu_at(3);
        expect(cm && !strcmp(cm->name, "CLEAR"), "menu 3 is CLEAR");
        if (cm) {
            /* abacus #3958: the 35s's option names (the title supplies CL), STK kept second */
            static const struct { km_kind kind; int op, prompt; const char *legend; } CL[6] = {
                {KM_OP, AB_CLX, 0, "x"}, {KM_OP, AB_CLSTK, 0, "STK"}, {KM_OP, AB_CLVARS, 0, "VARS"},
                {KM_OP, AB_CLSIGMA, 0, "\xce\xa3"}, {KM_CONFIRM, AB_CLALL, 0, "ALL"},
                {KM_PROMPT, AB_CLVARX, KM_ARG_ADDR, "VARx"}};
            for (int i = 0; i < 6; i++) {
                const km_action *it = &cm->item[i];
                snprintf(m, sizeof m, "CLEAR's soft key %d is %s", i + 1, CL[i].legend);
                expect(it->kind == CL[i].kind && it->op == CL[i].op && it->prompt == CL[i].prompt &&
                           !strcmp(it->legend, CL[i].legend), m);
            }
            /* CLVARx's three digits are an argument its op records (UG p.1-5's CLVAR056) */
            char t[64];
            ab_init(&c);
            ab_do(&c, AB_PRGM);
            abn_status st = ab_do_arg(&c, AB_CLVARX, 56);
            expect(st == ABN_OK && c.prog_len == 1 && ab_prog_line(&c, 0, NULL, t, sizeof t) > 0 && !strcmp(t, "CLVAR056"),
                   "CLVARx 056 in program entry: the line CLVAR056");
        }
        if (dm) {
            static const struct { km_kind kind; int op, prompt; const char *legend; } D[6] = {
                {KM_PROMPT, AB_SETFIX, KM_ARG_DIGITS, "FIX"}, {KM_PROMPT, AB_SETSCI, KM_ARG_DIGITS, "SCI"},
                {KM_PROMPT, AB_SETENG, KM_ARG_DIGITS, "ENG"}, {KM_OP, AB_SETALL, 0, "ALL"},
                {KM_OP, AB_XIY, 0, "xiy"}, {KM_OP, AB_RTA, 0, "r\xce\xb8" "a"}};
            for (int i = 0; i < 6; i++) {
                const km_action *it = &dm->item[i];
                snprintf(m, sizeof m, "DISP menu soft key %d: %s", i + 1, D[i].legend);
                expect(it->kind == D[i].kind && it->op == D[i].op && it->prompt == D[i].prompt && it->legend &&
                           !strcmp(it->legend, D[i].legend) && screen_missing(it->legend) == 0, m);
            }
        }
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
        ab_do_arg(&c, AB_MODEL, 35);            /* the letter layer's (I) and (J) are 35s mode's; app.c maps
                                                   them to i and (i) in 33s mode (unit 025) */
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
