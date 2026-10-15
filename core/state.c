/* state: the calculator's state as a byte image (see state.h). */
#include <stdio.h>
#include <string.h>

#include "const.h"
#include "eqn.h"
#include "fmt.h"
#include "state.h"

/* The operation codes of version 1: the position in this table is the code. Frozen: a later
 * firmware keeps this table (adding codes only at its end), so an image's program means the same
 * keys whatever the enum's order becomes. */
static const ab_op V1_OPS[] = {
    AB_K0, AB_K1, AB_K2, AB_K3, AB_K4, AB_K5, AB_K6, AB_K7, AB_K8, AB_K9, AB_KPOINT, AB_KE, AB_KCHS,
    AB_KBKSP, AB_ENTER, AB_RDN, AB_RUP, AB_XY, AB_LASTX, AB_CLX, AB_CLSTK, AB_ADD, AB_SUB, AB_MUL,
    AB_DIV, AB_EXP, AB_LN, AB_TENX, AB_LOG, AB_IDIV, AB_RMDR, AB_SQ, AB_SQRT, AB_INV, AB_POW,
    AB_XROOT, AB_PI, AB_DEG, AB_RAD, AB_GRAD, AB_SIN, AB_COS, AB_TAN, AB_ASIN, AB_ACOS, AB_ATAN,
    AB_SINH, AB_COSH, AB_TANH, AB_ASINH, AB_ACOSH, AB_ATANH, AB_PCT, AB_PCTCHG, AB_HMS, AB_HMSINV,
    AB_TODEG, AB_TORAD, AB_TOKG, AB_TOLB, AB_TOC, AB_TOF, AB_TOCM, AB_TOIN, AB_TOL, AB_TOGAL,
    AB_TOKM, AB_TOMILE, AB_FACT, AB_NCR, AB_NPR, AB_RAND, AB_SEED, AB_IP, AB_FP, AB_INTG, AB_RND,
    AB_ABS, AB_SGN, AB_TOPOL, AB_TOREC, AB_CONST, AB_SETFIX, AB_SETSCI, AB_SETENG, AB_SETALL,
    AB_STO, AB_RCL, AB_STOADD, AB_STOSUB, AB_STOMUL, AB_STODIV, AB_RCLADD, AB_RCLSUB, AB_RCLMUL,
    AB_RCLDIV, AB_XVAR, AB_VIEW, AB_CLVARS, AB_SIGMAPLUS, AB_SIGMAMINUS, AB_CLSIGMA, AB_XBAR,
    AB_YBAR, AB_XWBAR, AB_SX, AB_SY, AB_PSX, AB_PSY, AB_XHAT, AB_YHAT, AB_CORR, AB_SLOPE, AB_YINT,
    AB_NSUM, AB_SUMX, AB_SUMY, AB_SUMX2, AB_SUMY2, AB_SUMXY, AB_FDISP, AB_SETC, AB_RCLC, AB_SF,
    AB_CF, AB_DEC, AB_HEX, AB_OCT, AB_BIN, AB_KA, AB_KB, AB_KC, AB_KD, AB_KE_HEX, AB_KF, AB_SUF,
    AB_AND, AB_XOR, AB_OR, AB_NOT, AB_NAND, AB_NOR, AB_KI, AB_KTHETA, AB_XIY, AB_RTA, AB_ARG,
    AB_KVEC, AB_KCOMMA, AB_PRGM, AB_LBL, AB_RTN, AB_XEQ, AB_GTO, AB_GTOPOS, AB_RS, AB_PSE, AB_SST,
    AB_BST, AB_CLPGM, AB_TEST, AB_FS, AB_ISG, AB_DSE, AB_LIT, AB_INPUT, AB_CANCEL, AB_FN, AB_SOLVE,
    AB_INTEG, AB_EQN, AB_KEQ, AB_KPAREN, AB_KLEFT, AB_KRIGHT, AB_EQLINE, AB_KSPACE, AB_KREG,
    /* unit 021 */
    AB_EXPM1, AB_LN1P, AB_TWOX, AB_LOG2, AB_CLVARX, AB_CLALL,
    /* unit 027 (appended: the codes before keep their meaning in an image) */
    AB_BEEP,
    /* unit 039 (appended) */
    AB_CMPLX,
    /* unit 041 C (appended) */
    AB_CUBE, AB_CBRT,
};
#define V1_N ((int)(sizeof V1_OPS / sizeof V1_OPS[0]))

/* The image version from which each status may be the message showing, in abn_status's order; 0 for one that never
   shows as a message (the brief notices). A status added to abn.h fails the assert below until it has its entry here
   (056: four were added without one, and an image saved while one showed was refused). */
static const uint8_t STATUS_SINCE[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* ABN_OK .. ABN_BAD_IMAGE */
    1, 1,                       /* ABN_OVERFLOW, ABN_UNDERFLOW (unit 026) */
    0, 0, 0, 0,                 /* ABN_RPN_ONLY, ABN_STU_ONLY, ABN_NO_EQN, ABN_NO_VAR: brief notices */
    12, 12, 12, 12,             /* CAS 004: ABN_CAS_TOO_LONG .. ABN_CAS_COPY */
    14,                         /* unit 047: ABN_TOO_DEEP */
    15, 15, 15, 15,             /* unit 048: ABN_CAS_NOT_POLY .. ABN_CAS_EXPR_ONLY */
    16, 16, 16, 16,             /* unit 056: ABN_CAS_IMPROPER .. ABN_CAS_BOUND_REAL */
    18,                         /* CAS 015: ABN_CAS_UNDECIDED */
    19, 19, 19,                 /* unit 070: ABN_CAS_NO_CLOSED .. ABN_CAS_WHERE */
};
_Static_assert(sizeof STATUS_SINCE == ABN_STATUS_LAST + 1, "an image version for every status (STATUS_SINCE)");

static bool status_in_image(unsigned st, int version)
{
    return st <= ABN_STATUS_LAST && STATUS_SINCE[st] && version >= STATUS_SINCE[st];
}

enum { T_MODES = 1, T_STACK, T_VARS, T_STAT, T_IND, T_PROG, T_EQNS, T_ENTRY, T_EDITOR, T_RUN, T_SOLVE, T_MODE25, T_ALG,
       T_EQMODEL, T_TABLE, T_GRAPH, T_ZOOM, T_CAS, T_ENG, T_EQNOTE };

#define BUILTIN2 "\x01" "2*2 lin. solve"
#define BUILTIN3 "\x01" "3*3 lin. solve"

uint32_t ab_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/* ---- writing ---- */

typedef struct {
    uint8_t *buf;
    size_t cap, len;
} wr;

static void w8(wr *w, unsigned v)
{
    if (w->len < w->cap) w->buf[w->len] = (uint8_t)v;
    w->len++;
}
static void w16(wr *w, unsigned v) { w8(w, v & 0xFF); w8(w, (v >> 8) & 0xFF); }
static void w32(wr *w, uint32_t v) { w16(w, v & 0xFFFF); w16(w, v >> 16); }
static void wi32(wr *w, int32_t v) { w32(w, (uint32_t)v); }
static void w64(wr *w, uint64_t v) { w32(w, (uint32_t)v); w32(w, (uint32_t)(v >> 32)); }

static void wbytes(wr *w, const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) w8(w, (uint8_t)s[i]);
}

static void wtext(wr *w, const char *s)
{
    size_t n = strlen(s);
    w16(w, (unsigned)n);
    wbytes(w, s, n);
}

/* A number: sign, exponent, the 113-bit coefficient in four 32-bit limbs, from the engine's
   exact text form ([+-]digitsE[+-]exponent). */
static void wnum(wr *w, const abn *a)
{
    char t[ABN_TEXT_MAX];
    uint32_t l[4] = {0, 0, 0, 0};
    const char *p = t + 1;
    abn_to_text(a, t, sizeof t);
    for (; *p >= '0' && *p <= '9'; p++) {
        uint64_t carry = (uint64_t)(*p - '0');
        for (int i = 0; i < 4; i++) {
            uint64_t v = (uint64_t)l[i] * 10u + carry;
            l[i] = (uint32_t)v;
            carry = v >> 32;
        }
    }
    {
        long e = 0;
        bool en = false;
        if (*p == 'E') p++;
        if (*p == '+' || *p == '-') en = *p++ == '-';
        for (; *p >= '0' && *p <= '9'; p++) e = e * 10 + (*p - '0');
        w8(w, t[0] == '-');
        w16(w, (unsigned)(uint16_t)(int16_t)(en ? -e : e));
    }
    for (int i = 0; i < 4; i++) w32(w, l[i]);
}

static bool is_zero_bits(const abn *a)
{
    abn z = abn_zero();
    return memcmp(a, &z, sizeof z) == 0;
}

/* A value: kind, length, which parts are not the all-zero number, those parts. */
static void wval(wr *w, const ab_val *v)
{
    const abn *part[3] = {&v->re, &v->im, &v->p3};
    unsigned mask = 0;
    for (int i = 0; i < 3; i++)
        if (!is_zero_bits(part[i])) mask |= 1u << i;
    w8(w, (unsigned)v->kind);
    w8(w, (unsigned)v->len);
    w8(w, mask);
    for (int i = 0; i < 3; i++)
        if (mask >> i & 1) wnum(w, part[i]);
}

static void wentry(wr *w, const ab_entry *e)
{
    w8(w, e->active);
    w8(w, (unsigned)e->len);
    wbytes(w, e->text, (size_t)e->len);
    w8(w, e->mant_neg);
    w8(w, e->exp_neg);
    w8(w, (unsigned)e->back);               /* version 2 */
}

/* A section: its tag, a size patched in once the payload is written. */
static size_t sec_begin(wr *w, unsigned tag)
{
    size_t at;
    w16(w, tag);
    at = w->len;
    w32(w, 0);
    return at;
}

static void sec_end(wr *w, size_t at)
{
    uint32_t n = (uint32_t)(w->len - at - 4);
    for (int i = 0; i < 4; i++)
        if (at + (size_t)i < w->cap) w->buf[at + (size_t)i] = (uint8_t)(n >> (8 * i));
}

static int op_code(ab_op op)
{
    for (int i = 0; i < V1_N; i++)
        if (V1_OPS[i] == op) return i;
    return -1;
}

static int kind_code(int k) { return k == AB_SOLVE ? 1 : k == AB_INTEG ? 2 : 0; }

size_t ab_state_save(const ab_calc *c, uint8_t *buf, size_t cap)
{
    wr w = {buf, cap, 0};
    size_t s;
    if (c->running) return 0;
    wbytes(&w, "ABSI", 4);
    w16(&w, AB_STATE_VERSION);
    w16(&w, 0);
    w32(&w, 0);                                 /* the length, patched below */

    s = sec_begin(&w, T_MODES);
    w8(&w, (unsigned)c->angle);
    w8(&w, (unsigned)c->disp_mode);
    w8(&w, (unsigned)c->disp_n);
    w8(&w, (unsigned)c->base);
    w8(&w, c->fdisp);
    w16(&w, (unsigned)c->frac_c);
    w8(&w, c->flag8);
    w8(&w, c->flag9);
    w8(&w, c->cpolar);
    w16(&w, c->flags);
    w64(&w, c->rng);
    w8(&w, c->yes);
    w8(&w, (unsigned)(uint8_t)(int8_t)c->fn);
    w8(&w, c->lift);
    w8(&w, c->m35);                         /* version 2 */
    w8(&w, c->msg);
    {
        int code = op_code((ab_op)c->err_op);   /* version 3: what the message says */
        w16(&w, (unsigned)(code < 0 ? 0xFFFF : code));
        wi32(&w, c->err_arg);
        w8(&w, (unsigned)kind_code(c->err_active));
        w8(&w, (unsigned)c->err_status);
        wval(&w, &c->err_x);
    }
    w8(&w, c->viewing);                     /* version 4: a keyboard VIEW's display, which decides what
                                               C and backspace do next (abacus #3982) */
    wi32(&w, c->viewing ? c->view_id : 0);
    sec_end(&w, s);

    s = sec_begin(&w, T_STACK);
    wval(&w, &c->x);
    wval(&w, &c->y);
    wval(&w, &c->z);
    wval(&w, &c->t);
    wval(&w, &c->lastx);
    sec_end(&w, s);

    s = sec_begin(&w, T_VARS);
    {
        int n = 0;
        for (int i = 0; i < 26; i++)
            n += !(c->var[i].kind == AB_REAL && is_zero_bits(&c->var[i].re) && is_zero_bits(&c->var[i].im) &&
                   is_zero_bits(&c->var[i].p3) && c->var[i].len == 0);
        w8(&w, (unsigned)n);
        for (int i = 0; i < 26; i++) {
            const ab_val *v = &c->var[i];
            if (v->kind == AB_REAL && is_zero_bits(&v->re) && is_zero_bits(&v->im) && is_zero_bits(&v->p3) && v->len == 0)
                continue;
            w8(&w, (unsigned)i);
            wval(&w, v);
        }
    }
    sec_end(&w, s);

    s = sec_begin(&w, T_STAT);
    {
        unsigned mask = 0;
        for (int i = 0; i < AB_STAT_REGS; i++)
            if (!is_zero_bits(&c->stat[i])) mask |= 1u << i;
        w8(&w, mask);
        for (int i = 0; i < AB_STAT_REGS; i++)
            if (mask >> i & 1) wnum(&w, &c->stat[i]);
    }
    sec_end(&w, s);

    s = sec_begin(&w, T_IND);
    w16(&w, (unsigned)(uint16_t)(int16_t)c->ind_top);
    for (int i = 0; i <= c->ind_top; i++) wnum(&w, &c->ind[i]);
    sec_end(&w, s);

    s = sec_begin(&w, T_PROG);
    w16(&w, (unsigned)c->prog_len);
    for (int i = 0; i < c->prog_len; i++) {
        int op = c->prog[i].op;
        w16(&w, (unsigned)op_code((ab_op)op));
        if (op == AB_LIT) wval(&w, &c->lit[c->prog[i].arg]);
        else if (op == AB_EQLINE) wtext(&w, c->peqn[c->prog[i].arg]);
        else wi32(&w, c->prog[i].arg);
    }
    sec_end(&w, s);

    s = sec_begin(&w, T_EQNS);
    w8(&w, (unsigned)c->eqn_count);
    w8(&w, (unsigned)(uint8_t)(int8_t)c->eqn_cur);
    for (int i = 0; i < c->eqn_count; i++) wtext(&w, c->eqn[i]);
    sec_end(&w, s);

    s = sec_begin(&w, T_ENTRY);
    wentry(&w, &c->entry);
    wentry(&w, &c->entry1);
    w8(&w, (unsigned)c->cpart);
    wentry(&w, &c->ventry[0]);
    wentry(&w, &c->ventry[1]);
    w8(&w, (unsigned)c->vlen);
    w8(&w, c->prgm);
    wval(&w, &c->plit);
    sec_end(&w, s);

    s = sec_begin(&w, T_EDITOR);
    w8(&w, c->eqn_mode);
    w8(&w, c->eqn_typing && !c->cas_def && !c->cas_pop);  /* (057: QUOT's and GCD's g likewise) */   /* unit 056: a bound of ∫AB being typed isn't kept: a restart cancels the
                                               step, as the list was before it (#5630 rule 1) */
    w8(&w, (unsigned)(uint8_t)(int8_t)c->eqn_edit);
    wtext(&w, c->cas_def || c->cas_pop ? "" : c->ebuf);
    w16(&w, c->cas_def || c->cas_pop ? 0u : (unsigned)c->ecur);
    w16(&w, (unsigned)c->eqn_errpos);
    w8(&w, (unsigned)c->eqn_pending);
    w8(&w, (unsigned)c->eqn_nvars);
    for (int i = 0; i < c->eqn_nvars; i++) w8(&w, (unsigned)c->eqn_vars[i]);
    w8(&w, (unsigned)c->eqn_next);
    sec_end(&w, s);

    s = sec_begin(&w, T_RUN);
    w16(&w, (unsigned)(uint16_t)(int16_t)c->pc);
    w8(&w, (unsigned)c->rdepth);
    for (int i = 0; i < c->rdepth; i++) w16(&w, (unsigned)(uint16_t)(int16_t)c->rstack[i]);
    w8(&w, (unsigned)c->rbase);
    w8(&w, (unsigned)c->stop);
    wi32(&w, c->stop_var);
    w16(&w, (unsigned)c->stop_line);
    w32(&w, (uint32_t)c->evals);
    w16(&w, (unsigned)(uint16_t)(int16_t)c->peq_line);
    w8(&w, c->peq_ready);
    w32(&w, (uint32_t)(c->stop == AB_STOP_SLICE ? c->run_lines : 0));   /* used only to go on */
    sec_end(&w, s);

    s = sec_begin(&w, T_SOLVE);
    w8(&w, (unsigned)kind_code(c->active));
    wi32(&w, c->solve_var);
    w8(&w, (unsigned)c->solve_phase);
    w8(&w, (unsigned)(uint8_t)(int8_t)c->fn_eqn);
    w8(&w, c->ev_cached);
    wnum(&w, &c->ev_cx);
    wnum(&w, &c->ev_cf);
    w8(&w, (unsigned)c->ev_cst);
    wval(&w, &c->ev_y);
    wval(&w, &c->ev_z);
    wval(&w, &c->ev_t);
    w8(&w, (unsigned)kind_code(c->susp.kind));
    wi32(&w, c->susp.var);
    wi32(&w, c->susp.code);
    wval(&w, &c->susp.x);
    wval(&w, &c->susp.y);
    wval(&w, &c->susp.z);
    wval(&w, &c->susp.t);
    wval(&w, &c->susp.lastx);
    wval(&w, &c->susp.var0);
    w8(&w, c->susp.lift);
    w16(&w, (unsigned)(uint16_t)(int16_t)c->susp.pc);
    w8(&w, (unsigned)c->susp.rbase);
    w8(&w, (unsigned)c->susp.rdepth);
    w8(&w, c->susp.from_prog);
    sec_end(&w, s);

    s = sec_begin(&w, T_MODE25);                /* version 5 (unit 025): i, the labels' modes */
    wval(&w, &c->var[26]);
    for (int l = 0; l < 26; l++) w8(&w, c->lmode[l]);
    w8(&w, c->err_m35);
    sec_end(&w, s);

    s = sec_begin(&w, T_ALG);                   /* version 6 (unit 029): STU, ALG and ANS */
    w8(&w, c->stu);
    w8(&w, c->alg);
    w8(&w, c->alg_shown);
    wval(&w, &c->ans);
    sec_end(&w, s);

    s = sec_begin(&w, T_EQMODEL);               /* version 7 (unit 031): each equation's model, the list's
                                                   then the program's lines in order */
    w8(&w, (unsigned)c->eqn_count);
    for (int i = 0; i < c->eqn_count; i++) w8(&w, c->eqn_model[i]);
    {
        int n = 0;
        for (int i = 0; i < c->prog_len; i++) n += c->prog[i].op == AB_EQLINE;
        w8(&w, (unsigned)n);
        for (int i = 0; i < c->prog_len; i++)
            if (c->prog[i].op == AB_EQLINE) w8(&w, c->peqn_model[c->prog[i].arg]);
    }
    sec_end(&w, s);

    s = sec_begin(&w, T_TABLE);                 /* version 8 (unit 032): TABLE's settings and the open table */
    w8(&w, (unsigned)(uint8_t)(int8_t)c->tbl_var);
    w8(&w, (unsigned)(uint8_t)(int8_t)c->tbl_v);
    w8(&w, c->tbl_start_set);
    wnum(&w, &c->tbl_start);
    wnum(&w, &c->tbl_step);
    w8(&w, c->tbl_on);
    w8(&w, (unsigned)(uint8_t)(int8_t)c->tbl_eq);
    wi32(&w, c->tbl_k);
    sec_end(&w, s);

    s = sec_begin(&w, T_GRAPH);                 /* version 9 (unit 033): GRAPH's window and the open graph */
    wnum(&w, &c->gr_xmin);
    wnum(&w, &c->gr_xmax);
    wnum(&w, &c->gr_ymin);
    wnum(&w, &c->gr_ymax);
    w8(&w, c->gr_auto);
    w8(&w, c->gr_on);
    w8(&w, (unsigned)(uint8_t)(int8_t)c->gr_eq);
    w8(&w, (unsigned)(uint8_t)(int8_t)c->gr_v);
    w16(&w, (unsigned)c->gr_cur);
    sec_end(&w, s);

    s = sec_begin(&w, T_ZOOM);                  /* version 10 (unit 040): the plot screen's cursor, zooms, tool */
    w16(&w, (unsigned)c->gr_row);
    w8(&w, c->gr_trace);
    w8(&w, c->gr_mark);
    w16(&w, (unsigned)c->gr_mcol);
    w16(&w, (unsigned)c->gr_mrow);
    wnum(&w, &c->gr_hf);
    wnum(&w, &c->gr_vf);
    w8(&w, c->gr_has_last);
    w8(&w, c->gr_last_auto);
    wnum(&w, &c->gr_lxmin);
    wnum(&w, &c->gr_lxmax);
    wnum(&w, &c->gr_lymin);
    wnum(&w, &c->gr_lymax);
    w8(&w, (unsigned)c->gr_res_kind);
    wnum(&w, &c->gr_res);
    wnum(&w, &c->gr_res_b);
    sec_end(&w, s);

    s = sec_begin(&w, T_CAS);                   /* version 12 (CAS 004): SUBST's replacement typed, a refusal's name */
    w8(&w, c->cas_sub);
    wtext(&w, c->cas_bad);
    sec_end(&w, s);

    s = sec_begin(&w, T_ENG);                   /* version 13 (unit 041 C, B4): ENG→ / ←ENG's view of X */
    w8(&w, c->eng_on);
    w16(&w, c->eng_on ? (unsigned)(uint16_t)c->eng_e : 0);     /* a view gone keeps no exponent or place */
    w16(&w, c->eng_on ? (unsigned)(uint16_t)c->eng_low : 0);
    sec_end(&w, s);

    s = sec_begin(&w, T_EQNOTE);                /* version 17 (unit 057): each equation's label, the list's order */
    w8(&w, (unsigned)c->eqn_count);
    for (int i = 0; i < c->eqn_count; i++) w8(&w, c->eqn_note[i]);
    sec_end(&w, s);

    {
        size_t total = w.len + 4;
        for (int i = 0; i < 4; i++)
            if (8 + (size_t)i < w.cap) w.buf[8 + i] = (uint8_t)(total >> (8 * i));
        if (total <= cap) w32(&w, ab_crc32(buf, w.len));
        else w.len += 4;
        return total;
    }
}

size_t ab_state_label_bytes(const ab_calc *c, int l)
{
    if (l < 0 || l >= 26 || c->labelpos[l] < 0 || c->labelpos[l] >= c->prog_len) return 0;
    wr w = {NULL, 0, 0};                        /* counts only */
    for (int i = c->labelpos[l]; i < c->prog_len && (i == c->labelpos[l] || c->prog[i].op != AB_LBL); i++) {
        int op = c->prog[i].op;
        w16(&w, (unsigned)op_code((ab_op)op));
        if (op == AB_LIT) wval(&w, &c->lit[c->prog[i].arg]);
        else if (op == AB_EQLINE) wtext(&w, c->peqn[c->prog[i].arg]);
        else wi32(&w, c->prog[i].arg);
    }
    return w.len;
}

/* ---- reading ---- */

typedef struct {
    const uint8_t *p;
    size_t len, pos;
    bool bad;
    ab_calc *c;             /* NULL: check only */
    /* what later sections are checked against */
    int prog_len, eqn_count;
    bool builtin[AB_EQN_MAX + 2];
    unsigned version;       /* the image's: fields a version lacks keep their defaults */
    bool mode25;            /* its version-5 section was read (unit 025) */
    bool m35, prgm, emode, typing;  /* what the ALG section is checked against (unit 029) */
    int peq_count;                  /* the program's equation lines (unit 031's section) */
    bool stu, typing_num;           /* what the TABLE section is checked against (unit 032) */
    unsigned base;                  /* and the ENG view (041 C): the base, and whether X is real */
    bool xreal;
    bool entry_seen;                /* (rentry: the last entry read was active) */
    bool table_on;                  /* an open table: no open graph beside it (unit 033) */
    int edit;
} rd;

static unsigned r8(rd *r)
{
    if (r->pos >= r->len) {
        r->bad = true;
        return 0;
    }
    return r->p[r->pos++];
}
static unsigned r16(rd *r) { unsigned a = r8(r); return a | r8(r) << 8; }
static uint32_t r32(rd *r) { uint32_t a = r16(r); return a | (uint32_t)r16(r) << 16; }
static int ri16(rd *r) { return (int16_t)(uint16_t)r16(r); }
static int ri8(rd *r) { return (int8_t)(uint8_t)r8(r); }
static int32_t ri32(rd *r) { return (int32_t)r32(r); }
static uint64_t r64(rd *r) { uint64_t a = r32(r); return a | (uint64_t)r32(r) << 32; }

static void need_at(rd *r, bool ok, int line)
{
#ifdef STATE_DEBUG
    if (!ok && !r->bad) fprintf(stderr, "state: check at line %d failed (offset %lu)\n", line, (unsigned long)r->pos);
#else
    (void)line;
#endif
    if (!ok) r->bad = true;
}
#define need(r, ok) need_at(r, ok, __LINE__)

/* 10^34, the first coefficient past decimal128's 34 digits, in 32-bit limbs */
static const uint32_t P34[4] = {0x00000000u, 0x378D8E64u, 0xBEAD87C0u, 0x0001ED09u};

static void rnum(rd *r, abn *out)
{
    uint32_t l[4];
    unsigned sign = r8(r);
    int e = ri16(r);
    char digits[48], t[64];
    int nd = 0;
    abn v;
    for (int i = 0; i < 4; i++) l[i] = r32(r);
    if (r->bad) return;
    need(r, sign <= 1 && e >= -6176 && e <= 6111);
    {
        int k = 3;
        while (k >= 0 && l[k] == P34[k]) k--;
        need(r, k >= 0 && l[k] < P34[k]);    /* the coefficient is below 10^34 */
    }
    if (r->bad) return;
    /* the coefficient's decimal digits, by repeated division by 10 */
    do {
        uint64_t rem = 0;
        for (int i = 3; i >= 0; i--) {
            uint64_t cur = (rem << 32) | l[i];
            l[i] = (uint32_t)(cur / 10u);
            rem = cur % 10u;
        }
        digits[nd++] = (char)('0' + rem);
    } while (l[0] | l[1] | l[2] | l[3]);
    {
        int k = 0;
        t[k++] = sign ? '-' : '+';
        while (nd > 0) t[k++] = digits[--nd];
        k += sprintf(t + k, "E%+d", e);
        t[k] = '\0';
    }
    /* the engine must take it exactly, and give the same form back */
    {
        char back[ABN_TEXT_MAX];
        need(r, abn_from_text_exact(&v, t) == ABN_OK);
        if (r->bad) return;
        abn_to_text(&v, back, sizeof back);
        need(r, strcmp(back, t) == 0);
    }
    if (!r->bad && out) *out = v;
}

static void rval(rd *r, ab_val *out)
{
    unsigned kind = r8(r), len = r8(r), mask = r8(r);
    ab_val v;
    memset(&v, 0, sizeof v);
    v.re = v.im = v.p3 = abn_zero();
    need(r, kind <= AB_VECTOR && mask <= 7 && (kind == AB_VECTOR ? len >= 1 && len <= 3 : len == 0));
    if (r->bad) return;
    v.kind = (ab_kind)kind;
    v.len = (int)len;
    if (mask & 1) rnum(r, &v.re);
    if (mask & 2) rnum(r, &v.im);
    if (mask & 4) rnum(r, &v.p3);
    if (!r->bad && out) *out = v;
}

/* Text of at most max - 1 bytes: no NUL; no control byte but a built-in's leading \x01 when
   allow1. */
static void rtext(rd *r, char *out, size_t max, bool allow1)
{
    unsigned n = r16(r);
    char t[AB_EQN_LEN];
    need(r, n < max && n < sizeof t);
    for (unsigned i = 0; i < n && !r->bad; i++) {
        unsigned b = r8(r);
        need(r, b >= 0x20 || (b == 0x01 && i == 0 && allow1));
        t[i] = (char)b;
    }
    if (r->bad) return;
    t[n] = '\0';
    if (out) memcpy(out, t, n + 1);
}

static void rentry(rd *r, ab_entry *out)
{
    ab_entry e;
    memset(&e, 0, sizeof e);
    e.active = r8(r) != 0;
    r->entry_seen = e.active;
    e.len = (int)r8(r);
    need(r, e.len < AB_ENTRY_TEXT);
    for (int i = 0; i < e.len && !r->bad; i++) {
        unsigned b = r8(r);
        need(r, (b >= '0' && b <= '9') || b == '.' || b == 'E' || (b >= 'a' && b <= 'f'));   /* hex digits lower case */
        e.text[i] = (char)b;
    }
    e.mant_neg = r8(r) != 0;
    e.exp_neg = r8(r) != 0;
    if (r->version >= 2) {
        e.back = (int)r8(r);
        need(r, e.back <= e.len);
    }
    if (!r->bad && out) *out = e;
}

static bool flag_byte(unsigned v) { return v <= 1; }

/* A variable or register as a stop / solve id (see ab_calc.stop_var). */
static bool id_ok(int id, bool allow_none)
{
    return (id >= 0 && id <= 26) || (id <= -27 && id >= -32) || (id >= 1000 && id <= 1000 + AB_IND_MAX) ||
           (allow_none && (id == -1 || id == -2));
}

/* An operation program entry records as a line, and its argument (see program_key). */
bool ab_line_ok(ab_op op, int32_t arg)
{
    switch (op) {
    case AB_PRGM: case AB_KVEC: case AB_KCOMMA: case AB_GTOPOS: case AB_SST: case AB_BST: case AB_KBKSP:
    case AB_CLPGM: case AB_FDISP: case AB_CANCEL: case AB_EQN: case AB_SUF: case AB_KI: case AB_KTHETA:
    case AB_KPOINT: case AB_KE: case AB_KA: case AB_KB: case AB_KC: case AB_KD: case AB_KE_HEX: case AB_KF:
    case AB_LIT: case AB_EQLINE: case AB_CLALL:
    case AB_ENTRYM: case AB_ANS: case AB_HUP: case AB_HDOWN: case AB_SHOW:     /* never lines (029, 030) */
    case AB_TABLE: case AB_TVAR: case AB_TSTART: case AB_TSTEP: case AB_TUP: case AB_TDOWN:      /* (032) */
    case AB_GRAPH: case AB_XMIN: case AB_XMAX: case AB_YMIN: case AB_YMAX: case AB_GAUTO: case AB_GLEFT:
    case AB_GRIGHT:                                                                            /* (033) */
    case AB_GZOOM: case AB_GMOVE: case AB_GTRACE: case AB_GMARK: case AB_GFACT: case AB_GFCN:          /* (040) */
    case AB_CASD: case AB_CASSIMP: case AB_CASEXP: case AB_CASCOL: case AB_CASSUB:                    /* (CAS 004) */
    case AB_CASSOLVE: case AB_CASINT: case AB_CASFACT: case AB_CASDEF:                                /* (048, 056) */
    case AB_CASQUOT: case AB_CASGCD: case AB_CASCANCL:                                                /* (057) */
    case AB_CASSUM: case AB_KINF:                                                                     /* (070) */
    case AB_ENGL: case AB_ENGR:                                                                       /* (041 C) */
    case AB_FRACFMT: case AB_CLEQN:     /* nonprogrammable since 019 and 021 (load_fuzz's line sweep) */
        return false;
    case AB_CLVARX:
        return arg >= 0 && arg <= AB_IND_MAX;
    case AB_LBL: case AB_FN:
        return (arg >= 0 && arg < 26) || (op == AB_FN && (arg == AB_TGT_IND_I || arg == AB_TGT_IND_J));
    case AB_XEQ: case AB_GTO:
        return arg == AB_TGT_IND_I || arg == AB_TGT_IND_J || arg == 26 * 1000 ||   /* 26000: target deleted */
               (arg >= 0 && arg / 1000 < 26 && arg % 1000 >= 1 && arg % 1000 <= AB_LABEL_LINES);
    case AB_STO: case AB_RCL: case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV: case AB_XVAR: case AB_VIEW:
    case AB_INPUT: case AB_ISG: case AB_DSE: case AB_SOLVE: case AB_INTEG:
        return arg >= 0 && arg <= AB_VAR_SI;   /* A..Z, (I), (J), i (unit 025) */
    case AB_SF: case AB_CF: case AB_FS: case AB_TEST:
        return arg >= 0 && arg <= 11;
    case AB_SETFIX: case AB_SETSCI: case AB_SETENG:
        return arg >= 0 && arg <= AB_FMT_N_MAX;
    case AB_CONST:
        return arg >= 0 && arg < ab_const_count();
    case AB_KREG:
        return arg >= 0 && arg < 4;
    case AB_CMPLX:                      /* unit 039 */
        return arg >= 0 && arg < AB_CXP_N;
    default:
        return op <= AB_K9 ? false : arg == 0;
    }
}

static int kind_of(unsigned k);

static void sec_modes(rd *r)
{
    unsigned angle = r8(r), mode = r8(r), n = r8(r), base = r8(r), fdisp = r8(r), fc = r16(r),
             f8 = r8(r), f9 = r8(r), cpolar = r8(r), flags = r16(r);
    uint64_t rng = r64(r);
    unsigned yes = r8(r);
    int fn = ri8(r);
    unsigned lift = r8(r), m35 = 0, msg = 0, ecode = 0xFFFF, eactive = 0, estatus = 0;
    r->base = base;
    int32_t earg = 0;
    ab_val ex;
    memset(&ex, 0, sizeof ex);
    ex.re = ex.im = ex.p3 = abn_zero();
    if (r->version >= 2) {
        m35 = r8(r);
        msg = r8(r);
        need(r, flag_byte(m35) && flag_byte(msg));
    }
    if (r->version >= 3) {
        ecode = r16(r);
        earg = ri32(r);
        eactive = r8(r);
        estatus = r8(r);
        rval(r, &ex);
        need(r, (ecode == 0xFFFF || ecode < (unsigned)V1_N) && eactive <= 2 &&
                    status_in_image(estatus, r->version));   /* unit 026
                                                   added two; CAS 004 four; 047 one; 048 four */
    } else {
        msg = 0;                                /* an older image: no message to say */
    }
    unsigned viewing = 0;
    int32_t view_id = 0;
    if (r->version >= 4) {
        viewing = r8(r);
        view_id = ri32(r);
        need(r, flag_byte(viewing) &&
                    (viewing ? id_ok(view_id, false) || view_id == AB_VIEW_INTEG || view_id == AB_VIEW_TEST ||
                                   view_id == AB_VIEW_LIN2 ||
                                   view_id == AB_VIEW_LIN2 - 1 || (view_id <= AB_VIEW_LIN3 && view_id >= AB_VIEW_LIN3 - 2)
                             : view_id == 0));   /* (unit 035's views) */
    }
    need(r, angle <= AB_ANGLE_GRAD && mode <= AB_ALL && n <= AB_FMT_N_MAX && (mode != AB_ALL || n == 0) &&
                (base == 2 || base == 8 || base == 10 || base == 16) && flag_byte(fdisp) && fc >= 2 &&
                fc <= 4095 && flag_byte(f8) && flag_byte(f9) && flag_byte(cpolar) && (flags & ~0x0C7Fu) == 0 &&
                flag_byte(yes) && fn >= -1 && fn < 26 && flag_byte(lift));
    r->m35 = m35;
    if (r->bad || !r->c) return;
    r->c->angle = (ab_angle)angle;
    r->c->disp_mode = (ab_mode)mode;
    r->c->disp_n = (int)n;
    r->c->base = (int)base;
    r->c->fdisp = fdisp;
    r->c->frac_c = fc;
    r->c->flag8 = f8;
    r->c->flag9 = f9;
    r->c->cpolar = cpolar;
    r->c->flags = (uint16_t)flags;
    r->c->rng = rng;
    r->c->yes = yes;
    r->c->fn = fn;
    r->c->lift = lift;
    r->c->m35 = m35;
    r->c->msg = msg;
    r->c->err_op = ecode == 0xFFFF ? -1 : (int)V1_OPS[ecode];
    r->c->err_arg = earg;
    r->c->err_active = kind_of(eactive);
    r->c->err_status = (abn_status)estatus;
    r->c->err_x = ex;
    r->c->viewing = viewing;
    r->c->view_id = view_id;
}

static void sec_stack(rd *r)
{
    ab_calc *c = r->c;
    ab_val x;
    memset(&x, 0, sizeof x);
    rval(r, &x);
    if (c) c->x = x;
    r->xreal = x.kind == AB_REAL;
    rval(r, c ? &c->y : NULL);
    rval(r, c ? &c->z : NULL);
    rval(r, c ? &c->t : NULL);
    rval(r, c ? &c->lastx : NULL);
}

/* version 5 (unit 025): i, each label's mode (0, 33 or 35), the message's mode */
static void sec_mode25(rd *r)
{
    ab_val iv;
    uint8_t lm[26];
    rval(r, &iv);
    for (int l = 0; l < 26; l++) {
        lm[l] = (uint8_t)r8(r);
        need(r, lm[l] == 0 || lm[l] == AB_MODEL_STU || lm[l] == 33 || lm[l] == 35);
    }
    unsigned em = r8(r);
    need(r, flag_byte(em));
    if (r->bad || !r->c) return;
    r->mode25 = true;
    r->c->var[26] = iv;
    memcpy(r->c->lmode, lm, sizeof lm);
    r->c->err_m35 = em;
}

/* version 6 (unit 029): STU (a 35s), ALG (STU's only: the line typed, no program entry or Equation
   mode), the line showing its result, ANS */
static void sec_alg(rd *r)
{
    unsigned stu = r8(r), alg = r8(r), shown = r8(r);
    ab_val ans;
    rval(r, &ans);
    need(r, flag_byte(stu) && flag_byte(alg) && flag_byte(shown) && (!stu || r->m35) && (!shown || alg));
    need(r, !alg || (stu && r->typing && !r->emode && !r->prgm && r->edit == -1));
    r->stu = stu;
    if (r->bad || !r->c) return;
    r->c->stu = stu;
    r->c->alg = alg;
    r->c->alg_shown = shown;
    r->c->ans = ans;
}

static bool model_ok(unsigned m)
{
    return m == 33 || m == 35 || m == AB_MODEL_STU;
}

/* version 7 (unit 031): the model of each listed equation, then of each program equation line in order */
static void sec_eqmodel(rd *r)
{
    uint8_t lm[AB_EQN_MAX + 2], pm[AB_PEQN_MAX];
    unsigned n = r8(r);
    need(r, (int)n == r->eqn_count);
    for (unsigned i = 0; i < n && !r->bad; i++) {
        lm[i] = (uint8_t)r8(r);
        need(r, model_ok(lm[i]));
    }
    unsigned k = r8(r);
    need(r, (int)k == r->peq_count);
    for (unsigned i = 0; i < k && !r->bad; i++) {
        pm[i] = (uint8_t)r8(r);
        need(r, model_ok(pm[i]));
    }
    if (r->bad || !r->c) return;
    memcpy(r->c->eqn_model, lm, n);
    memcpy(r->c->peqn_model, pm, k);    /* the loaded lines take pool slots 0.. in order (sec_prog) */
}

/* version 17 (unit 057): each equation's label (AB_NOTE_*), as many as the list has; a built-in has none */
static void sec_eqnote(rd *r)
{
    uint8_t note[AB_EQN_MAX + 2];
    unsigned n = r8(r);
    need(r, (int)n == r->eqn_count);
    for (unsigned i = 0; i < n && !r->bad; i++) {
        note[i] = (uint8_t)r8(r);
        need(r, note[i] <= (r->version >= 19 ? AB_NOTE_LAST : AB_NOTE_NOTAT) && (!note[i] || !r->builtin[i]));
                                                /* (WHERE, a sum's condition, from version 19: unit 070) */
    }
    if (r->bad || !r->c) return;
    memcpy(r->c->eqn_note, note, n);
}

/* version 8 (unit 032): TABLE's variable (-1 the first), the open table's variable, START set, the start
   and step, open, its equation (a listed one, not a built-in), the selected row */
static void sec_table(rd *r)
{
    int var = ri8(r), v = ri8(r);
    unsigned set = r8(r), on;
    abn start, step;
    rnum(r, &start);
    rnum(r, &step);
    on = r8(r);
    int eq = ri8(r);
    int32_t k = ri32(r);
    need(r, var >= -1 && var < 26 && v >= -1 && v < 26 && flag_byte(set) && flag_byte(on) && eq >= -1);
    if (!r->bad && on)                          /* an open table: in STU, not in program entry, no number
                                                   being typed (drawing a row must change nothing), its row
                                                   with room for the three above it (review F4) */
        need(r, v >= 0 && eq >= 0 && eq < r->eqn_count && !r->builtin[eq] && r->stu && !r->prgm && !r->typing_num &&
                    k >= INT32_MIN + 3);
    r->table_on = on && !r->bad;
    if (r->bad || !r->c) return;
    r->c->tbl_var = var;
    r->c->tbl_v = v;
    r->c->tbl_start_set = set;
    r->c->tbl_start = start;
    r->c->tbl_step = step;
    r->c->tbl_on = on;
    r->c->tbl_eq = eq;
    r->c->tbl_k = k;
}

/* version 9 (unit 033): the window, the fit on, the open graph (its equation a listed one, its variable,
   the trace column), as an open table is checked */
static void sec_graph(rd *r)
{
    abn x0, x1, y0, y1;
    rnum(r, &x0);
    rnum(r, &x1);
    rnum(r, &y0);
    rnum(r, &y1);
    unsigned au = r8(r), on = r8(r);
    int eq = ri8(r), v = ri8(r), cur = (int)r16(r);
    need(r, flag_byte(au) && flag_byte(on) && eq >= -1 && v >= -1 && v < 26 && cur < AB_GRAPH_W);
    if (!r->bad && on)
        need(r, v >= 0 && eq >= 0 && eq < r->eqn_count && !r->builtin[eq] && r->stu && !r->prgm && !r->typing_num &&
                    !r->table_on);
    if (r->bad || !r->c) return;
    r->c->gr_xmin = x0, r->c->gr_xmax = x1, r->c->gr_ymin = y0, r->c->gr_ymax = y1;
    r->c->gr_auto = au;
    r->c->gr_on = on;
    r->c->gr_eq = eq;
    r->c->gr_v = v;
    r->c->gr_cur = cur;
}

/* version 12 (CAS 004): SUBST's replacement being typed (its variable + 1, 0 none: only while typing), and the name
   a refusal's message shows */
static void sec_cas(rd *r)
{
    unsigned sub = r8(r);
    char bad[16];
    rtext(r, bad, sizeof bad, false);
    /* pending only while a new text is typed in Equation mode (no edit, no program entry; so never in ALG, which
       has Equation mode off): fresh eyes F4 */
    need(r, sub <= 26 && (!sub || (r->typing && r->emode && !r->prgm && r->edit == -1)));
    if (r->bad || !r->c) return;
    r->c->cas_sub = (uint8_t)sub;
    memcpy(r->c->cas_bad, bad, sizeof bad);
}

/* version 13 (unit 041 C, B4): ENG→ / ←ENG's view: showing, its exponent (a multiple of three) and the place of its
   last digit; only at the keyboard, in decimal, on a real X (no program entry, no number keyed: the key completes it),
   and within the key's bounds (fresh eyes: a HEX view, an exponent past them) */
static void sec_eng(rd *r)
{
    unsigned on = r8(r);
    int e = (int16_t)r16(r), low = (int16_t)r16(r);
    need(r, on <= 1 && (!on || (e % 3 == 0 && e >= -AB_ENG_LIMIT && e <= AB_ENG_LIMIT && low >= -AB_ENG_LIMIT &&
                                low <= AB_ENG_LIMIT && !r->prgm && !r->typing_num && r->base == 10 && r->xreal)));
    need(r, on || (e == 0 && low == 0));
    if (r->bad || !r->c) return;
    r->c->eng_on = on;
    r->c->eng_e = (int16_t)e;
    r->c->eng_low = (int16_t)low;
}

static void sec_zoom(rd *r)
{
    abn hf, vf, l0, l1, l2, l3, res, rb, one = abn_from_int(1), hundred = abn_from_int(100);
    int row = (int)r16(r);
    unsigned tr = r8(r), mk = r8(r);
    int mcol = (int)r16(r), mrow = (int)r16(r);
    rnum(r, &hf);
    rnum(r, &vf);
    unsigned hl = r8(r), la = r8(r);
    rnum(r, &l0);
    rnum(r, &l1);
    rnum(r, &l2);
    rnum(r, &l3);
    unsigned rk = r8(r);
    rnum(r, &res);
    rnum(r, &rb);
    need(r, row < AB_GRAPH_H && flag_byte(tr) && flag_byte(mk) && mcol < AB_GRAPH_W && mrow < AB_GRAPH_H &&
                flag_byte(hl) && flag_byte(la) && rk <= AB_F_N);
    /* the factors as ZFACT sets them: integers 1 to 100 */
    need(r, !r->bad && abn_is_integer(&hf) && abn_is_integer(&vf) && abn_cmp(&hf, &one) >= 0 && abn_cmp(&vf, &one) >= 0 &&
                abn_cmp(&hf, &hundred) <= 0 && abn_cmp(&vf, &hundred) <= 0);
    if (r->bad || !r->c) return;
    r->c->gr_row = row, r->c->gr_trace = tr, r->c->gr_mark = mk, r->c->gr_mcol = mcol, r->c->gr_mrow = mrow;
    r->c->gr_hf = hf, r->c->gr_vf = vf, r->c->gr_has_last = hl, r->c->gr_last_auto = la;
    r->c->gr_lxmin = l0, r->c->gr_lxmax = l1, r->c->gr_lymin = l2, r->c->gr_lymax = l3;
    r->c->gr_res_kind = (int)rk, r->c->gr_res = res, r->c->gr_res_b = rb;
}

static void sec_vars(rd *r)
{
    unsigned n = r8(r);
    int last = -1;
    need(r, n <= 26);
    for (unsigned k = 0; k < n && !r->bad; k++) {
        int i = (int)r8(r);
        need(r, i > last && i < 26);
        last = i;
        if (!r->bad) rval(r, r->c ? &r->c->var[i] : NULL);
    }
}

static void sec_stat(rd *r)
{
    unsigned mask = r8(r);
    need(r, mask < (1u << AB_STAT_REGS));
    for (int i = 0; i < AB_STAT_REGS && !r->bad; i++)
        if (mask >> i & 1) rnum(r, r->c ? &r->c->stat[i] : NULL);
}

static void sec_ind(rd *r)
{
    int top = ri16(r);
    abn v = abn_zero();
    need(r, top >= -1 && top <= AB_IND_MAX);
    for (int i = 0; i <= top && !r->bad; i++) rnum(r, r->c ? &r->c->ind[i] : &v);
    if (r->bad) return;
    if (top >= 0) need(r, !abn_is_zero(r->c ? &r->c->ind[top] : &v));   /* the last one defined */
    if (!r->bad && r->c) r->c->ind_top = top;
}

static void sec_prog(rd *r)
{
    int n = (int)r16(r), lits = 0, eqs = 0, owner = -1, run = 0;
    bool seen[26] = {false};
    need(r, n <= AB_PROG_LINES);
    for (int i = 0; i < n && !r->bad; i++) {
        unsigned code = r16(r);
        ab_op op;
        need(r, code < (unsigned)V1_N);
        if (r->bad) return;
        op = V1_OPS[code];
        if (op == AB_LIT) {
            need(r, lits < AB_PROG_LITS);
            if (r->bad) return;
            rval(r, r->c ? &r->c->lit[lits] : NULL);
            if (r->c) {
                r->c->prog[i].op = AB_LIT;
                r->c->prog[i].arg = lits;
                r->c->lit_used[lits] = true;
            }
            lits++;
        } else if (op == AB_EQLINE) {
            need(r, eqs < AB_PEQN_MAX);
            if (r->bad) return;
            rtext(r, r->c ? r->c->peqn[eqs] : NULL, AB_EQN_LEN, false);
            if (r->c) {
                r->c->prog[i].op = AB_EQLINE;
                r->c->prog[i].arg = eqs;
                r->c->peqn_used[eqs] = true;
            }
            eqs++;
        } else {
            int32_t arg = ri32(r);
            need(r, ab_line_ok(op, arg));
            if (op == AB_LBL && !r->bad) {
                need(r, !seen[arg]);                /* one line per label (UG p.13-4) */
                seen[arg] = true;
                owner = arg;
                run = 0;
            }
            if (r->c) {
                r->c->prog[i].op = (uint16_t)op;
                r->c->prog[i].arg = arg;
            }
        }
        run++;
        need(r, run <= AB_LABEL_LINES || owner < 0);
        need(r, run <= AB_LABEL_LINES + 1);
    }
    if (r->bad) return;
    r->prog_len = n;
    r->peq_count = eqs;
    if (r->c) r->c->prog_len = n;
}

static void sec_eqns(rd *r)
{
    int n = (int)r8(r), cur = ri8(r), b2 = -1, b3 = -1;
    char t[AB_EQN_LEN];
    need(r, n >= 2 && n <= AB_EQN_MAX + 2 && cur >= -1 && cur < n);
    for (int i = 0; i < n && !r->bad; i++) {
        rtext(r, t, AB_EQN_LEN, true);
        if (r->bad) return;
        if (t[0] == '\x01') {
            if (strcmp(t, BUILTIN2) == 0 && b2 < 0) b2 = i;
            else if (strcmp(t, BUILTIN3) == 0 && b3 < 0) b3 = i;
            else need(r, false);
        } else {
            need(r, t[0] != '\0');
        }
        r->builtin[i] = t[0] == '\x01';
        if (r->c) strcpy(r->c->eqn[i], t);
    }
    need(r, b2 >= 0 && b3 == b2 + 1);           /* both built-ins, side by side (UG p.6-8) */
    if (r->bad) return;
    r->eqn_count = n;
    if (r->c) {
        r->c->eqn_count = n;
        r->c->eqn_cur = cur;
    }
}

static void sec_entry(rd *r)
{
    ab_calc *c = r->c;
    unsigned cpart, vlen, prgm;
    rentry(r, c ? &c->entry : NULL);
    r->typing_num = r->entry_seen;
    rentry(r, c ? &c->entry1 : NULL);
    cpart = r8(r);
    rentry(r, c ? &c->ventry[0] : NULL);
    rentry(r, c ? &c->ventry[1] : NULL);
    vlen = r8(r);
    prgm = r8(r);
    rval(r, c ? &c->plit : NULL);
    need(r, cpart <= 2 && vlen <= 3 && flag_byte(prgm));
    r->prgm = prgm;
    r->typing_num = r->typing_num || cpart || vlen;
    if (r->bad || !c) return;
    c->cpart = (int)cpart;
    c->vlen = (int)vlen;
    c->prgm = prgm;
}

static void sec_editor(rd *r)
{
    unsigned mode = r8(r), typing = r8(r);
    int edit = ri8(r), ecur, errpos, pending, nvars, next;
    char t[AB_EQN_LEN];
    int vars[27];
    rtext(r, t, AB_EQN_LEN, false);
    ecur = (int)r16(r);
    errpos = (int)r16(r);
    pending = (int)r8(r);
    nvars = (int)r8(r);
    need(r, nvars <= 27);
    for (int i = 0; i < nvars && !r->bad; i++) {
        vars[i] = (int)r8(r);
        need(r, vars[i] < 27);                  /* 26: i (unit 025) */
    }
    next = (int)r8(r);
    need(r, flag_byte(mode) && flag_byte(typing) && edit >= -1 && edit < r->eqn_count &&
                (edit < 0 || !typing || !r->builtin[edit]) && errpos <= AB_EQN_LEN && pending <= 6 && next <= nvars);
    if (r->bad) return;
    need(r, ecur <= (int)strlen(t) && ((unsigned char)t[ecur] & 0xC0) != 0x80);
    r->emode = mode;
    r->typing = typing;
    r->edit = edit;
    if (r->bad || !r->c) return;
    r->c->eqn_mode = mode;
    r->c->eqn_typing = typing;
    r->c->eqn_edit = edit;
    strcpy(r->c->ebuf, t);
    r->c->ecur = ecur;
    r->c->eqn_errpos = errpos;
    r->c->eqn_pending = pending;
    r->c->eqn_nvars = nvars;
    for (int i = 0; i < nvars; i++) r->c->eqn_vars[i] = vars[i];
    r->c->eqn_next = next;
}

static void sec_run(rd *r)
{
    int pc = ri16(r), rdepth = (int)r8(r), rs[AB_RTN_DEPTH], rbase, stop, stop_var, stop_line, peq_line;
    uint32_t evals, run_lines;
    unsigned peq_ready;
    need(r, rdepth <= AB_RTN_DEPTH);
    for (int i = 0; i < rdepth && !r->bad; i++) {
        rs[i] = ri16(r);
        need(r, rs[i] >= -1 && rs[i] <= r->prog_len);
    }
    rbase = (int)r8(r);
    stop = (int)r8(r);
    stop_var = ri32(r);
    stop_line = (int)r16(r);
    evals = r32(r);
    peq_line = ri16(r);
    peq_ready = r8(r);
    run_lines = r32(r);
    need(r, (pc == AB_PC_TOP || (pc >= 0 && pc <= r->prog_len)) && rbase <= rdepth && stop <= AB_STOP_SLICE &&
                id_ok(stop_var, true) && stop_line < (r->prog_len > 0 ? r->prog_len : 1) && evals <= 100000000u &&
                peq_line >= -1 && peq_line < r->prog_len && flag_byte(peq_ready) && run_lines <= 0x7FFFFFFFu);
    if (r->bad || !r->c) return;
    r->c->pc = pc;
    r->c->rdepth = rdepth;
    for (int i = 0; i < rdepth; i++) r->c->rstack[i] = rs[i];
    r->c->rbase = rbase;
    r->c->stop = (ab_stop)stop;
    r->c->stop_var = stop_var;
    r->c->stop_line = stop_line;
    r->c->evals = (long)evals;
    r->c->peq_line = peq_line;
    r->c->peq_ready = peq_ready;
    r->c->run_lines = (long)run_lines;
}

static int kind_of(unsigned k) { return k == 1 ? AB_SOLVE : k == 2 ? AB_INTEG : 0; }

static void sec_solve(rd *r)
{
    ab_calc *c = r->c;
    unsigned active = r8(r), phase, cached, cst, skind, slift, srbase, srdepth, sfrom;
    int32_t svar_ = ri32(r), svar, scode;
    int fn_eqn, spc;
    phase = r8(r);
    fn_eqn = ri8(r);
    cached = r8(r);
    rnum(r, c ? &c->ev_cx : NULL);
    rnum(r, c ? &c->ev_cf : NULL);
    cst = r8(r);
    rval(r, c ? &c->ev_y : NULL);
    rval(r, c ? &c->ev_z : NULL);
    rval(r, c ? &c->ev_t : NULL);
    skind = r8(r);
    svar = ri32(r);
    scode = ri32(r);
    rval(r, c ? &c->susp.x : NULL);
    rval(r, c ? &c->susp.y : NULL);
    rval(r, c ? &c->susp.z : NULL);
    rval(r, c ? &c->susp.t : NULL);
    rval(r, c ? &c->susp.lastx : NULL);
    rval(r, c ? &c->susp.var0 : NULL);
    slift = r8(r);
    spc = ri16(r);
    srbase = r8(r);
    srdepth = r8(r);
    sfrom = r8(r);
    need(r, active <= 2 && (svar_ == 0 || id_ok(svar_, false)) && phase <= 2 && fn_eqn >= -1 && fn_eqn < r->eqn_count &&
                (fn_eqn < 0 || !r->builtin[fn_eqn]) && flag_byte(cached) &&
                (cst == ABN_OK || cst == ABN_DIV_ZERO || cst == ABN_RANGE || cst == ABN_DOMAIN || cst == ABN_OVERFLOW ||
                 cst == ABN_UNDERFLOW) && skind <= 2 &&
                (svar == 0 || svar == -1 || id_ok(svar, false)) && scode >= -1 && scode <= AB_VAR_SI &&
                flag_byte(slift) && (spc == AB_PC_TOP || (spc >= 0 && spc <= r->prog_len)) &&
                srbase <= AB_RTN_DEPTH && srdepth < AB_RTN_DEPTH && flag_byte(sfrom));
    if (r->bad || !c) return;
    c->active = kind_of(active);
    c->solve_var = svar_;
    c->solve_phase = (int)phase;
    c->fn_eqn = fn_eqn;
    c->ev_cached = cached;
    c->ev_cst = (abn_status)cst;
    c->susp.kind = kind_of(skind);
    c->susp.var = svar;
    c->susp.code = scode;
    c->susp.lift = slift;
    c->susp.pc = spc;
    c->susp.rbase = (int)srbase;
    c->susp.rdepth = (int)srdepth;
    c->susp.from_prog = sfrom;
}

/* Header, length and checksum; then the sections in order, each at most once, each read to
   exactly its size. */
static bool parse(rd *r)
{
    unsigned last = 0;
    uint32_t total;
    if (r->len < 16 || memcmp(r->p, "ABSI", 4) != 0) return false;
    r->pos = 4;
    r->version = r16(r);
    if (r->version < 1 || r->version > AB_STATE_VERSION || r16(r) != 0) return false;  /* future: refused */
    total = r32(r);
    if (total != r->len) return false;
    if (ab_crc32(r->p, r->len - 4) != (uint32_t)(r->p[r->len - 4] | r->p[r->len - 3] << 8 |
                                                  r->p[r->len - 2] << 16 | (uint32_t)r->p[r->len - 1] << 24))
        return false;
    r->len -= 4;                                /* the checksum is not a section */
    r->prog_len = 0;
    r->eqn_count = 2;
    r->builtin[0] = r->builtin[1] = true;
    while (r->pos < r->len && !r->bad) {
        unsigned tag = r16(r);
        uint32_t size = r32(r);
        size_t end;
        if (r->bad || tag <= last || tag > T_EQNOTE || size > r->len - r->pos) return false;
        last = tag;
        end = r->pos + size;
        switch (tag) {
        case T_MODES: sec_modes(r); break;
        case T_STACK: sec_stack(r); break;
        case T_VARS: sec_vars(r); break;
        case T_STAT: sec_stat(r); break;
        case T_IND: sec_ind(r); break;
        case T_PROG: sec_prog(r); break;
        case T_EQNS: sec_eqns(r); break;
        case T_ENTRY: sec_entry(r); break;
        case T_EDITOR: sec_editor(r); break;
        case T_RUN: sec_run(r); break;
        case T_MODE25:
            if (r->version >= 5) sec_mode25(r);
            else r->pos = end;                  /* a version without it: skipped */
            break;
        case T_ALG:
            if (r->version >= 6) sec_alg(r);
            else r->pos = end;
            break;
        case T_EQMODEL:
            if (r->version >= 7) sec_eqmodel(r);
            else r->pos = end;
            break;
        case T_TABLE:
            if (r->version >= 8) sec_table(r);
            else r->pos = end;
            break;
        case T_GRAPH:
            if (r->version >= 9) sec_graph(r);
            else r->pos = end;
            break;
        case T_ZOOM:
            if (r->version >= 10) sec_zoom(r);
            else r->pos = end;
            break;
        case T_CAS:
            if (r->version >= 12) sec_cas(r);
            else r->pos = end;
            break;
        case T_ENG:
            if (r->version >= 13) sec_eng(r);
            else r->pos = end;
            break;
        case T_EQNOTE:
            if (r->version >= 17) sec_eqnote(r);
            else r->pos = end;                  /* an image before 057: no labels */
            break;
        default: sec_solve(r); break;
        }
        if (r->pos != end) return false;
    }
    return !r->bad;
}

/* Unit 041 D: an older image's 33-stamped equation, restamped 35 on load? Below version 11 (041 A's (a)): when the 33s
   parse refuses it and the 35s parse takes it. At 11 and 12 (D): also only when the 33s parse takes it in the 33s's
   spelling (the comma a colon, nCr Cn,r, nPr Pn,r, HMS→ →HR, RAND RANDOM), so the spelling was the reason. */
static bool restamp33(ab_calc *c, const char *t, int version)
{
    ab_val v;
    int pos;
    if (eqn_eval(c, t, 33, false, true, &v, &pos) != ABN_SYNTAX || eqn_eval(c, t, 35, false, true, &v, &pos) != ABN_OK)
        return false;
    if (version < 11) return true;
    static const char *const FROM[] = {"nCr", "nPr", "HMS\xe2\x86\x92", "RAND", ","};
    static const char *const TO[] = {"Cn,r", "Pn,r", "\xe2\x86\x92HR", "RANDOM", ":"};
    char u[AB_EQN_LEN];
    size_t o = 0;
    while (*t) {
        size_t k = 0, n = 0;
        for (; k < sizeof FROM / sizeof FROM[0]; k++)
            if (!strncmp(t, FROM[k], n = strlen(FROM[k]))) break;
        const char *w = k < sizeof FROM / sizeof FROM[0] ? TO[k] : NULL;
        size_t m = w ? strlen(w) : 1;
        if (o + m >= sizeof u) return false;
        if (w) {
            memcpy(u + o, w, m);
            t += n;
        } else {
            u[o] = *t++;
        }
        o += m;
    }
    u[o] = '\0';
    return eqn_eval(c, u, 33, false, true, &v, &pos) == ABN_OK;
}

abn_status ab_state_load(ab_calc *c, const uint8_t *buf, size_t len)
{
    rd r;
    memset(&r, 0, sizeof r);
    r.p = buf;
    r.len = len;
    if (!buf || !parse(&r)) return ABN_BAD_IMAGE;
    /* every byte has passed: from MEMORY CLEAR, the image's state (an interrupt the device or a
       test has pending is the device's, as its budget and slice are: kept) */
    long intr_after = c->intr_after, budget = c->budget;
    /* the expression line's history (and the entry it shows) is RAM, not state (unit 029 rule 5,
       abacus #4254): an image loaded over it (UNDO's) leaves it; a boot or MEMORY CLEAR has none */
    ab_memory_clear_state(c);                         /* (which resets the budget to the core's, #3971) */
    c->intr_after = intr_after;
    c->budget = budget;
    c->prog_len = 0;
    memset(&r, 0, sizeof r);
    r.p = buf;
    r.len = len;
    r.c = c;
    parse(&r);
    if (!c->alg || c->hist_at >= c->hist_n) c->hist_at = -1;
    /* an image from before unit 031: its equations stamped left to right (35), as every mode read them */
    for (int i = 0; i < c->eqn_count; i++)
        if (!c->eqn_model[i]) c->eqn_model[i] = 35;
    for (int i = 0; i < c->prog_len; i++)
        if (c->prog[i].op == AB_EQLINE && !c->peqn_model[c->prog[i].arg]) c->peqn_model[c->prog[i].arg] = 35;  /* an entry shown stays shown in ALG (one
                                                                  that exists: a calc not from ab_init) */
    if (r.version < 13) {
        /* an image from before unit 041 (version below 11, abacus #4799 ruling (a)): an equation stamped 33 that the
           33s parse now refuses (a 35s feature: REGX, AND, a vector, ...) but the 35s parse takes is restamped 35, so
           a line already written keeps working; its meaning is unchanged (both read left to right). Others keep 33.
           From before 041 D (11 or 12, abacus #4880): the same, but only where the 35s's spelling is the reason
           (the comma, nCr, nPr, HMS→, RAND): one 041 A already refused stays refused (fresh eyes). */
        for (int i = 0; i < c->eqn_count; i++)
            if (c->eqn_model[i] == 33 && c->eqn[i][0] != '\x01' && restamp33(c, c->eqn[i], r.version))
                c->eqn_model[i] = 35;
        for (int i = 0; i < c->prog_len; i++) {
            if (c->prog[i].op != AB_EQLINE) continue;
            int k = c->prog[i].arg;
            if (c->peqn_model[k] == 33 && restamp33(c, c->peqn[k], r.version)) c->peqn_model[k] = 35;
        }
    }
    if (!r.mode25) {
        /* an image from before unit 025: every label stamped 35s, since (I) followed the 35s model
           in either mode then (unit 025 rule 6 as amended, abacus b75640f); i at 0; a message
           showing reads in (I) terms, as it did */
        for (int l = 0; l < 26; l++) c->lmode[l] = 35;
        c->err_m35 = true;
    }
    ab_prog_relabel(c);
    return ABN_OK;
}
