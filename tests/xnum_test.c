/* xnum_test: the exact layer's edge cases that random data almost never reaches (unit 010b).
 *
 * The square roots are correctly rounded only if the sticky digit is right: it decides only when
 * every computed digit past the 34th is 5 then zeros, about one case in 10^7. Built here from
 * r = 12345678901234567890123456789012345 (35 digits, a tie at 34, the 34th digit even):
 *   sqrt(r^2)            an exact tie: rounds to even, ...234;
 *   sqrt(r^2 + 1/b)      the quotient is inexact but its root exact (b > 10^k): only the quotient's
 *                        sticky rounds it up, ...235;
 *   sqrt(r^2 10^80 + 1)  the quotient exact, the root inexact: only the root's sticky rounds up.
 * Also the in-place bigint add_u32 / sub_u32 (carries and borrows across limbs, the refusal).
 * Expected values: python integers (math.isqrt-free: they are r itself, rounded by hand).
 */
#include <stdio.h>
#include <string.h>

#include "xnum.h"

static int fails, checks;

static void set(xnum *x, const char *digits)
{
    x->neg = false;
    x->e = 0;
    bi_from_digits(&x->m, digits, 0);
}

static void check_root(const char *name, const char *a, const char *b, const char *want)
{
    static xnum xa, xb;
    abn got, w;
    char t[ABN_TEXT_MAX];
    set(&xa, a);
    set(&xb, b);
    abn_status st = xn_sqrt_ratio_to_abn(&got, &xa, &xb);
    abn_from_text(&w, want);
    checks++;
    if (st != ABN_OK || abn_cmp(&got, &w) != 0) {
        abn_to_text(&got, t, sizeof t);
        printf("FAIL %s: expected %s, got %s (status %d)\n", name, want, t, (int)st);
        fails++;
    }
}

static void check_bi(const char *name, bool ok, const bigint *a, const char *want, bool want_ok)
{
    static bigint w;
    bi_from_digits(&w, want, 0);
    checks++;
    if (ok != want_ok || bi_cmp(a, &w) != 0) {
        printf("FAIL %s\n", name);
        fails++;
    }
}

int main(void)
{
    check_root("tie, exact", "152415787532388367504953515625666802687090533479957338669120562399025", "1", "1.234567890123456789012345678901234E34");
    check_root("above the tie, quotient sticky only", "1524157875323883675049535156256668026870905334799573386691205623990250000000000000000000000000000000152415787532388367504953515625666802687090533479957338669120562399026", "10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001", "1.234567890123456789012345678901235E34");
    check_root("above the tie, root sticky only", "15241578753238836750495351562566680268709053347995733866912056239902500000000000000000000000000000000000000000000000000000000000000000000000000000001", "1", "1.234567890123456789012345678901235E74");
    check_root("a square", "144", "1", "12");
    check_root("a ratio that is a square", "9", "4", "1.5");

    static bigint a;
    bool ok;
    bi_from_digits(&a, "999999999999999999", 0);            /* two full limbs */
    ok = bi_add_u32(&a, 1);
    check_bi("add_u32 carries across limbs", ok, &a, "1000000000000000000", true);
    ok = bi_add_u32(&a, 4294967295u);
    check_bi("add_u32 a large addend", ok, &a, "1000000004294967295", true);
    bi_from_digits(&a, "1000000000000000000", 0);
    ok = bi_sub_u32(&a, 1);
    check_bi("sub_u32 borrows across limbs", ok, &a, "999999999999999999", true);
    bi_from_digits(&a, "5000000000", 0);
    ok = bi_sub_u32(&a, 4294967295u);
    check_bi("sub_u32 a large subtrahend", ok, &a, "705032705", true);
    bi_from_digits(&a, "7", 0);
    ok = bi_sub_u32(&a, 8);
    check_bi("sub_u32 refuses to go below 0", ok, &a, "7", false);

    printf("xnum_test: %d/%d checks\n", checks - fails, checks);
    return fails ? 1 : 0;
}
