/* load_fuzz: ab_state_load on damaged and random images (unit 016 rule 3).
 *
 *   build/load_fuzz [iterations] [seed]
 *
 * Seeds: the golden image (tests/golden-v1.img) and images of states built here (unit 029: one in
 * STU's ALG; unit 031: equations of each model; unit 082: tests/golden-v21-082.img, expressions and names in the
 * stack, the letters and the message's X). Each case damages
 * a seed (flipped bits, truncation, inserted or deleted bytes, or wholly random bytes), and half
 * the time repairs the length and checksum so the fields themselves are what is tested. A refused
 * image must leave the calculator exactly as it was (its image unchanged, byte for byte); an
 * accepted one must give save, load, save the same bytes, and then survive random keys (the run
 * is meant for ASan/UBSan). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "const.h"
#include "fmt.h"
#include "state.h"

static unsigned long long rng = 0x9E3779B97F4A7C15ull;
static unsigned long long next(void)
{
    rng ^= rng >> 12;
    rng ^= rng << 25;
    rng ^= rng >> 27;
    return rng * 2685821657736338717ull;
}
static unsigned below(unsigned n) { return (unsigned)(next() % n); }

static uint8_t *save(const ab_calc *c, size_t *n)
{
    *n = ab_state_save(c, NULL, 0);
    uint8_t *b = malloc(*n);
    if (b) ab_state_save(c, b, *n);
    return b;
}

/* A key a device could send: a valid argument for the operations that take one. */
static void random_key(ab_calc *c)
{
    ab_op op = (ab_op)below(AB_KREG + 1);
    int arg = 0;
    if (below(16) == 0) {               /* unit 029: the model (STU among them), the entry, the line's keys */
        static const ab_op L[] = {AB_MODEL, AB_ENTRYM, AB_ANS, AB_HUP, AB_HDOWN,
                                  AB_TABLE, AB_TVAR, AB_TSTART, AB_TSTEP, AB_TUP, AB_TDOWN,    /* (032) */
                                  AB_GRAPH, AB_XMIN, AB_XMAX, AB_YMIN, AB_YMAX, AB_GAUTO, AB_GLEFT, AB_GRIGHT,
                                  AB_CMPLX,                                                     /* (039) */
                                  AB_GZOOM, AB_GMOVE, AB_GTRACE, AB_GMARK, AB_GFACT, AB_GFCN,   /* (040) */
                                  AB_EQDROP, AB_EQSTORE};                                       /* (082) */
        op = L[below(sizeof L / sizeof L[0])];
        arg = op == AB_MODEL ? (int[]){33, 35, AB_MODEL_STU}[below(3)] : op == AB_ENTRYM ? (int)below(2) :
              op == AB_TVAR ? AB_VAR_A + (int)below(26) : op == AB_CMPLX ? (int)below(AB_CXP_N) :
              op == AB_GZOOM ? (int)below(AB_Z_N) : op == AB_GMOVE ? 1 + (int)below(19) :
              op == AB_GFACT ? (int)below(102) : op == AB_GFCN ? (int)below(AB_F_N) : 0;
    }
    switch (op) {
    case AB_LBL: case AB_FN: arg = (int)below(26); break;
    case AB_XEQ: case AB_GTO: case AB_GTOPOS: arg = (int)below(26) * 1000 + (int)below(4); break;
    case AB_STO: case AB_RCL: case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV: case AB_XVAR: case AB_VIEW:
    case AB_INPUT: case AB_ISG: case AB_DSE: case AB_SOLVE: case AB_INTEG: arg = (int)below(28); break;
    case AB_SF: case AB_CF: case AB_FS: case AB_TEST: arg = (int)below(12); break;
    case AB_SETFIX: case AB_SETSCI: case AB_SETENG: arg = (int)below(AB_FMT_N_MAX + 1); break;
    case AB_CONST: arg = (int)below((unsigned)ab_const_count()); break;
    case AB_SUF: arg = (int[]){2, 8, 10, 16}[below(4)]; break;
    case AB_KREG: arg = (int)below(4); break;
    default: break;
    }
    c->budget = 2000;                   /* random programs may loop */
    ab_do_arg(c, op, arg);
    if (c->stop == AB_STOP_SLICE) ab_continue(c);
}

static uint8_t *file(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(*n);
    if (b && fread(b, 1, *n, f) != *n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    return b;
}

/* The loader's line rule against program entry (ab_line_ok): for every op with a valid argument, program
   entry records it as a line of its own exactly when the loader accepts that line. Digits and the other
   entry keys become a number line (AB_LIT), never a line of their own; LIT and EQLINE are lines the loader
   reads by their own rules. A line program entry records must also survive a save and load. Returns the number
   of disagreements. */
static int line_rule_sweep(void)
{
    static ab_calc c;
    int bad = 0;
    for (int op = 0; op <= AB_OP_LAST; op++) {
        int arg = 0;
        switch (op) {
        case AB_LBL: case AB_FN: arg = 2; break;
        case AB_XEQ: case AB_GTO: case AB_GTOPOS: arg = 1001; break;
        case AB_SF: case AB_CF: case AB_FS: case AB_TEST: arg = 3; break;
        case AB_SUF: arg = 10; break;
        case AB_MODEL: arg = 33; break;
        case AB_TVAR: arg = AB_VAR_A; break;
        default: break;
        }
        if (op == AB_LIT || op == AB_EQLINE) continue;
        if (op == AB_KEQ || op == AB_KPAREN || op == AB_KLEFT || op == AB_KRIGHT || op == AB_KSPACE ||
            op == AB_KREG || op == AB_EQTOP)
            continue;                   /* Equation mode's keys: no line now, but an older image's such line
                                           still loads (unit 018; calc.c's program_key) */
        /* recorded in 33s mode or in 35s mode: the loader takes both (a 35s feature is hidden from 33s mode's
           keys, unit 041 A, but a 35s label's line of it loads, and runs) */
        bool own = false;
        for (int m = 0; m < 2 && !own; m++) {
            ab_init(&c);
            if (m) ab_do_arg(&c, AB_MODEL, 35);
            ab_do(&c, AB_PRGM);
            ab_do_arg(&c, (ab_op)op, arg);
            own = c.prog_len == 1 && c.prog[0].op == op && c.prog[0].arg >= 0;
            if (op == AB_XEQ || op == AB_GTO) own = c.prog_len == 1 && c.prog[0].op == op;
        }
        if (own) {                      /* and the image carries it: saved, it loads (unit 039 found the gap) */
            static ab_calc d;
            size_t n;
            uint8_t *img = save(&c, &n);
            ab_init(&d);
            if (!img || ab_state_load(&d, img, n) != ABN_OK || d.prog_len != 1 || d.prog[0].op != op) {
                fprintf(stderr, "load_fuzz: op %d arg %d: program entry records a line its image cannot carry\n", op, arg);
                bad++;
            }
            free(img);
        }
        if (own != ab_line_ok((ab_op)op, arg)) {
            fprintf(stderr, "load_fuzz: op %d arg %d: program entry %s a line, the loader %s it\n", op, arg,
                    own ? "records" : "records no", ab_line_ok((ab_op)op, arg) ? "accepts" : "refuses");
            bad++;
        }
    }
    if (!ab_line_ok(AB_CMPLX, AB_CXP_N - 1) || ab_line_ok(AB_CMPLX, AB_CXP_N) || ab_line_ok(AB_CMPLX, -1)) {
        fprintf(stderr, "load_fuzz: CMPLX's operations are 0..%d as lines\n", AB_CXP_N - 1);   /* unit 039 */
        bad++;
    }
    return bad;
}

int main(int argc, char **argv)
{
    long iters = argc > 1 ? atol(argv[1]) : 20000;
    static ab_calc c;
    uint8_t *seeds[8];
    size_t seedn[8];
    long refused = 0, accepted = 0, bad = 0;
    if (line_rule_sweep()) return 1;
    if (argc > 2) rng = (strtoull(argv[2], NULL, 0) * 0x9E3779B97F4A7C15ull) | 1;

    /* seeds: the golden image; an empty calculator; a state from random keys */
    seeds[0] = file("tests/golden-v1.img", &seedn[0]);
    if (!seeds[0]) {
        fprintf(stderr, "load_fuzz: tests/golden-v1.img not found (run from the repository root)\n");
        return 2;
    }
    ab_init(&c);
    seeds[1] = save(&c, &seedn[1]);
    for (int i = 0; i < 300; i++) random_key(&c);
    seeds[2] = save(&c, &seedn[2]);
    ab_init(&c);                        /* unit 029: STU's ALG, a result shown, then a line typed */
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    ab_do_arg(&c, AB_ENTRYM, 1);
    {
        static const ab_op K[] = {AB_K2, AB_ADD, AB_K3, AB_ENTER, AB_MUL, AB_K4};
        for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) ab_do(&c, K[i]);
    }
    seeds[3] = save(&c, &seedn[3]);
    ab_init(&c);                        /* unit 031: equations of each model, listed and in a program */
    ab_eqn_add(&c, "2^3^2");
    ab_do_arg(&c, AB_EQN, 0);
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    ab_eqn_add(&c, "2A+B");
    ab_do_arg(&c, AB_EQN, 0);
    ab_do(&c, AB_PRGM);
    ab_do_arg(&c, AB_LBL, 0);
    ab_eqn_add(&c, "4^3^2");
    ab_do(&c, AB_PRGM);
    ab_do_arg(&c, AB_MODEL, 33);
    ab_do(&c, AB_PRGM);
    ab_do_arg(&c, AB_LBL, 1);
    ab_eqn_add(&c, "X^2");
    ab_do(&c, AB_PRGM);
    seeds[4] = save(&c, &seedn[4]);
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);      /* unit 032: a table open on a listed X^2+Y */
    ab_eqn_add(&c, "X^2+Y");
    ab_do_arg(&c, AB_TABLE, 0);
    ab_do(&c, AB_TDOWN);
    if (!c.tbl_on || c.prgm) {
        fprintf(stderr, "load_fuzz: the table seed did not open its table\n");
        return 2;
    }
    seeds[5] = save(&c, &seedn[5]);
    ab_do(&c, AB_CANCEL);                       /* unit 033: the table left, a graph open, the trace moved */
    ab_do_arg(&c, AB_GRAPH, 0);
    ab_do(&c, AB_GRIGHT);
    if (!c.gr_on) {
        fprintf(stderr, "load_fuzz: the graph seed did not open its graph\n");
        return 2;
    }
    seeds[6] = save(&c, &seedn[6]);
    seeds[7] = file("tests/golden-v21-082.img", &seedn[7]);     /* unit 082: version 21's kinds */
    if (!seeds[7]) {
        fprintf(stderr, "load_fuzz: tests/golden-v21-082.img not found\n");
        return 2;
    }

    for (long it = 0; it < iters; it++) {
        int s = (int)below(8);
        size_t n = seedn[s], cap = n + 64;
        uint8_t *m = malloc(cap), *before, *after;
        size_t nb, na;
        memcpy(m, seeds[s], n);
        switch (below(5)) {
        case 0: for (unsigned k = 1 + below(8); k > 0; k--) m[below((unsigned)n)] ^= (uint8_t)(1u << below(8)); break;
        case 1: n = below((unsigned)n); break;
        case 2: {                                           /* insert bytes */
            unsigned at = below((unsigned)n + 1), k = 1 + below(16);
            memmove(m + at + k, m + at, n - at);
            for (unsigned i = 0; i < k; i++) m[at + i] = (uint8_t)next();
            n += k;
            break;
        }
        case 3: {                                           /* delete bytes */
            unsigned at = below((unsigned)n), k = 1 + below(16);
            if (at + k > n) k = (unsigned)(n - at);
            memmove(m + at, m + at + k, n - at - k);
            n -= k;
            break;
        }
        default: for (size_t i = 0; i < n; i++) m[i] = (uint8_t)next(); break;
        }
        if (below(2) && n >= 16) {
            /* repair the header's length and the checksum: the fields are on trial */
            m[8] = (uint8_t)n;
            m[9] = (uint8_t)(n >> 8);
            m[10] = (uint8_t)(n >> 16);
            m[11] = (uint8_t)(n >> 24);
            uint32_t k = ab_crc32(m, n - 4);
            for (int i = 0; i < 4; i++) m[n - 4 + (size_t)i] = (uint8_t)(k >> (8 * i));
        }
        /* the calculator before: a loaded seed and a few keys */
        if (ab_state_load(&c, seeds[s], seedn[s]) != ABN_OK) {
            fprintf(stderr, "load_fuzz: seed %d refused\n", s);
            return 1;
        }
        for (unsigned k = below(5); k > 0; k--) random_key(&c);
        before = save(&c, &nb);
        if (ab_state_load(&c, m, n) != ABN_OK) {
            refused++;
            after = save(&c, &na);
            if (na != nb || memcmp(after, before, nb) != 0) {
                fprintf(stderr, "load_fuzz: case %ld: a refused image changed the calculator\n", it);
                bad++;
            }
        } else {
            uint8_t *again;
            size_t ng;
            accepted++;
            after = save(&c, &na);
            if (ab_state_load(&c, after, na) != ABN_OK) {
                fprintf(stderr, "load_fuzz: case %ld: an accepted image's own save was refused\n", it);
                bad++;
            }
            again = save(&c, &ng);
            if (ng != na || memcmp(again, after, na) != 0) {
                fprintf(stderr, "load_fuzz: case %ld: save, load, save differ\n", it);
                bad++;
            }
            free(again);
            for (int k = 0; k < 40; k++) random_key(&c);   /* whatever was accepted must run */
        }
        free(m);
        free(before);
        free(after);
    }
    for (int i = 0; i < 8; i++) free(seeds[i]);
    printf("load_fuzz: %ld cases, %ld refused, %ld accepted, %ld violations\n", iters, refused, accepted, bad);
    return bad ? 1 : 0;
}
