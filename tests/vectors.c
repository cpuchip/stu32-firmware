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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"

#include "calc.h"
#include "state.h"
#include "store.h"
#include "norm.h"

#define LINE_MAX_ 16384
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
    {"HMS", AB_HMS}, {"HMSINV", AB_HMSINV}, {"TODEG", AB_TODEG}, {"TORAD", AB_TORAD},
    {"TOKG", AB_TOKG}, {"TOLB", AB_TOLB}, {"TOC", AB_TOC}, {"TOF", AB_TOF},
    {"TOCM", AB_TOCM}, {"TOIN", AB_TOIN}, {"TOL", AB_TOL}, {"TOGAL", AB_TOGAL},
    {"TOKM", AB_TOKM}, {"TOMILE", AB_TOMILE}, {"FACT", AB_FACT}, {"NCR", AB_NCR}, {"NPR", AB_NPR},
    {"RAND", AB_RAND}, {"SEED", AB_SEED}, {"IP", AB_IP}, {"FP", AB_FP}, {"INTG", AB_INTG},
    {"RND", AB_RND}, {"ABS", AB_ABS}, {"SGN", AB_SGN}, {"TOPOL", AB_TOPOL}, {"TOREC", AB_TOREC},
    {"ALL", AB_SETALL},
    {"DEC", AB_DEC}, {"HEX", AB_HEX}, {"OCT", AB_OCT}, {"BIN", AB_BIN},
    {"HA", AB_KA}, {"HB", AB_KB}, {"HC", AB_KC}, {"HD", AB_KD}, {"HE", AB_KE_HEX}, {"HF", AB_KF},
    {"CI", AB_KI}, {"CA", AB_KTHETA}, {"XIY", AB_XIY}, {"RTA", AB_RTA}, {"ARG", AB_ARG},
    {"[", AB_KVEC}, {",", AB_KCOMMA},
    {"PRGM", AB_PRGM}, {"RTN", AB_RTN}, {"RS", AB_RS}, {"PSE", AB_PSE}, {"SST", AB_SST},
    {"BST", AB_BST}, {"CLPGM", AB_CLPGM}, {"C", AB_CANCEL},
    {"EQN", AB_EQN}, {"EQTOP", AB_EQTOP} /* EQN LIST's NEW (tutor #4941) */, {"SPACE", AB_KSPACE}, {"=", AB_KEQ}, {"()", AB_KPAREN}, {"<", AB_KLEFT}, {">", AB_KRIGHT},
    {"AND", AB_AND}, {"XOR", AB_XOR}, {"OR", AB_OR}, {"NOT", AB_NOT}, {"NAND", AB_NAND}, {"NOR", AB_NOR},
    {"SIGMA+", AB_SIGMAPLUS}, {"SIGMA-", AB_SIGMAMINUS}, {"CLSIGMA", AB_CLSIGMA},
    {"XBAR", AB_XBAR}, {"YBAR", AB_YBAR}, {"XWBAR", AB_XWBAR}, {"SX", AB_SX}, {"SY", AB_SY},
    {"PSX", AB_PSX}, {"PSY", AB_PSY}, {"XHAT", AB_XHAT}, {"YHAT", AB_YHAT}, {"CORR", AB_CORR},
    {"SLOPE", AB_SLOPE}, {"YINT", AB_YINT}, {"NSUM", AB_NSUM}, {"SUMX", AB_SUMX},
    {"SUMY", AB_SUMY}, {"SUMX2", AB_SUMX2}, {"SUMY2", AB_SUMY2}, {"SUMXY", AB_SUMXY},
    {"CUBE", AB_CUBE}, {"CBRT", AB_CBRT}, {"ENGL", AB_ENGL}, {"ENGR", AB_ENGR}, {"EXPM1", AB_EXPM1}, {"LN1P", AB_LN1P}, {"TWOX", AB_TWOX}, {"LOG2", AB_LOG2},
    {"CLALL", AB_CLALL}, {"CLEQN", AB_CLEQN}, {"BEEP", AB_BEEP},
    {"ANS", AB_ANS}, {"HUP", AB_HUP}, {"HDOWN", AB_HDOWN},     /* unit 029 */
    {"SHOW", AB_SHOW},                                          /* unit 030 */
    {"TABLE", AB_TABLE}, {"TSTART", AB_TSTART}, {"TSTEP", AB_TSTEP}, {"TUP", AB_TUP}, {"TDOWN", AB_TDOWN},   /* 032 */
    {"GRAPH", AB_GRAPH}, {"XMIN", AB_XMIN}, {"XMAX", AB_XMAX}, {"YMIN", AB_YMIN}, {"YMAX", AB_YMAX},       /* 033 */
    {"AUTO", AB_GAUTO}, {"GLEFT", AB_GLEFT}, {"GRIGHT", AB_GRIGHT},
};

/* The last error any key returned in the current vector, for %ERR (unit 012). */
static abn_status last_err;
/* X as SAVEX found it, for %SAMEX (unit 014: an equation against the keyboard path). */
static ab_val saved_x;

/* Unit 016's harness switches: AB_SAVELOAD=1 saves and loads after every key (and checks that
   save, load, save gives the same bytes); AB_SLICE=n runs every program in slices of n lines. */
static int env_saveload;
static long env_slice;
static uint8_t *img;            /* the image SAVE made (grown as needed: a device has little RAM) */
static size_t img_len, img_cap;
static uint8_t *mark;           /* a second image, MARK's, for %SAMEMARK */
static size_t mark_len;

static uint8_t *himg;           /* the harness switches' own image, apart from SAVE's (abacus's
                                   016/017 review: sharing one let the switch overwrite SAVE's) */
static size_t himg_len, himg_cap;

static bool buf_save(const ab_calc *c, uint8_t **buf, size_t *len, size_t *cap)
{
    size_t n = ab_state_save(c, NULL, 0);
    if (n == 0) return false;
    if (n > *cap) {
        uint8_t *b = realloc(*buf, n);
        if (!b) return false;
        *buf = b;
        *cap = n;
    }
    *len = ab_state_save(c, *buf, *cap);
    return *len == n;
}

static bool img_save(const ab_calc *c)
{
    return buf_save(c, &img, &img_len, &img_cap);
}

/* Unit 018: with a store, the harness's images go through its two slots (store.h) and must come
   back byte for byte, and what is loaded is what came back: on the device its flash partition
   (device_main sets vectors_flash), on the host with AB_STORE=1 a flash in memory. */
const ab_flash *vectors_flash;
static uint8_t *simg;           /* an image as the store gave it back */
static size_t simg_cap;

static const uint8_t *through_store(const uint8_t *b, size_t n)
{
    if (!vectors_flash) return b;
    if (n > simg_cap) {
        uint8_t *g = realloc(simg, n);
        if (!g) return NULL;
        simg = g;
        simg_cap = n;
    }
    if (ab_store_save(vectors_flash, b, (uint32_t)n) != 0) return NULL;
    uint32_t got = ab_store_load(vectors_flash, simg, (uint32_t)simg_cap);
    return got == n && memcmp(simg, b, n) == 0 ? simg : NULL;
}

#ifndef AB_DEVICE
/* the host's flash for AB_STORE: two 64 KB slots of NOR (an erase sets 0xFF, a write clears bits) */
static uint8_t ram_flash[2 * 64 * 1024];

static int rf_erase(void *ctx, uint32_t off, uint32_t len)
{
    (void)ctx;
    if (off + len > sizeof ram_flash) return -1;
    memset(ram_flash + off, 0xFF, len);
    return 0;
}

static int rf_write(void *ctx, uint32_t off, const void *src, uint32_t len)
{
    (void)ctx;
    if (off + len > sizeof ram_flash) return -1;
    for (uint32_t i = 0; i < len; i++) ram_flash[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}

static int rf_read(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    (void)ctx;
    if (off + len > sizeof ram_flash) return -1;
    memcpy(dst, ram_flash + off, len);
    return 0;
}

static const ab_flash host_flash = {NULL, 4096, sizeof ram_flash / 2, rf_erase, rf_write, rf_read};
#endif

/* The harness's save and load: its own buffer, through the store when there is one. */
static bool harness_saveload(ab_calc *c)
{
    const uint8_t *back;
    uint16_t beeps = c->beeps;      /* the BEEP count is the device's to read, not state (unit 027) */
    bool ok = buf_save(c, &himg, &himg_len, &himg_cap) && (back = through_store(himg, himg_len)) != NULL &&
              ab_state_load(c, back, himg_len) == ABN_OK;
    c->beeps = beeps;
    return ok;
}

static ab_calc calc;            /* 106 KB: off the stack, and the only one on a device */

#ifdef AB_DEVICE
/* A file the vectors name, from the device's flash bundle (tests/device/main/device_main.c). */
bool device_file(const char *path, const uint8_t **data, size_t *len);
#endif

#ifdef AB_DEVICE
/* The runner's calculator, for the device test's other steps (a device has room for one). */
ab_calc *vectors_calc(void)
{
    return &calc;
}
#endif

static abn_status do1(ab_calc *c, ab_op op, int arg)
{
    abn_status st = ab_do_arg(c, op, arg);
    while (env_slice > 0 && c->stop == AB_STOP_SLICE) {
        /* the device's loop; with AB_SAVELOAD too, a save and load at every slice stop (a device
           that sleeps mid-run) */
        if (env_saveload && !harness_saveload(c)) return ABN_BAD_IMAGE;
        st = ab_continue(c);
    }
    if (st != ABN_OK) last_err = st;
    return st;
}

static const char *const STATUS_NAMES[] = {"OK", "DIV_ZERO", "RANGE", "BAD_TEXT", "DOMAIN", "INEXACT",
                                           "CLAMPED", "NONEXISTENT", "XEQ_OVERFLOW", "DUP_LABEL",
                                           "MEMORY_FULL", "NONPROGRAMMABLE", "BUDGET", "NO_ROOT",
                                           "SELECT_FN", "NESTED", "SYNTAX", "INVALID_VAR",
                                           "NO_SOLUTION", "MULT_SOLUTION", "INTERRUPTED",
                                           "BAD_IMAGE", "OVERFLOW", "UNDERFLOW", "RPN_ONLY", "STU_ONLY",
                                           "NO_EQN", "NO_VAR", "CAS_TOO_LONG", "CAS_NO_ROOM", "CAS_UNSUPPORTED",
                                           "CAS_COPY", "TOO_DEEP", "CAS_NOT_POLY", "CAS_NOT_NUMERIC",
                                           "CAS_NOT_INTEGRABLE", "CAS_EXPR_ONLY"};

/* Applies one key token. Returns 0, or -1 for an unknown token. */
static int apply_token(ab_calc *c, const char *tok, int *errors)
{
    for (size_t i = 0; i < sizeof NAMED / sizeof NAMED[0]; i++) {
        if (strcmp(tok, NAMED[i].name) == 0) {
            if (do1(c, NAMED[i].op, 0) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    static const struct { const char *prefix; ab_op op; } STORE[] = {
        {"STO:", AB_STO}, {"RCL:", AB_RCL}, {"STO+:", AB_STOADD}, {"STO-:", AB_STOSUB},
        {"STO*:", AB_STOMUL}, {"STO/:", AB_STODIV}, {"RCL+:", AB_RCLADD}, {"RCL-:", AB_RCLSUB},
        {"RCL*:", AB_RCLMUL}, {"RCL/:", AB_RCLDIV}, {"XVAR:", AB_XVAR}, {"VIEW:", AB_VIEW},
        {"INPUT:", AB_INPUT}, {"SOLVE:", AB_SOLVE}, {"INTEG:", AB_INTEG}, {"TVAR:", AB_TVAR}};
    for (size_t i = 0; i < sizeof STORE / sizeof STORE[0]; i++) {
        size_t n = strlen(STORE[i].prefix);
        if (strncmp(tok, STORE[i].prefix, n) == 0) {
            const char *v = tok + n;
            int code;
            if (v[0] >= 'A' && v[0] <= 'Z' && v[1] == '\0') code = AB_VAR_A + (v[0] - 'A');
            else if (strcmp(v, "(I)") == 0) code = AB_VAR_IND_I;
            else if (strcmp(v, "(J)") == 0) code = AB_VAR_IND_J;
            else if (strcmp(v, "i") == 0) code = AB_VAR_SI;             /* unit 025: the 33s's i */
            else if (strcmp(v, "(i)") == 0) code = AB_VAR_IND_I;        /* (i): the index in 33s mode */
            else return -1;
            if (do1(c, STORE[i].op, code) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    static const struct { const char *name; ab_op op; int arg; } FRACT[] = {
        {"FDISP", AB_FDISP, 0}, {"/C", AB_SETC, 0}, {"RCL/C", AB_RCLC, 0}};
    for (size_t i = 0; i < sizeof FRACT / sizeof FRACT[0]; i++) {
        if (strcmp(tok, FRACT[i].name) == 0) {
            if (do1(c, FRACT[i].op, FRACT[i].arg) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    /* unit 040: the open graph's tools: the zooms by their 50g names, GMOVE:n (a numpad digit, +10 for ten
       pixels), GTRACE, GMARK, ZFACT:n, and the function tools */
    {
        static const char *const Z[AB_Z_N] = {"ZIN", "ZOUT", "BOXZ", "CNTR", "ZAUTO", "HZIN", "HZOUT", "VZIN", "VZOUT",
                                              "ZLAST", "ZDFLT", "ZDECI", "ZINTG", "ZSQR", "ZTRIG"};
        static const char *const F[AB_F_N] = {"ROOT", "GSLOPE", "EXTR", "AREA", "F(X)", "TANL"};   /* GSLOPE: SLOPE is L.R.'s */
        for (int k = 0; k < AB_Z_N; k++)
            if (!strcmp(tok, Z[k])) {
                if (do1(c, AB_GZOOM, k) != ABN_OK) (*errors)++;
                return 0;
            }
        for (int k = 0; k < AB_F_N; k++)
            if (!strcmp(tok, F[k])) {
                if (do1(c, AB_GFCN, k) != ABN_OK) (*errors)++;
                return 0;
            }
        if (!strcmp(tok, "GTRACE") || !strcmp(tok, "GMARK")) {
            if (do1(c, tok[1] == 'T' ? AB_GTRACE : AB_GMARK, 0) != ABN_OK) (*errors)++;
            return 0;
        }
        if (!strncmp(tok, "GMOVE:", 6) || !strncmp(tok, "ZFACT:", 6)) {
            if (do1(c, tok[0] == 'G' ? AB_GMOVE : AB_GFACT, atoi(tok + 6)) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    /* unit 039: CMPLX:op, the 33s's CMPLX prefix then the operator (by the runner's own name for it) */
    if (strncmp(tok, "CMPLX:", 6) == 0) {
        static const char *const CXP[AB_CXP_N] = {"+", "-", "*", "/", "POW", "CHS", "INV", "LN", "EXP", "SIN", "COS", "TAN"};
        for (int k = 0; k < AB_CXP_N; k++) {
            if (strcmp(tok + 6, CXP[k]) == 0) {
                if (do1(c, AB_CMPLX, k) != ABN_OK) (*errors)++;
                return 0;
            }
        }
        return -1;
    }
    /* unit 011: program keys. LBL:A; XEQ:A or XEQ:A005; GTO:A or GTO:A005; GTO.:A005 (the
       pointer-only form); GTO.. (PRGM TOP); SF:n CF:n FS?:n; ISG:A DSE:(I); X<Y? X=0? etc. */
    {
        static const struct { const char *prefix; ab_op op; } TGT[] = {
            {"XEQ:", AB_XEQ}, {"GTO.:", AB_GTOPOS}, {"GTO:", AB_GTO}};
        for (size_t i = 0; i < sizeof TGT / sizeof TGT[0]; i++) {
            size_t n = strlen(TGT[i].prefix);
            if (strncmp(tok, TGT[i].prefix, n) == 0) {
                const char *v = tok + n;
                if (strcmp(v, "(I)") == 0 || strcmp(v, "(J)") == 0 || strcmp(v, "(i)") == 0) {
                    /* XEQ (I), GTO (I): the label the index names (unit 012); (i) in 33s mode */
                    if (do1(c, TGT[i].op, v[1] == 'J' ? AB_TGT_IND_J : AB_TGT_IND_I) != ABN_OK) (*errors)++;
                    return 0;
                }
                if (v[0] < 'A' || v[0] > 'Z') return -1;
                int line = v[1] ? atoi(v + 1) : 0;
                if (v[1] && (strlen(v + 1) != 3 || line < 1)) return -1;
                if (do1(c, TGT[i].op, (v[0] - 'A') * 1000 + line) != ABN_OK) (*errors)++;
                return 0;
            }
        }
        if (strncmp(tok, "REG:", 4) == 0 && tok[4] && strchr("XYZT", tok[4]) && !tok[5]) {
            /* in an equation: R-down's menu, the register chosen (unit 015) */
            if (do1(c, AB_KREG, (int)(strchr("XYZT", tok[4]) - "XYZT")) != ABN_OK) (*errors)++;
            return 0;
        }
        /* unit 016: the state image and the device's calls */
        if (strcmp(tok, "SAVE") == 0) {
            if (!img_save(c)) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "MARK") == 0) {
            size_t n = ab_state_save(c, NULL, 0);
            free(mark);
            mark = malloc(n ? n : 1);
            mark_len = mark ? ab_state_save(c, mark, n) : 0;
            return 0;
        }
        if (strcmp(tok, "LOAD") == 0) {
            abn_status st = ab_state_load(c, img, img_len);
            if (st != ABN_OK) {
                last_err = st;
                (*errors)++;
            }
            return 0;
        }
#ifdef AB_DEVICE
        if (strncmp(tok, "LOADFILE:", 9) == 0) {
            /* on a device the file is in the flash bundle (tests/device) */
            const uint8_t *data;
            size_t n;
            if (!device_file(tok + 9, &data, &n)) return -1;
            abn_status st = ab_state_load(c, data, n);
            if (st != ABN_OK) {
                last_err = st;
                (*errors)++;
            }
            return 0;
        }
#endif
        if (strncmp(tok, "SAVEFILE:", 9) == 0 || strncmp(tok, "LOADFILE:", 9) == 0) {
            FILE *f = fopen(tok + 9, tok[0] == 'S' ? "wb" : "rb");
            if (!f) return -1;
            if (tok[0] == 'S') {
                if (!img_save(c) || fwrite(img, 1, img_len, f) != img_len) (*errors)++;
            } else {
                long n;
                fseek(f, 0, SEEK_END);
                n = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (n <= 0 || (size_t)n > img_cap) {
                    uint8_t *b = realloc(img, (size_t)(n > 0 ? n : 1));
                    if (!b) return -1;
                    img = b;
                    img_cap = (size_t)(n > 0 ? n : 1);
                }
                img_len = fread(img, 1, (size_t)(n > 0 ? n : 0), f);
                abn_status st = ab_state_load(c, img, img_len);
                if (st != ABN_OK) {
                    last_err = st;
                    (*errors)++;
                }
            }
            fclose(f);
            return 0;
        }
        if (strncmp(tok, "CORRUPT:", 8) == 0) {    /* flip a bit of byte k (no image: nothing) */
            if (img_len) img[(size_t)atol(tok + 8) % img_len] ^= 0x01;
            return 0;
        }
        if (strncmp(tok, "TRUNC:", 6) == 0) {
            size_t n = (size_t)atol(tok + 6);
            if (n < img_len) img_len = n;
            return 0;
        }
        if (strncmp(tok, "IMGVER:", 7) == 0) {   /* another version, CRC redone */
            unsigned v = (unsigned)atoi(tok + 7);
            if (img_len < 6) return 0;
            img[4] = (uint8_t)v;
            img[5] = (uint8_t)(v >> 8);
            goto fixcrc;
        }
        if (strcmp(tok, "FIXCRC") == 0) {     /* the checksum made right again */
            if (img_len < 4) return 0;
        fixcrc: {
                uint32_t k = ab_crc32(img, img_len - 4);
                for (int i = 0; i < 4; i++) img[img_len - 4 + (size_t)i] = (uint8_t)(k >> (8 * i));
            }
            return 0;
        }
        if (strncmp(tok, "SETBYTE:", 8) == 0) {    /* SETBYTE:offset:value */
            char *colon = strchr(tok + 8, ':');
            if (!colon) return -1;
            if (img_len) img[(size_t)atol(tok + 8) % img_len] = (uint8_t)atoi(colon + 1);
            return 0;
        }
        if (strncmp(tok, "BUDGET:", 7) == 0) {  /* the device's budget of lines per run */
            c->budget = atol(tok + 7);
            return 0;
        }
        if (strncmp(tok, "SLICE:", 6) == 0) {
            c->slice = atol(tok + 6);
            return 0;
        }
        if (strcmp(tok, "CONT") == 0) {
            abn_status st = ab_continue(c);
            if (st != ABN_OK) {
                last_err = st;
                (*errors)++;
            }
            return 0;
        }
        if (strcmp(tok, "INTERRUPT") == 0) {
            ab_interrupt(c);
            return 0;
        }
        if (strncmp(tok, "INTR:", 5) == 0) {   /* an interrupt arriving after n lines (tests) */
            c->intr_after = atol(tok + 5);
            return 0;
        }
        if (strncmp(tok, "MODEL:", 6) == 0) {   /* the mode op with any argument (unit 017) */
            if (do1(c, AB_MODEL, atoi(tok + 6)) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "STU") == 0 || strcmp(tok, "ALG") == 0 || strcmp(tok, "RPN") == 0) {   /* unit 029 */
            abn_status st = tok[0] == 'S' ? do1(c, AB_MODEL, AB_MODEL_STU) : do1(c, AB_ENTRYM, tok[0] == 'A');
            if (st != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "MODE33") == 0 || strcmp(tok, "MODE35") == 0) {   /* unit 017 */
            if (do1(c, AB_MODEL, tok[5] == '3' ? 33 : 35) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "MEMCLR") == 0) {
            ab_memory_clear(c);
            return 0;
        }
        if (strcmp(tok, "SOLVE") == 0) {      /* SOLVE with no letter: a built-in solver shown */
            if (do1(c, AB_SOLVE, -1) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "RSALL") == 0) {       /* answer every equation prompt with R/S (runner) */
            while (c->eqn_pending && c->stop == AB_STOP_INPUT)
                if (do1(c, AB_RS, 0) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "SAVEX") == 0) {       /* remember X (a runner token, not a key) */
            saved_x = c->x;
            return 0;
        }
        if (strcmp(tok, "XEQ") == 0) {         /* in Equation mode: evaluate the equation shown */
            if (do1(c, AB_XEQ, AB_TGT_EQN) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strncmp(tok, "EQ:", 3) == 0) {     /* an equation's text, added to the list (unit 014) */
            abn_status st = ab_eqn_add(c, tok + 3);
            if (st != ABN_OK) {
                (*errors)++;
                last_err = st;
            }
            return 0;
        }
        if (strncmp(tok, "FN=:", 4) == 0) {
            const char *v = tok + 4;
            int a;
            if (strcmp(v, "(I)") == 0 || strcmp(v, "(i)") == 0) a = AB_TGT_IND_I;
            else if (strcmp(v, "(J)") == 0) a = AB_TGT_IND_J;
            else if (v[0] >= 'A' && v[0] <= 'Z' && !v[1]) a = v[0] - 'A';
            else return -1;
            if (do1(c, AB_FN, a) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "GTO..") == 0) {
            if (do1(c, AB_GTOPOS, AB_PC_TOP) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strncmp(tok, "LBL:", 4) == 0) {
            if (tok[4] < 'A' || tok[4] > 'Z' || tok[5]) return -1;
            if (do1(c, AB_LBL, tok[4] - 'A') != ABN_OK) (*errors)++;
            return 0;
        }
        static const struct { const char *prefix; ab_op op; } FLG[] = {
            {"SF:", AB_SF}, {"CF:", AB_CF}, {"FS?:", AB_FS}};
        for (size_t i = 0; i < sizeof FLG / sizeof FLG[0]; i++) {
            size_t n = strlen(FLG[i].prefix);
            if (strncmp(tok, FLG[i].prefix, n) == 0 && isdigit((unsigned char)tok[n])) {
                if (do1(c, FLG[i].op, atoi(tok + n)) != ABN_OK) (*errors)++;
                return 0;
            }
        }
        static const struct { const char *prefix; ab_op op; } LOOP[] = {{"ISG:", AB_ISG}, {"DSE:", AB_DSE}};
        for (size_t i = 0; i < 2; i++) {
            if (strncmp(tok, LOOP[i].prefix, 4) == 0) {
                const char *v = tok + 4;
                int code;
                if (v[0] >= 'A' && v[0] <= 'Z' && v[1] == '\0') code = AB_VAR_A + (v[0] - 'A');
                else if (strcmp(v, "(I)") == 0 || strcmp(v, "(i)") == 0) code = AB_VAR_IND_I;
                else if (strcmp(v, "(J)") == 0) code = AB_VAR_IND_J;
                else if (strcmp(v, "i") == 0) code = AB_VAR_SI;
                else return -1;
                if (do1(c, LOOP[i].op, code) != ABN_OK) (*errors)++;
                return 0;
            }
        }
        static const char *const REL[] = {"!=", "<=", "<", ">", ">=", "="};
        if (tok[0] == 'X' && strlen(tok) >= 4 && tok[strlen(tok) - 1] == '?') {
            char rel[4] = "";
            char other = tok[strlen(tok) - 2];
            size_t rl = strlen(tok) - 3;
            if (rl < 1 || rl > 2 || (other != 'Y' && other != '0')) return -1;
            memcpy(rel, tok + 1, rl);
            rel[rl] = '\0';
            for (int r = 0; r < 6; r++) {
                if (strcmp(rel, REL[r]) == 0) {
                    if (do1(c, AB_TEST, r + (other == '0' ? 6 : 0)) != ABN_OK) (*errors)++;
                    return 0;
                }
            }
            return -1;
        }
    }
    if (strncmp(tok, "SUF:", 4) == 0) {
        int b = tok[4] == 'b' ? 2 : tok[4] == 'o' ? 8 : tok[4] == 'd' ? 10 : tok[4] == 'h' ? 16 : 0;
        if (!b || tok[5]) return -1;
        if (do1(c, AB_SUF, b) != ABN_OK) (*errors)++;
        return 0;
    }
    if (strncmp(tok, "FRACFMT:", 8) == 0 && tok[8] >= '0' && tok[8] <= '9' && !tok[9]) {   /* unit 019 */
        if (do1(c, AB_FRACFMT, tok[8] - '0') != ABN_OK) (*errors)++;
        return 0;
    }
    if (strncmp(tok, "CLVAR:", 6) == 0) {      /* CLVARx with its three-digit address (unit 021) */
        const char *v = tok + 6;
        if (strlen(v) != 3 || !isdigit((unsigned char)v[0]) || !isdigit((unsigned char)v[1]) ||
            !isdigit((unsigned char)v[2]))
            return -1;
        if (do1(c, AB_CLVARX, atoi(v)) != ABN_OK) (*errors)++;
        return 0;
    }
    if (strcmp(tok, "CLVARS") == 0) {
        if (do1(c, AB_CLVARS, 0) != ABN_OK) (*errors)++;
        return 0;
    }
    {   /* CAS 004: CASD:v CASCOL:v CASSUB:v (a variable A-Z), CASSIMP, CASEXP */
        static const struct { const char *t; ab_op op; bool var; } CAS[] = {
            {"CASD:", AB_CASD, true}, {"CASCOL:", AB_CASCOL, true}, {"CASSUB:", AB_CASSUB, true},
            {"CASSOLVE:", AB_CASSOLVE, true}, {"CASINT:", AB_CASINT, true}, {"CASFACT:", AB_CASFACT, true},   /* 048 */
            {"CASSIMP", AB_CASSIMP, false}, {"CASEXP", AB_CASEXP, false}};
        for (size_t i = 0; i < sizeof CAS / sizeof CAS[0]; i++) {
            size_t n = strlen(CAS[i].t);
            if (strncmp(tok, CAS[i].t, n) != 0) continue;
            if (CAS[i].var ? !(tok[n] >= 'A' && tok[n] <= 'Z' && !tok[n + 1]) : tok[n] != 0) return -1;
            if (do1(c, CAS[i].op, CAS[i].var ? tok[n] - 'A' : 0) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    if (strncmp(tok, "CONST:", 6) == 0) {
        int k = ab_const_index(tok + 6);
        if (k < 0) return -1;
        if (do1(c, AB_CONST, k) != ABN_OK) (*errors)++;
        return 0;
    }
    static const struct { const char *prefix; ab_op op; } SETTING[] = {
        {"FIX", AB_SETFIX}, {"SCI", AB_SETSCI}, {"ENG", AB_SETENG}};
    for (size_t i = 0; i < sizeof SETTING / sizeof SETTING[0]; i++) {
        size_t n = strlen(SETTING[i].prefix);
        if (strncmp(tok, SETTING[i].prefix, n) == 0 && isdigit((unsigned char)tok[n])) {
            int digits = atoi(tok + n);
            if (digits > AB_FMT_N_MAX) return -1;
            if (do1(c, SETTING[i].op, digits) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    for (const char *p = tok; *p; p++)
        if (!isdigit((unsigned char)*p) && *p != '.') return -1;
    for (const char *p = tok; *p; p++) {
        ab_op op = (*p == '.') ? AB_KPOINT : (ab_op)(AB_K0 + (*p - '0'));
        if (do1(c, op, 0) != ABN_OK) (*errors)++;
    }
    return 0;
}

static const ab_val *reg(const ab_calc *c, char r)
{
    switch (r) {
    case 'X': return &c->x;
    case 'Y': return &c->y;
    case 'Z': return &c->z;
    case 'T': return &c->t;
    case 'L': return &c->lastx;
    case 'N': return &c->ans;           /* ANS (unit 029) */
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

/* Runs one vectors file from f (opened by the caller, who closes it); name labels the report.
 * Returns 0 when every vector passes, 1 on a failure, 2 on a malformed file. */
int vectors_run(const char *name, FILE *f)
{
    env_saveload = getenv("AB_SAVELOAD") != NULL;
    env_slice = getenv("AB_SLICE") ? atol(getenv("AB_SLICE")) : 0;
#ifndef AB_DEVICE
    if (getenv("AB_STORE") && !vectors_flash) vectors_flash = &host_flash;
#endif
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
            fprintf(stderr, "%s:%d: expected 4 fields\n", name, lineno);
            return 2;
        }
        char *id = trim(field[0]), *keys = trim(field[2]), *expect = trim(field[3]);

        ab_calc *cp = &calc;
#define c (*cp)
        ab_init(&c);
        c.slice = env_slice;
        int op_errors = 0;
        bool holdpse = false;
        last_err = ABN_OK;
        for (char *tok = strtok(keys, " \t"); tok; tok = strtok(NULL, " \t")) {
            if (strcmp(tok, "HOLDPSE") == 0) {
                holdpse = true;         /* see the PSE stops instead of resuming them */
                continue;
            }
            if (apply_token(&c, tok, &op_errors) != 0) {
                fprintf(stderr, "%s:%d: %s: unknown key token '%s'\n", name, lineno, id, tok);
                return 2;
            }
            /* a PSE stop resumes by itself after the device's pause (unit 012 rule 1): the runner
               plays the device */
            while (!holdpse && c.stop == AB_STOP_PSE)
                if (do1(&c, AB_RS, 0) != ABN_OK) op_errors++;
            if (env_saveload) {
                /* save, load, save: the same bytes, and the loaded calculator goes on (unit 016) */
                size_t n1;
                static uint8_t *b2;
                static size_t c2;
                uint16_t hbeeps = 0;
                const uint8_t *back = NULL;
                if (!buf_save(&c, &himg, &himg_len, &himg_cap)) {
                    fprintf(stderr, "%s:%d: %s: save failed after '%s'\n", name, lineno, id, tok);
                    op_errors += 1000;
                } else if ((back = through_store(himg, himg_len)) == NULL) {
                    fprintf(stderr, "%s:%d: %s: the store did not give the image back after '%s'\n", name, lineno, id, tok);
                    op_errors += 1000;
                } else if ((hbeeps = c.beeps, ab_state_load(&c, back, himg_len)) != ABN_OK) {
                    fprintf(stderr, "%s:%d: %s: load refused its own image after '%s'\n", name, lineno, id, tok);
                    op_errors += 1000;
                } else {
                    n1 = ab_state_save(&c, NULL, 0);
                    if (n1 > c2) {
                        free(b2);
                        b2 = malloc(n1);
                        c2 = b2 ? n1 : 0;
                    }
                    c.beeps = hbeeps;           /* the device's to read, not state (unit 027) */
                    if (!b2 || ab_state_save(&c, b2, c2) != himg_len || memcmp(b2, himg, himg_len) != 0) {
                        fprintf(stderr, "%s:%d: %s: save, load, save differ after '%s'\n", name, lineno, id, tok);
                        op_errors += 1000;
                    }
                }
            }
        }

        if (getenv("AB_IMGCRC") && buf_save(&c, &himg, &himg_len, &himg_cap))   /* the final state's identity */
            fprintf(stderr, "%s %08lX\n", id, (unsigned long)ab_crc32(himg, himg_len));
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
            if (ex[0] == '%') {
                /* program state (unit 011): %PC=A005 or TOP; %GO:A004=A003 (that line's target);
                   %LINES=n; %YES=0|1 (the last test); %Fn=0|1 (flag n) */
                char *eq = strchr(ex, '=');
                char got[AB_EQN_LEN + 8] = "?";   /* the longest text compared: an equation */
                if (!eq) {
                    fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                    return 2;
                }
                *eq = '\0';
                if (strcmp(ex, "%PC") == 0) {
                    ab_prog_addr(&c, c.pc, got);
                } else if (strncmp(ex, "%GO:", 4) == 0) {
                    int i = ab_prog_find(&c, ex + 4);
                    if (i >= 0 && (c.prog[i].op == AB_XEQ || c.prog[i].op == AB_GTO)) {
                        int a = c.prog[i].arg;
                        if (a / 1000 >= 26) snprintf(got, sizeof got, "NONE");
                        else snprintf(got, sizeof got, "%c%03d", 'A' + a / 1000, a % 1000);
                    }
                } else if (strcmp(ex, "%ERR") == 0) {
                    snprintf(got, sizeof got, "%s", (unsigned)last_err < sizeof STATUS_NAMES / sizeof STATUS_NAMES[0]
                                                         ? STATUS_NAMES[last_err] : "?");
                } else if (strcmp(ex, "%COVER") == 0) {
                    /* the uncertainty in Y covers the true error of X: |x - v| <= y (unit 013) */
                    abn v, d;
                    bool okc = abn_from_text(&v, eq + 1) == ABN_OK && c.x.kind == AB_REAL &&
                               abn_sub(&d, &c.x.re, &v) == ABN_OK;
                    d = abn_abs(&d);
                    snprintf(got, sizeof got, "%s", okc && abn_cmp(&d, &c.y.re) <= 0 ? eq + 1 : "not covered");
                } else if (strncmp(ex, "%EVALS<", 7) == 0) {
                    /* "%EVALS<=n" is written %EVALS<=n: the evaluations used, at most n */
                    snprintf(got, sizeof got, "%s", c.evals <= atol(eq + 1) ? eq + 1 : "more");
                } else if (strcmp(ex, "%XVAR") == 0) {
                    /* "%XVAR=A": variable A holds exactly what X holds (unit 013: SOLVE's root) */
                    int vi = eq[1] >= 'A' && eq[1] <= 'Z' ? eq[1] - 'A' : -1;
                    bool same = vi >= 0 && c.var[vi].kind == c.x.kind && abn_cmp(&c.var[vi].re, &c.x.re) == 0 &&
                                abn_cmp(&c.var[vi].im, &c.x.im) == 0;
                    snprintf(got, sizeof got, "%s", same ? eq + 1 : "different");
                } else if (strcmp(ex, "%STOP") == 0) {
                    ab_stop_text(&c, got);
                } else if (strcmp(ex, "%SAMEX") == 0) {
                    /* X is exactly what SAVEX remembered */
                    bool same = c.x.kind == saved_x.kind && abn_cmp(&c.x.re, &saved_x.re) == 0 &&
                                abn_cmp(&c.x.im, &saved_x.im) == 0;
                    snprintf(got, sizeof got, "%s", same ? eq + 1 : "different");
                } else if (strcmp(ex, "%IMAGE") == 0) {
                    /* the CRC-32 of the image SAVE made, in hex (unit 016) */
                    snprintf(got, sizeof got, "%08lX", img_len ? (unsigned long)ab_crc32(img, img_len) : 0ul);
                } else if (strcmp(ex, "%IMGLEN") == 0) {
                    snprintf(got, sizeof got, "%lu", (unsigned long)img_len);
                } else if (strcmp(ex, "%SAMEIMG") == 0) {
                    /* saving now gives the bytes SAVE gave */
                    size_t n = ab_state_save(&c, NULL, 0);
                    uint8_t *b = malloc(n ? n : 1);
                    bool same = b && ab_state_save(&c, b, n) == img_len && memcmp(b, img, img_len) == 0;
                    free(b);
                    snprintf(got, sizeof got, "%s", same ? eq + 1 : "different");
                } else if (strcmp(ex, "%SAMEMARK") == 0) {
                    /* saving now gives the bytes MARK gave */
                    size_t n = ab_state_save(&c, NULL, 0);
                    uint8_t *b = malloc(n ? n : 1);
                    bool same = b && mark && ab_state_save(&c, b, n) == mark_len && memcmp(b, mark, mark_len) == 0;
                    free(b);
                    snprintf(got, sizeof got, "%s", same ? eq + 1 : "different");
                } else if (strcmp(ex, "%STATUSMSG") == 0) {
                    /* the last error status's own text (unit 018), as %ERRMSG writes it */
                    snprintf(got, sizeof got, "%s", ab_status_message(last_err));
                    for (char *q = got; *q; q++)
                        if (*q == ' ') *q = '_';
                        else if (*q == '\n') *q = '/';
                } else if (strcmp(ex, "%ERRMSG") == 0) {
                    /* the message showing (unit 018), blanks written _ and its line break / */
                    const char *m = ab_error_message(&c);
                    snprintf(got, sizeof got, "%s", m ? m : "none");
                    for (char *q = got; *q; q++)
                        if (*q == ' ') *q = '_';
                        else if (*q == '\n') *q = '/';
                } else if (strcmp(ex, "%MSG") == 0) {
                    /* the equation line shown as a message (flag 10, unit 015); blanks written _ */
                    bool msg = c.stop == AB_STOP_MSG || (c.stop == AB_STOP_PSE && c.stop_var == -2);
                    snprintf(got, sizeof got, "%s", msg ? c.peqn[c.prog[c.stop_line].arg] : "none");
                    for (char *q = got; *q; q++)
                        if (*q == ' ') *q = '_';
                } else if (strcmp(ex, "%EQ") == 0) {
                    /* the equation shown (unit 014) */
                    /* a built-in linear solver as its LCD name, blanks written _ (unit 015) */
                    const char *e = c.eqn_cur >= 0 ? c.eqn[c.eqn_cur] : "TOP";
                    snprintf(got, sizeof got, "%s", e[0] == '\x01' ? e + 1 : e);
                    for (char *q = got; *q; q++)
                        if (*q == ' ') *q = '_';
                } else if (strcmp(ex, "%CASREP") == 0 || strcmp(ex, "%CASROW") == 0 || strcmp(ex, "%NOTE") == 0) {
                    /* unit 048: SOLVE's report while open, the row under its cursor, the note beside the equation
                       shown; none when there is none; blanks written _ */
                    const char *r = ab_cas_report(&c), *n = ab_cas_note(&c);
                    char row[AB_EQN_LEN];
                    if (ex[1] == 'N') snprintf(got, sizeof got, "%s", n ? n : "none");
                    else if (ex[4] == 'R' && ex[5] == 'E') snprintf(got, sizeof got, "%s", r ? r : "none");
                    else snprintf(got, sizeof got, "%s", ab_cas_row(&c, c.cas_row, row, sizeof row) ? row : "none");
                    for (char *q = got; *q; q++)
                        if (*q == ' ') *q = '_';
                } else if (strcmp(ex, "%XRAW") == 0) {
                    /* X's real part as the engine holds it, coefficient and exponent, so its cohort as well as its
                       value (unit 050: a rounding must keep the cohort, as 045 found) */
                    abn_to_text(&c.x.re, got, sizeof got);
                } else if (strcmp(ex, "%EQN") == 0) {
                    int n = 0;                          /* the built-in solvers apart (unit 015) */
                    for (int q = 0; q < c.eqn_count; q++) n += c.eqn[q][0] != '\x01';
                    snprintf(got, sizeof got, "%d", n);
                } else if (strcmp(ex, "%ERRPOS") == 0) {
                    snprintf(got, sizeof got, "%d", c.eqn_errpos);
                } else if (strcmp(ex, "%LINES") == 0) {
                    snprintf(got, sizeof got, "%d", c.prog_len);
                } else if (strcmp(ex, "%YES") == 0) {
                    snprintf(got, sizeof got, "%d", c.yes ? 1 : 0);
                } else if (strcmp(ex, "%LINE") == 0) {     /* ALG's expression line (unit 029) */
                    snprintf(got, sizeof got, "%.*s", AB_EQN_LEN - 1, c.ebuf);
                } else if (strcmp(ex, "%ALG") == 0) {
                    snprintf(got, sizeof got, "%d", c.alg ? 1 : 0);
                } else if (strcmp(ex, "%STU") == 0) {
                    snprintf(got, sizeof got, "%d", c.stu ? 1 : 0);
                } else if (strcmp(ex, "%HIST") == 0) {
                    snprintf(got, sizeof got, "%d", c.hist_n);
                } else if (strcmp(ex, "%HSEL") == 0) {     /* the history entry shown, - none */
                    const char *h = c.hist_at >= 0 && c.hist_at < c.hist_n ? c.hist[c.hist_at] : "-";
                    snprintf(got, sizeof got, "%.*s", AB_EQN_LEN - 1, h);
                } else if (strcmp(ex, "%SHOW") == 0) {     /* SHOW's text, parts joined by /, - none (unit 030) */
                    char sh[2][AB_SHOW_LEN];
                    int n = ab_show_text(&c, sh);
                    if (n == 0) snprintf(got, sizeof got, "-");
                    else if (n == 1) snprintf(got, sizeof got, "%s", sh[0]);
                    else snprintf(got, sizeof got, "%s/%s", sh[0], sh[1]);
                } else if (strcmp(ex, "%ENG") == 0) {      /* unit 041 C (B4): ENG→ / ←ENG's view, - none */
                    ab_fmt w = {AB_ENG, 0, AB_FMT_WIDE, '.', 0};
                    if (!c.eng_on || ab_format_eng_view(&c.x.re, c.eng_low, c.eng_e, &w, got, sizeof got) < 0)
                        snprintf(got, sizeof got, "-");
                } else if (strcmp(ex, "%VIEW") == 0) {     /* unit 035: the keyboard VIEW showing: its id, - none */
                    if (c.viewing) snprintf(got, sizeof got, "%d", c.view_id);
                    else snprintf(got, sizeof got, "-");
                } else if (strcmp(ex, "%GRAPH") == 0) {    /* unit 033: the graph model */
                    snprintf(got, sizeof got, "%d", c.gr_on ? 1 : 0);
                } else if (strcmp(ex, "%GCUR") == 0) {
                    snprintf(got, sizeof got, "%d", c.gr_cur);
                } else if (strcmp(ex, "%GROW") == 0) {     /* unit 040: the cursor's row, TRACE, the mark */
                    snprintf(got, sizeof got, "%d", c.gr_row);
                } else if (strcmp(ex, "%GTRACE") == 0) {
                    snprintf(got, sizeof got, "%d", c.gr_trace ? 1 : 0);
                } else if (strcmp(ex, "%GMARK") == 0) {
                    if (c.gr_mark) snprintf(got, sizeof got, "%d,%d", c.gr_mcol, c.gr_mrow);
                    else snprintf(got, sizeof got, "-");
                } else if (strcmp(ex, "%GRES#") == 0 || strcmp(ex, "%GRESB#") == 0) {
                    /* "%GRES#=v,t": the tool's result within t of v (TANL's b: %GRESB#) (unit 040) */
                    bool isb = ex[5] == 'B';
                    const char *vs = eq + 1, *cm = strchr(vs, ',');
                    char wv[64];
                    abn want, tol, d;
                    snprintf(wv, sizeof wv, "%.*s", cm ? (int)(cm - vs) : 0, vs);
                    const abn *val = isb ? &c.gr_res_b : &c.gr_res;
                    if (c.gr_res_kind != 0 && cm && abn_from_text(&want, wv) == ABN_OK && abn_from_text(&tol, cm + 1) == ABN_OK &&
                        abn_sub(&d, val, &want) == ABN_OK && (d = abn_abs(&d), abn_cmp(&d, &tol) <= 0))
                        snprintf(got, sizeof got, "%s", eq + 1);
                    else if (c.gr_res_kind == 0)
                        snprintf(got, sizeof got, "-");
                    else
                        abn_to_text(val, got, sizeof got);
                } else if (strcmp(ex, "%XWIN") == 0 || strcmp(ex, "%GCXY") == 0 || strcmp(ex, "%GRES") == 0) {
                    /* "lo,hi" by value: the x window; the cursor's x and y; the tool's result (and TANL's b) */
                    abn lo = abn_zero(), hi = abn_zero(), want_lo, want_hi;
                    char a[64], b[64], wl[64];
                    bool two = true;
                    if (ex[1] == 'X') lo = c.gr_xmin, hi = c.gr_xmax;
                    else if (ex[2] == 'C') ab_graph_cursor(&c, &lo, &hi);
                    else if (c.gr_res_kind == 0) two = false, snprintf(a, sizeof a, "-");
                    else lo = c.gr_res, hi = c.gr_res_b, two = c.gr_res_kind == AB_F_TANL + 1;
                    const char *sp = strchr(eq + 1, ',');
                    snprintf(wl, sizeof wl, "%.*s", sp ? (int)(sp - eq - 1) : (int)strlen(eq + 1), eq + 1);
                    if (ex[2] == 'R' && c.gr_res_kind == 0) {
                        snprintf(got, sizeof got, "-");
                    } else if (abn_from_text(&want_lo, wl) == ABN_OK && abn_cmp(&want_lo, &lo) == 0 &&
                               (two ? sp && abn_from_text(&want_hi, sp + 1) == ABN_OK && abn_cmp(&want_hi, &hi) == 0 : !sp)) {
                        snprintf(got, sizeof got, "%s", eq + 1);     /* equal by value */
                    } else {
                        abn_to_text(&lo, a, sizeof a);
                        abn_to_text(&hi, b, sizeof b);
                        if (two) snprintf(got, sizeof got, "%s,%s", a, b);
                        else snprintf(got, sizeof got, "%s", a);
                    }
                } else if (strcmp(ex, "%GWIN") == 0) {
                    abn lo, hi, want_lo, want_hi;
                    char a[64], b[64], wl[64];
                    ab_graph_window(&c, &lo, &hi);
                    const char *sp = strchr(eq + 1, ',');     /* "ymin,ymax" */
                    snprintf(wl, sizeof wl, "%.*s", sp ? (int)(sp - eq - 1) : 0, eq + 1);
                    if (sp && abn_from_text(&want_lo, wl) == ABN_OK && abn_from_text(&want_hi, sp + 1) == ABN_OK &&
                        abn_cmp(&want_lo, &lo) == 0 && abn_cmp(&want_hi, &hi) == 0)
                        snprintf(got, sizeof got, "%s", eq + 1);     /* equal by value */
                    else {
                        abn_to_text(&lo, a, sizeof a);
                        abn_to_text(&hi, b, sizeof b);
                        snprintf(got, sizeof got, "%s,%s", a, b);
                    }
                } else if (!strncmp(ex, "%GX:", 4) || !strncmp(ex, "%GY:", 4) || !strncmp(ex, "%GR:", 4)) {
                    int col = atoi(ex + 4);
                    abn gx, want, lo, hi;
                    ab_val gy;
                    abn_status gs = ab_graph_sample(&c, col, &gx, &gy);
                    if (ex[2] == 'R') {
                        int rl, rh;
                        ab_graph_window(&c, &lo, &hi);
                        if (ab_graph_column(&c, col, &lo, &hi, &rl, &rh)) snprintf(got, sizeof got, "%d-%d", rl, rh);
                        else snprintf(got, sizeof got, "-");
                    } else {
                        abn *val = ex[2] == 'X' ? &gx : &gy.re;
                        if (ex[2] == 'Y' && gs != ABN_OK)
                            snprintf(got, sizeof got, "%s", (unsigned)gs < sizeof STATUS_NAMES / sizeof STATUS_NAMES[0] ? STATUS_NAMES[gs] : "?");
                        else if (abn_from_text(&want, eq + 1) == ABN_OK && abn_cmp(&want, val) == 0)
                            snprintf(got, sizeof got, "%s", eq + 1);
                        else
                            abn_to_text(val, got, sizeof got);
                    }
                } else if (strcmp(ex, "%TABLE") == 0) {    /* unit 032: the table open, its row, x, y */
                    snprintf(got, sizeof got, "%d", c.tbl_on ? 1 : 0);
                } else if (strcmp(ex, "%TK") == 0) {
                    snprintf(got, sizeof got, "%ld", (long)c.tbl_k);
                } else if (strcmp(ex, "%TX") == 0 || strcmp(ex, "%TY") == 0) {
                    abn tx;
                    ab_val ty;
                    abn_status ts = ab_table_row(&c, c.tbl_k, &tx, &ty);
                    abn want, *val = strcmp(ex, "%TX") == 0 ? &tx : &ty.re;
                    if (val == &ty.re && ts != ABN_OK)  /* an erring row: its status's name */
                        snprintf(got, sizeof got, "%s", (unsigned)ts < sizeof STATUS_NAMES / sizeof STATUS_NAMES[0] ? STATUS_NAMES[ts] : "?");
                    else if (abn_from_text(&want, eq + 1) == ABN_OK && abn_cmp(&want, val) == 0)
                        snprintf(got, sizeof got, "%s", eq + 1);     /* equal by value, as X= compares */
                    else
                        abn_to_text(val, got, sizeof got);
                } else if (strcmp(ex, "%BEEP") == 0) {     /* BEEPs done (unit 027) */
                    snprintf(got, sizeof got, "%u", (unsigned)c.beeps);
                } else if (ex[1] == 'F' && isdigit((unsigned char)ex[2])) {
                    snprintf(got, sizeof got, "%d", ab_flag(&c, atoi(ex + 2)) ? 1 : 0);
                } else {
                    fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                    return 2;
                }
                checks++;
                if (strcmp(got, eq + 1) == 0) {
                    checks_ok++;
                } else {
                    vec_ok = 0;
                    printf("FAIL %s: expected %s=%s, got %s\n", id, ex, eq + 1, got);
                }
                continue;
            }
            const ab_val *rv = reg(&c, ex[0]);
            const abn *r = rv ? &rv->re : NULL;
            ab_val var_val;
            char orig[96];
            snprintf(orig, sizeof orig, "%.*s", (int)sizeof orig - 1, ex);
            if (ex[0] == '@') {
                /* @A=v: variable A; @#n=v: unnamed indirect variable n (unit 005); any of the
                   comparisons = ~ ^ # after the name (unit 013) */
                char *eq = ex[1] == '#' ? strchr(ex, '=') : strpbrk(ex + 1, "=~^#");
                if (eq && ((ex[1] >= 'A' && ex[1] <= 'Z') || ex[1] == 'i') && eq == ex + 2) {
                    var_val = c.var[ex[1] == 'i' ? 26 : ex[1] - 'A'];         /* @i: unit 025 */
                    rv = &var_val;
                    r = &var_val.re;
                } else if (eq && ex[1] == '#') {
                    int a = atoi(ex + 2);
                    r = (a >= 0 && a <= AB_IND_MAX) ? &c.ind[a] : NULL;
                    rv = NULL;
                }
                if (r) {
                    memmove(ex + 1, eq, strlen(eq) + 1);    /* now "@=v" */
                }
            }
            if (r && ex[1] == '#') {
                /* "X#v,t": |x - v| <= t (unit 013: results judged against a tolerance) */
                char vs[128], *comma = strchr(ex + 2, ',');
                char gt[ABN_TEXT_MAX];
                abn want, tol, d, got;
                if (!comma || (size_t)(comma - ex - 2) >= sizeof vs) {
                    fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                    return 2;
                }
                memcpy(vs, ex + 2, (size_t)(comma - ex - 2));
                vs[comma - ex - 2] = '\0';
                bool okt = rv && rv->kind == AB_REAL && abn_from_text(&want, vs) == ABN_OK &&
                           abn_from_text(&tol, comma + 1) == ABN_OK;
                if (okt) {
                    got = *r;
                    okt = abn_sub(&d, &got, &want) == ABN_OK;
                    d = abn_abs(&d);
                    okt = okt && abn_cmp(&d, &tol) <= 0;
                }
                checks++;
                if (okt) {
                    checks_ok++;
                } else {
                    vec_ok = 0;
                    abn_to_text(r, gt, sizeof gt);
                    printf("FAIL %s: expected %s, got %s\n", id, orig, gt);
                }
                continue;
            }
            if (!r || (ex[1] != '=' && ex[1] != '~' && ex[1] != '^')) {
                fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                return 2;
            }
            /* "[a,b,c]" is a vector expectation (unit 010): the register must hold a vector of that
               length; "=" compares each element exactly, "^" each within 8 units of its own 34th
               digit. */
            if (ex[2] == '[') {
                char buf[256], *el[4];
                int n = 0, okv = 1;
                size_t bl = strlen(ex + 3);
                snprintf(buf, sizeof buf, "%.*s", (int)sizeof buf - 1, ex + 3);
                if (bl == 0 || buf[bl - 1] != ']') {
                    fprintf(stderr, "%s:%d: %s: bad vector '%s'\n", name, lineno, id, ex + 2);
                    return 2;
                }
                buf[bl - 1] = '\0';
                for (char *q = strtok(buf, ","); q && n < 4; q = strtok(NULL, ",")) el[n++] = q;
                if (n < 1 || n > 3) {
                    fprintf(stderr, "%s:%d: %s: bad vector '%s'\n", name, lineno, id, ex + 2);
                    return 2;
                }
                okv = rv && rv->kind == AB_VECTOR && rv->len == n;
                for (int j = 0; okv && j < n; j++) {
                    const abn *g = j == 0 ? &rv->re : j == 1 ? &rv->im : &rv->p3;
                    char gt[ABN_TEXT_MAX];
                    norm w, gv;
                    if (normalize(el[j], &w) != 0) {
                        fprintf(stderr, "%s:%d: %s: bad element '%s'\n", name, lineno, id, el[j]);
                        return 2;
                    }
                    abn_to_text(g, gt, sizeof gt);
                    okv = normalize(gt, &gv) == 0 &&
                          (ex[1] == '^' ? norm_within_units(&w, &gv, 8) : norm_equal(&w, &gv));
                }
                checks++;
                if (okv) {
                    checks_ok++;
                } else {
                    char g[3][ABN_TEXT_MAX] = {"?", "?", "?"};
                    if (rv) {
                        abn_to_text(&rv->re, g[0], sizeof g[0]);
                        abn_to_text(&rv->im, g[1], sizeof g[1]);
                        abn_to_text(&rv->p3, g[2], sizeof g[2]);
                    }
                    vec_ok = 0;
                    printf("FAIL %s: expected %s, got %s (len %d) %s %s %s\n", id, orig,
                           !rv ? "?" : rv->kind == AB_VECTOR ? "vector" : rv->kind == AB_COMPLEX ? "complex" : "real",
                           rv ? rv->len : 0, g[0], g[1], g[2]);
                }
                continue;
            }
            /* "a i b" is a complex expectation (unit 009): the register must hold a complex number;
               "=" compares each part exactly, "^" each part within 8 units of the expected
               value's leading digit (normwise). A real expectation needs a real register. */
            char *ipos = strchr(ex + 2, 'i');
            int ok;
            if (ipos) {
                char re_s[128], im_s[128];
                size_t lr = (size_t)(ipos - (ex + 2));
                if (lr >= sizeof re_s) lr = sizeof re_s - 1;
                memcpy(re_s, ex + 2, lr);
                re_s[lr] = '\0';
                snprintf(im_s, sizeof im_s, "%.*s", (int)sizeof im_s - 1, ipos + 1);
                norm wr, wi, gr, gi;
                char t_re[ABN_TEXT_MAX], t_im[ABN_TEXT_MAX];
                if (normalize(re_s, &wr) != 0 || normalize(im_s, &wi) != 0) {
                    fprintf(stderr, "%s:%d: %s: bad complex '%s'\n", name, lineno, id, ex + 2);
                    return 2;
                }
                ok = rv && rv->kind == AB_COMPLEX;
                if (ok) {
                    abn_to_text(&rv->re, t_re, sizeof t_re);
                    abn_to_text(&rv->im, t_im, sizeof t_im);
                    ok = normalize(t_re, &gr) == 0 && normalize(t_im, &gi) == 0;
                }
                if (ok && ex[1] == '^') {
                    long lead = wr.zero ? (wi.zero ? 0 : wi.exp) : (wi.zero ? wr.exp : (wr.exp > wi.exp ? wr.exp : wi.exp));
                    ok = norm_within_unit_exp(&wr, &gr, 8, lead - 33) && norm_within_unit_exp(&wi, &gi, 8, lead - 33);
                } else if (ok) {
                    ok = norm_equal(&wr, &gr) && norm_equal(&wi, &gi);
                }
                checks++;
                if (ok) {
                    checks_ok++;
                } else {
                    vec_ok = 0;
                    char g1[ABN_TEXT_MAX] = "?", g2[ABN_TEXT_MAX] = "?";
                    if (rv) {
                        abn_to_text(&rv->re, g1, sizeof g1);
                        abn_to_text(&rv->im, g2, sizeof g2);
                    }
                    printf("FAIL %s: expected %s, got %s %si%s\n", id, orig,
                           rv && rv->kind == AB_COMPLEX ? "complex" : "real", g1, g2);
                }
                continue;
            }
            if (rv && rv->kind != AB_REAL) {
                checks++;
                vec_ok = 0;
                printf("FAIL %s: expected real %s, got a %s\n", id, orig,
                       rv->kind == AB_VECTOR ? "vector" : "complex number");
                continue;
            }
            char got_text[ABN_TEXT_MAX];
            abn_to_text(r, got_text, sizeof got_text);
            norm want, got;
            if (normalize(ex + 2, &want) != 0) {
                fprintf(stderr, "%s:%d: %s: bad number '%s'\n", name, lineno, id, ex + 2);
                return 2;
            }
            ok = normalize(got_text, &got) == 0;
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
                printf("FAIL %s: expected %s, got %s\n", id, orig, got_text);
            }
        }
        vectors++;
        if (vec_ok) vectors_ok++;
        if (getenv("AB_EVALS")) fprintf(stderr, "%s evals %ld\n", id, c.evals);   /* unit 013 report */
    }
#undef c
    printf("%s: %d/%d vectors, %d/%d expectations\n", name, vectors_ok, vectors,
           checks_ok, checks);
    if (getenv("AB_ARENA_PEAK")) fprintf(stderr, "arena peak %d\n", ar_peak());   /* unit 010b measurement */
    return (vectors_ok == vectors && checks_ok == checks) ? 0 : 1;
}

#ifndef AB_DEVICE
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
    int r = vectors_run(argv[1], f);
    fclose(f);
    return r;
}
#endif
