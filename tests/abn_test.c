/* abn_test: the shipped number engine (abn.h) against the mathematics.
 *
 * Expected results (tests/cases.h) were computed with Python's decimal module at 34
 * significant digits, ROUND_HALF_EVEN, and are compared through tests/norm.h, not
 * through the engine's own parser.
 */
#include <stdio.h>
#include <string.h>

#include "abn.h"
#include "norm.h"
#include "cases.h"

static int failures, checks;

static void check_text(const char *what, const abn *got, const char *want_text)
{
    char got_text[ABN_TEXT_MAX];
    norm want, have;
    abn_to_text(got, got_text, sizeof got_text);
    checks++;
    if (normalize(want_text, &want) != 0 || normalize(got_text, &have) != 0 ||
        !norm_equal(&want, &have)) {
        failures++;
        printf("FAIL %s: expected %s, got %s\n", what, want_text, got_text);
    }
}

static void check(const char *what, int cond)
{
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL %s\n", what);
    }
}


/* unit 054: abn_small_int and abn_sig_digits read the decimal128 itself, without text; each must give what
 * abn_digits gives. The references are the text readings they replace (fn.c's small_int before 054). */
static bool ref_small_int(const abn *x, int32_t *v)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n;
    abn_digits(x, &neg, d, &lead);
    n = (int)strlen(d);
    if (n == 0) {
        *v = 0;
        return true;
    }
    if (lead < 0 || lead > 8 || n > lead + 1) return false;
    int32_t r = 0;
    for (int i = 0; i <= lead; i++) r = r * 10 + (i < n ? d[i] - '0' : 0);
    *v = neg ? -r : r;
    return true;
}

static int ref_sig_digits(const abn *x)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead;
    abn_digits(x, &neg, d, &lead);
    return (int)strlen(d);
}

static int read_checks, read_failures;

static void check_reads(const char *text, const abn *x)
{
    int32_t v0 = 0x5A5A5A5A, v1 = 0x5A5A5A5A;
    bool r0 = ref_small_int(x, &v0), r1 = abn_small_int(x, &v1);
    int s0 = ref_sig_digits(x), s1 = abn_sig_digits(x);
    read_checks++;
    if (r0 != r1 || (r0 && v0 != v1) || s0 != s1) {
        if (read_failures++ < 10)
            printf("FAIL reads of %s: small_int %d %ld (want %d %ld), sig_digits %d (want %d)\n", text, r1, (long)v1,
                   r0, (long)v0, s1, s0);
    }
}

static uint32_t lcg_state = 54;

static uint32_t lcg(uint32_t n)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return (lcg_state >> 8) % n;
}

#ifdef AB_DEVICE
#define READ_CASES 4000
#else
#define READ_CASES 400000
#endif

static void test_reads(void)
{
    static const char *const E[] = {
        "0", "-0", "1", "-1", "9", "10", "999999999", "-999999999", "1000000000", "-1000000000", "999999999.0",
        "999999999.5", "999999998.9999999999999999999999999", "1E8", "1E9", "-1E8", "1.5", "0.5", "-0.5",
        "2147483647", "2147483648", "-2147483648", "4294967296", "1.000000000", "100000000.0000000000000000000000000",
        "123456789", "12345678.9E1", "1234567.89E2", "1E-6143", "-1E-6143", "9.999999999999999999999999999999999E6144",
        "0.1E1", "10E-1", "5E0", "-0.000", "1E-1", "1000000000000000000000000000000000",
        "1000000000000000000000000000000000E-33", "1.000000000000000000000000000000001", "100", "12.3000",
        "0.3333333333333333333333333333333333", "1E33", "1E34", "1E-34", "4096", "-4096", "65", "1.00000000001",
    };
    abn x;
    for (size_t i = 0; i < sizeof E / sizeof E[0]; i++) {
        if (abn_from_text(&x, E[i]) == ABN_OK) check_reads(E[i], &x);
        if (abn_from_text_exact(&x, E[i]) == ABN_OK) check_reads(E[i], &x);     /* zeros keep their exponent */
    }
    static const char *const Z[] = {"0E5", "-0E-3", "0.000", "0E-6176", "0E6111", "-0E0"};
    for (size_t i = 0; i < sizeof Z / sizeof Z[0]; i++)
        if (abn_from_text_exact(&x, Z[i]) == ABN_OK) check_reads(Z[i], &x);
    for (int i = 0; i < READ_CASES; i++) {
        char text[ABN_TEXT_MAX + 16];
        int k = 1 + (int)lcg(34), z = (int)lcg(4) ? 0 : (int)lcg((uint32_t)(35 - k)), n = 0;
        int g = (int)lcg(10), e = g < 6 ? (int)lcg(25) - 12 : g < 9 ? (int)lcg(81) - 40 : (int)lcg(12300) - 6170;
        if (lcg(4) == 0) text[n++] = '-';
        text[n++] = (char)('1' + lcg(9));
        for (int j = 1; j < k; j++) text[n++] = (char)('0' + lcg(10));
        for (int j = 0; j < z; j++) text[n++] = '0';
        snprintf(text + n, sizeof text - (size_t)n, "E%d", e);
        if (abn_from_text(&x, text) != ABN_OK) continue;
        check_reads(text, &x);
        abn y, r;                       /* the cohorts arithmetic makes: products, sums, quotients */
        snprintf(text + n, sizeof text - (size_t)n, "E%d", (int)lcg(13) - 6);
        if (abn_from_text(&y, text) != ABN_OK) continue;
        if (abn_mul(&r, &x, &y) == ABN_OK) check_reads("a product", &r);
        if (abn_add(&r, &x, &y) == ABN_OK) check_reads("a sum", &r);
        if (abn_div(&r, &x, &y) == ABN_OK) check_reads("a quotient", &r);
    }
    checks++;
    if (read_failures) failures++;
    printf("abn_test: reads (unit 054): %d of %d agree with abn_digits\n", read_checks - read_failures, read_checks);
}

int main(void)
{
    char what[160];
    for (size_t i = 0; i < NCASES; i++) {
        abn a, b, r = abn_zero();
        abn_status st = ABN_BAD_TEXT;
        snprintf(what, sizeof what, "%s %c %s", CASES[i].a, CASES[i].op, CASES[i].b);
        if (abn_from_text(&a, CASES[i].a) != ABN_OK || abn_from_text(&b, CASES[i].b) != ABN_OK) {
            check(what, 0);
            continue;
        }
        switch (CASES[i].op) {
        case '+': st = abn_add(&r, &a, &b); break;
        case '-': st = abn_sub(&r, &a, &b); break;
        case '*': st = abn_mul(&r, &a, &b); break;
        case '/': st = abn_div(&r, &a, &b); break;
        }
        check(what, st == ABN_OK);
        check_text(what, &r, CASES[i].want);
    }

    /* Text in: entry-shaped strings and malformed ones. */
    abn a, b, r;
    check("parse 0.000062", abn_from_text(&a, "0.000062") == ABN_OK);
    check_text("parse 0.000062", &a, "6.2E-5");
    check("parse -6.6261E-34", abn_from_text(&a, "-6.6261E-34") == ABN_OK);
    check_text("parse -6.6261E-34", &a, "-6.6261E-34");
    check("parse 1E6", abn_from_text(&a, "1E6") == ABN_OK);
    check_text("parse 1E6", &a, "1000000");
    check("parse 12.", abn_from_text(&a, "12.") == ABN_OK);
    check_text("parse 12.", &a, "12");
    check("parse -0 is zero, not negative", abn_from_text(&a, "-0") == ABN_OK && abn_is_zero(&a));
    check_text("parse -0", &a, "0");
    check("parse 35 digits rounds half-even",
          abn_from_text(&a, "1.0000000000000000000000000000000005") == ABN_OK);
    check_text("parse 35 digits rounds half-even", &a, "1");
    check("parse past 36 digits keeps a sticky digit",
          abn_from_text(&a, "1.00000000000000000000000000000000050000001") == ABN_OK);
    check_text("parse past 36 digits keeps a sticky digit", &a, "1.000000000000000000000000000000001");
    check("reject empty", abn_from_text(&a, "") == ABN_BAD_TEXT);
    check("reject .", abn_from_text(&a, ".") == ABN_BAD_TEXT);
    check("reject 1.2.3", abn_from_text(&a, "1.2.3") == ABN_BAD_TEXT);
    check("reject 1E", abn_from_text(&a, "1E") == ABN_BAD_TEXT);
    check("reject 1x", abn_from_text(&a, "1x") == ABN_BAD_TEXT);
    check("reject exponent past range", abn_from_text(&a, "1E10000") == ABN_OVERFLOW);
    check("top of range", abn_from_text(&a, "9.999999999999999999999999999999999E6144") == ABN_OK);
    check("past the top", abn_from_text(&a, "1E6145") == ABN_OVERFLOW);
    check("bottom of range", abn_from_text(&a, "1E-6143") == ABN_OK);
    check("below the bottom", abn_from_text(&a, "1E-6144") == ABN_UNDERFLOW);
    check("reject inf", abn_from_text(&a, "inf") == ABN_BAD_TEXT);
    check("reject nan", abn_from_text(&a, "nan") == ABN_BAD_TEXT);

    /* Text out reads back to the same value. */
    const char *round_trip[] = {"0", "-5", "0.3", "149.0987", "6.6261E-34", "4.085E16",
                                "0.3333333333333333333333333333333333", "1E-33", "2.5E+20"};
    for (size_t i = 0; i < sizeof round_trip / sizeof round_trip[0]; i++) {
        char text[ABN_TEXT_MAX];
        snprintf(what, sizeof what, "round trip %s", round_trip[i]);
        abn_from_text(&a, round_trip[i]);
        abn_to_text(&a, text, sizeof text);
        check(what, abn_from_text(&b, text) == ABN_OK && abn_cmp(&a, &b) == 0);
        check_text(what, &b, round_trip[i]);
    }

    /* Errors leave the output alone and say why. */
    abn_from_text(&a, "1");
    abn_from_text(&b, "0");
    r = a;
    check("divide by zero", abn_div(&r, &a, &b) == ABN_DIV_ZERO);
    check_text("divide by zero leaves out", &r, "1");
    abn_from_text(&a, "1E4000");
    r = a;
    check("multiply past range: overflow (unit 026)", abn_mul(&r, &a, &a) == ABN_OVERFLOW);
    check("its sign: positive", !abn_overflow_negative());
    {
        abn na = abn_neg(&a);
        check("a negative product past range: overflow, negative", abn_mul(&r, &a, &na) == ABN_OVERFLOW &&
              abn_overflow_negative());
        r = a;
    }
    check_text("multiply past range leaves out", &r, "1E4000");
    abn_from_text(&a, "1E-4000");
    check("multiply below range: underflow (unit 026)", abn_mul(&r, &a, &a) == ABN_UNDERFLOW);
    check("the largest number: 34 nines E6144", abn_from_text(&b, "9.999999999999999999999999999999999E6144") == ABN_OK &&
          (r = abn_largest(), abn_cmp(&r, &b) == 0));

    /* No negative zero leaves the engine: decimal arithmetic makes one from 0 x -5 and
     * from -0 in text, and the calculator must never show it. */
    {
        abn neg5, zero = abn_zero();
        char text[ABN_TEXT_MAX];
        abn_from_text(&neg5, "-5");
        abn_mul(&r, &zero, &neg5);
        abn_to_text(&r, text, sizeof text);
        check("0 x -5 is not negative zero", text[0] != '-');
        abn_from_text(&r, "-0.000");
        abn_to_text(&r, text, sizeof text);
        check("-0.000 reads as zero, not negative zero", text[0] != '-');
        abn_div(&r, &zero, &neg5);
        abn_to_text(&r, text, sizeof text);
        check("0 / -5 is not negative zero", text[0] != '-');
    }

    /* Sign, significant digits and the exponent of the leading digit (for the formatter). */
    {
        static const struct { const char *in; int neg; const char *digits; int lead; } D[] = {
            {"149.0987", 0, "1490987", 2},
            {"-6.6261E-34", 1, "66261", -34},
            {"1E6", 0, "1", 6},
            {"100", 0, "1", 2},
            {"12.3000", 0, "123", 1},
            {"0", 0, "", 0},
            {"-0.000062", 1, "62", -5},
            {"0.3333333333333333333333333333333333", 0, "3333333333333333333333333333333333", -1},
            {"9.999999999999999999999999999999999E6144", 0, "9999999999999999999999999999999999", 6144},
            {"1E-6143", 0, "1", -6143},
        };
        for (size_t i = 0; i < sizeof D / sizeof D[0]; i++) {
            char dg[ABN_DIGITS + 1];
            bool ng = true;
            int ld = 12345;
            snprintf(what, sizeof what, "digits of %s", D[i].in);
            check(what, abn_from_text(&a, D[i].in) == ABN_OK);
            abn_digits(&a, &ng, dg, &ld);
            check(what, ng == (D[i].neg != 0) && strcmp(dg, D[i].digits) == 0 && ld == D[i].lead);
        }
        abn_from_text(&a, "1");
        abn_from_text(&b, "3");
        abn_div(&r, &a, &b);
        {
            char dg[ABN_DIGITS + 1];
            bool ng;
            int ld;
            abn_digits(&r, &ng, dg, &ld);
            check("digits of 1/3", !ng && ld == -1 && strcmp(dg, "3333333333333333333333333333333333") == 0);
        }
    }

    /* Comparison and negation. */
    abn m5, p3, z = abn_zero();
    abn_from_text(&m5, "-5");
    abn_from_text(&p3, "3");
    check("cmp -5 < 3", abn_cmp(&m5, &p3) < 0);
    check("cmp 3 > -5", abn_cmp(&p3, &m5) > 0);
    check("cmp 3 = 3.000", abn_from_text(&a, "3.000") == ABN_OK && abn_cmp(&a, &p3) == 0);
    check("cmp 0.3 < 3", abn_from_text(&a, "0.3") == ABN_OK && abn_cmp(&a, &p3) < 0);
    check("cmp -0.3 > -5", abn_from_text(&a, "-0.3") == ABN_OK && abn_cmp(&a, &m5) > 0);
    check("cmp 0 = 0", abn_cmp(&z, &z) == 0);
    r = abn_neg(&z);
    check("negating zero gives zero", abn_is_zero(&r) && abn_cmp(&r, &z) == 0);
    check_text("negating zero", &r, "0");
    r = abn_neg(&m5);
    check_text("negate -5", &r, "5");

    test_reads();

    printf("abn_test: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
