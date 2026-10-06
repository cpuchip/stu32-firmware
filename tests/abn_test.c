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

    printf("abn_test: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
