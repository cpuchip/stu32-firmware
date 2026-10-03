/* fmt: a number as the text of one display line (FIX, SCI, ENG, ALL).
 *
 * The value arrives as a sign, its significant digits and the exponent of the leading
 * digit (abn_digits). Every rounding below starts again from those digits, never from a
 * shorter text, and rounds half away from zero (display rounding, ours, unit 002;
 * provisional until checked on a real calculator).
 *
 * Rules from the user's guide are cited "UG p.N"; rules this project defines because a
 * 34-digit value on a wider line does not fit the guide's 12-digit display are marked
 * "ours, unit 002".
 */
#include <string.h>

#include "fmt.h"
#include "frac.h"
#include "base.h"

#define MAXD (ABN_DIGITS + 2)
#define TEXT_CAP 256        /* longest text built; widths beyond it are capped */

/* A decimal: d[0] is the digit at 10^lead; n digits, no trailing zeros; n == 0 is zero. */
typedef struct {
    bool neg;
    char d[MAXD];
    int n;
    int lead;
} dec;

static bool is_zero(const dec *v)
{
    return v->n == 0;
}

/* The digit at 10^pos ('0' outside the stored digits). */
static char digit_at(const dec *v, int pos)
{
    int i = v->lead - pos;
    return (i >= 0 && i < v->n) ? v->d[i] : '0';
}

/* Rounds v to keep only the digits at 10^p and above, half away from zero. */
static dec round_at(const dec *v, int p)
{
    dec r = *v;
    int k = v->lead - p + 1;                /* digits kept */
    if (is_zero(v) || k >= v->n) return r;
    if (k < 0 || (k == 0 && v->d[0] < '5')) {
        r.n = 0;
        r.neg = false;
        r.lead = 0;
        return r;
    }
    if (k == 0) {                           /* rounds up to one unit at 10^p */
        r.d[0] = '1';
        r.n = 1;
        r.lead = p;
        return r;
    }
    r.n = k;
    if (v->d[k] >= '5') {
        int i = k - 1;
        while (i >= 0 && r.d[i] == '9') r.d[i--] = '0';
        if (i >= 0) {
            r.d[i]++;
        } else {                            /* all nines: a power of ten */
            r.d[0] = '1';
            r.n = 1;
            r.lead++;
        }
    }
    while (r.n > 0 && r.d[r.n - 1] == '0') r.n--;
    return r;
}

static dec round_sig(const dec *v, int sig)
{
    return round_at(v, v->lead - sig + 1);
}

/* A small text builder that refuses to grow past its cap. */
typedef struct {
    char s[TEXT_CAP + 1];
    int len, cap;
    bool over;
} text;

static void put(text *t, char c)
{
    if (t->len >= t->cap) {
        t->over = true;
        return;
    }
    t->s[t->len++] = c;
    t->s[t->len] = '\0';
}

static void put_exp(text *t, int e)
{
    char buf[12];
    int n = 0;
    unsigned u = e < 0 ? (unsigned)(-(long)e) : (unsigned)e;
    put(t, 'E');
    if (e < 0) put(t, '-');
    do {
        buf[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n) put(t, buf[--n]);
}

static void start(text *t, int width)
{
    t->len = 0;
    t->s[0] = '\0';
    t->cap = width < TEXT_CAP ? width : TEXT_CAP;
    t->over = false;
}

/* Plain notation with exactly q decimals: [-]integer[radix fraction]. The thousands
 * separator groups the integer part in threes; it is ',' with radix '.' and '.' with
 * radix ',' (UG p.1-24). Returns false if it does not fit. */
static bool plain(text *t, const dec *v, int q, const ab_fmt *f)
{
    int top = (!is_zero(v) && v->lead > 0) ? v->lead : 0;
    int int_len = top + 1;
    int seps = f->separator ? (int_len - 1) / 3 : 0;
    long need = (v->neg ? 1 : 0) + int_len + seps + (q > 0 ? 1 + q : 0);
    if (need > t->cap) return false;
    char sep = f->radix == ',' ? '.' : ',';
    if (v->neg) put(t, '-');
    for (int pos = top; pos >= 0; pos--) {
        put(t, digit_at(v, pos));
        if (f->separator && pos > 0 && pos % 3 == 0) put(t, sep);
    }
    if (q > 0) {
        put(t, f->radix);
        for (int pos = -1; pos >= -q; pos--) put(t, digit_at(v, pos));
    }
    return !t->over;
}

/* A mantissa of m integer digits and fr fraction digits read from v starting at 10^top,
 * then the exponent e. No separators in a scientific mantissa (ours, unit 002). */
static bool scientific(text *t, const dec *v, int top, int m, int fr, int e, const ab_fmt *f)
{
    if (v->neg) put(t, '-');
    for (int i = 0; i < m; i++) put(t, digit_at(v, top - i));
    if (fr > 0) {
        put(t, f->radix);
        for (int i = 0; i < fr; i++) put(t, digit_at(v, top - m - i));
    }
    put_exp(t, e);
    return !t->over;
}

/* SCI n: one digit before the radix, n after, then the exponent; rounding to n + 1
 * significant digits may carry into the exponent (UG p.1-22). If it is too wide, digits
 * after the radix give way one at a time, re-rounded from the value; the exponent is never
 * cut off (ours, unit 002). */
static bool fmt_sci(text *t, const dec *v, int n, const ab_fmt *f)
{
    for (int k = n; k >= 0; k--) {
        dec r = round_sig(v, k + 1);
        start(t, f->width);
        if (scientific(t, &r, r.lead, 1, k, is_zero(&r) ? 0 : r.lead, f)) return true;
    }
    return false;
}

static int floor_div3(int x)
{
    return x >= 0 ? x / 3 : -((-x + 2) / 3);
}

/* ENG n: like SCI but the exponent is a multiple of three, so one to three digits come
 * before the radix and n digits follow the first significant digit (UG p.1-22, p.1-23).
 * Zero is 0, the radix and n zeros, then E0 (UG p.1-23). */
static bool fmt_eng(text *t, const dec *v, int n, const ab_fmt *f)
{
    for (int k = n; k >= 0; k--) {
        dec r = round_sig(v, k + 1);
        int e = is_zero(&r) ? 0 : floor_div3(r.lead) * 3;
        int m = is_zero(&r) ? 1 : r.lead - e + 1;
        int fr = k + 1 - m;
        start(t, f->width);
        if (scientific(t, &r, is_zero(&r) ? 0 : r.lead, m, fr > 0 ? fr : 0, e, f)) return true;
    }
    return false;
}

/* FIX n: n decimals in plain notation (UG p.1-21). Falls back to SCI n when a nonzero
 * value rounds to zero (UG p.1-16), when the plain form would show more than 34
 * significant digits, or when it is wider than the line (both ours, unit 002). */
static bool fmt_fix(text *t, const dec *v, int n, const ab_fmt *f)
{
    dec r = round_at(v, -n);
    bool fallback = (!is_zero(v) && is_zero(&r)) ||
                    (!is_zero(&r) && r.lead + n + 1 > ABN_DIGITS);
    if (!fallback) {
        start(t, f->width);
        if (plain(t, &r, n, f)) return true;
    }
    return fmt_sci(t, v, n, f);
}

/* The decimals needed to show every digit of v. */
static int decimals_of(const dec *v)
{
    int low = v->lead - v->n + 1;
    return (is_zero(v) || low >= 0) ? 0 : -low;
}

/* ALL: every significant digit, no trailing zeros, plain if it fits (UG p.1-23). If not,
 * in order (ours, unit 002 and its amendment 1): scientific with every digit; plain rounded
 * to the decimals that fit (down to none), trailing zeros removed, provided it still shows
 * at least min(the value's digits, 12) significant digits; scientific with as many
 * significant digits as fit, trailing zeros removed. Exact forms come before rounded ones. */
static bool fmt_all(text *t, const dec *v, const ab_fmt *f)
{
    start(t, f->width);
    if (plain(t, v, decimals_of(v), f)) return true;
    start(t, f->width);
    if (scientific(t, v, v->lead, 1, v->n - 1, v->lead, f)) return true;
    int want = v->n < 12 ? v->n : 12;
    for (int q = decimals_of(v); q >= 0; q--) {
        dec r = round_at(v, -q);
        start(t, f->width);
        if (!plain(t, &r, q, f)) continue;
        int shown = is_zero(&r) ? 0 : r.lead + q + 1;  /* digits rounded to, zeros included */
        if (shown < want) break;
        start(t, f->width);
        return plain(t, &r, decimals_of(&r), f);
    }
    for (int k = v->n - 1; k >= 0; k--) {
        dec r = round_sig(v, k + 1);
        start(t, f->width);
        if (scientific(t, &r, r.lead, 1, r.n - 1, r.lead, f)) return true;
    }
    return false;
}

int ab_format(const abn *value, const ab_fmt *f, char *out, int size)
{
    dec v;
    text t;
    bool ok = false;
    abn_digits(value, &v.neg, v.d, &v.lead);
    v.n = (int)strlen(v.d);
    int n = f->n < 0 ? 0 : (f->n > AB_FMT_N_MAX ? AB_FMT_N_MAX : f->n);
    if (f->width > 0) {
        switch (f->mode) {
        case AB_FIX: ok = fmt_fix(&t, &v, n, f); break;
        case AB_SCI: ok = fmt_sci(&t, &v, n, f); break;
        case AB_ENG: ok = fmt_eng(&t, &v, n, f); break;
        case AB_ALL: ok = fmt_all(&t, &v, f); break;
        }
    }
    if (!ok) {
        if (size > 0) out[0] = '\0';
        return -1;
    }
    if (size > 0) {
        int c = t.len < size - 1 ? t.len : size - 1;
        memcpy(out, t.s, (size_t)c);
        out[c] = '\0';
    }
    return t.len;
}

/* Fraction-display mode (UG p.5-1 to 5-9; rules ours, unit 007): the integer part and, unless
 * it is zero in a reducing format, the fraction: "2 3/8", "0 7/16", "-2 1/2", "3", and in the
 * fixed format "2 0/16". No thousands separators (the guide's fractions show none, p.5-3, p.5-5).
 * A fraction that does not fit, or whose integer part exceeds the 34 digits a number carries,
 * falls back to the decimal format with no indicator. */
int ab_format_display(const abn *v, const ab_display *d, char *out, int size, int *indicator)
{
    *indicator = 0;
    if (d->base == 16 || d->base == 8 || d->base == 2) {
        /* a base mode shows the integer part as a word pattern (UG p.11-2), whole: windowing a
           long pattern is the display's job (ours, unit 008 rule 6); beyond the word's range,
           the decimal format (ours) */
        int n = base_format(v, d->base, out, size);
        if (n >= 0) return n;
        return ab_format(v, &d->dec, out, size);
    }
    if (d->fdisp) {
        fr_result r;
        fr_format f = d->frac_format == 1 ? FR_FACTORS : d->frac_format == 2 ? FR_FIXED : FR_PRECISE;
        if (fr_approx(&r, v, d->c < 1 ? 1 : d->c, f) == ABN_OK) {
            char t[ABN_DIGITS + 32];
            int n = 0;
            if (r.neg) t[n++] = '-';
            for (const char *p = r.ip; *p; p++) t[n++] = *p;
            if (r.n != 0 || f == FR_FIXED) {
                char nb[12];
                int k = 0;
                uint32_t parts[2] = {r.n, r.d};
                t[n++] = ' ';
                for (int which = 0; which < 2; which++) {
                    uint32_t u = parts[which];
                    k = 0;
                    do {
                        nb[k++] = (char)('0' + u % 10);
                        u /= 10;
                    } while (u);
                    while (k) t[n++] = nb[--k];
                    if (which == 0) t[n++] = '/';
                }
            }
            t[n] = '\0';
            if (n <= d->dec.width) {
                if (size > 0) {
                    int c = n < size - 1 ? n : size - 1;
                    memcpy(out, t, (size_t)c);
                    out[c] = '\0';
                }
                *indicator = r.indicator;
                return n;
            }
        }
    }
    return ab_format(v, &d->dec, out, size);
}
