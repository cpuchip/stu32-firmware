/* base: integers in base modes and the logic functions (unit 008; see base.h). */
#include <string.h>

#include "base.h"

#define MASK ((((uint64_t)1) << AB_WORD_BITS) - 1)

int64_t base_max(void)
{
    return (int64_t)((((uint64_t)1) << (AB_WORD_BITS - 1)) - 1);
}

int64_t base_min(void)
{
    return -(int64_t)(((uint64_t)1) << (AB_WORD_BITS - 1));
}

int64_t base_clamp(int64_t v, bool *clamped)
{
    if (v > base_max()) {
        *clamped = true;
        return base_max();
    }
    if (v < base_min()) {
        *clamped = true;
        return base_min();
    }
    return v;
}

abn_status base_to_word(const abn *x, int64_t *v, bool *clamped)
{
    abn t;
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n;
    abn_status st = abn_trunc(&t, x);
    if (st != ABN_OK) return st;
    abn_digits(&t, &neg, d, &lead);
    n = (int)strlen(d);
    if (n == 0) {
        *v = 0;
        return ABN_OK;
    }
    if (lead > 17) {        /* far beyond any word: the nearest end */
        *clamped = true;
        *v = neg ? base_min() : base_max();
        return ABN_OK;
    }
    int64_t r = 0;
    for (int i = 0; i <= lead; i++) r = r * 10 + (i < n ? d[i] - '0' : 0);
    *v = base_clamp(neg ? -r : r, clamped);
    return ABN_OK;
}

abn base_from_word(int64_t v)
{
    char t[24];
    int n = 0;
    uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    char rev[24];
    int k = 0;
    abn out;
    if (v < 0) t[n++] = '-';
    do {
        rev[k++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (k) t[n++] = rev[--k];
    t[n] = '\0';
    abn_from_text(&out, t);
    return out;
}

int base_format(const abn *x, int base, char *out, int size)
{
    int64_t v;
    bool clamped = false;
    char rev[AB_WORD_BITS + 2], t[AB_WORD_BITS + 3];
    int k = 0, n = 0, shift = base == 16 ? 4 : base == 8 ? 3 : 1;
    if (base_to_word(x, &v, &clamped) != ABN_OK || clamped) return -1;
    uint64_t u = (uint64_t)v & MASK;    /* the word's two's complement pattern */
    do {
        unsigned dgt = (unsigned)(u & (uint64_t)(base - 1));
        rev[k++] = (char)(dgt < 10 ? '0' + dgt : 'A' + dgt - 10);
        u >>= shift;
    } while (u);
    while (k) t[n++] = rev[--k];
    t[n++] = base == 16 ? 'h' : base == 8 ? 'o' : 'b';
    t[n] = '\0';
    if (size > 0) {
        int c = n < size - 1 ? n : size - 1;
        memcpy(out, t, (size_t)c);
        out[c] = '\0';
    }
    return n;
}

bool base_parse(const char *digits, int base, int64_t *v)
{
    uint64_t r = 0;
    if (!*digits) return false;
    for (const char *p = digits; *p; p++) {
        int d = (*p >= '0' && *p <= '9') ? *p - '0'
              : (*p >= 'A' && *p <= 'F') ? *p - 'A' + 10
              : (*p >= 'a' && *p <= 'f') ? *p - 'a' + 10 : 99;
        if (d >= base) return false;
        r = r * (uint64_t)base + (uint64_t)d;
        if (r > (uint64_t)base_max()) return false;
    }
    *v = (int64_t)r;
    return true;
}

/* The signed value of a word pattern. */
static int64_t signed_of(uint64_t u)
{
    u &= MASK;
    if (u & (((uint64_t)1) << (AB_WORD_BITS - 1))) return (int64_t)u - (int64_t)(((uint64_t)1) << AB_WORD_BITS);
    return (int64_t)u;
}

/* AND, OR, XOR, NAND, NOR on the word patterns, returning the signed value (UG p.11-4). */
int64_t base_logic2(base_logic op, int64_t y, int64_t x)
{
    uint64_t a = (uint64_t)y & MASK, b = (uint64_t)x & MASK, r;
    switch (op) {
    case BL_AND: r = a & b; break;
    case BL_OR: r = a | b; break;
    case BL_XOR: r = a ^ b; break;
    case BL_NAND: r = ~(a & b); break;
    default: r = ~(a | b); break;
    }
    return signed_of(r);
}

int64_t base_not(int64_t x)
{
    return signed_of(~((uint64_t)x & MASK));
}
