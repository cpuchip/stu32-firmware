/* eqn_say_test: unit 086's round trip (work/blueprint/spoken-equation.md, (c)). Every equation of tests/086-corpus.txt is
 * spoken by ab_eqn_words; the words are read back to calculator text by a reader of this file's own, which knows only
 * the blueprint's grammar and its phrase tables (copied here from the spec, not from eqn.c); the core then evaluates the
 * original and the read-back text at four sets of A-Z, and they must agree bit for bit. Then the read-back text is
 * spoken again and must give the same words.
 *
 *   build/eqn_say_test tests/086-corpus.txt
 *
 * Allowed, each counted and printed: a text the parser refuses gets no words; RAND and RANDOM differ by nature; a
 * read-back text whose groups nest past the parser's bound (047) can't be evaluated or spoken again. Anything else
 * fails, and so does a corpus with no spoken line. AB_SAY_PLANT=1 plants a fault in the reader (its control). */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "eqn.h"

typedef struct { const char *say, *name; } phrase;
static const phrase FN1[] = {
    {"the natural log of", "LN"}, {"the log of", "LOG"}, {"the exponential of", "EXP"}, {"the antilog of", "ALOG"},
    {"the square of", "SQ"}, {"the square root of", "SQRT"}, {"the reciprocal of", "INV"}, {"the integer part of", "IP"},
    {"the fractional part of", "FP"}, {"the rounding of", "RND"}, {"the absolute value of", "ABS"},
    {"the sign of", "SGN"}, {"the floor of", "INTG"}, {"the sine of", "SIN"}, {"the cosine of", "COS"},
    {"the tangent of", "TAN"}, {"the arcsine of", "ASIN"}, {"the arccosine of", "ACOS"}, {"the arctangent of", "ATAN"},
    {"the hyperbolic sine of", "SINH"}, {"the hyperbolic cosine of", "COSH"}, {"the hyperbolic tangent of", "TANH"},
    {"the inverse hyperbolic sine of", "ASINH"}, {"the inverse hyperbolic cosine of", "ACOSH"},
    {"the inverse hyperbolic tangent of", "ATANH"}, {"the conversion to degrees of", "\xe2\x86\x92" "DEG"},
    {"the conversion to radians of", "\xe2\x86\x92" "RAD"}, {"the conversion from hours minutes seconds of", "HMS\xe2\x86\x92"},
    {"the conversion to hours minutes seconds of", "\xe2\x86\x92" "HMS"}, {"the conversion to liters of", "\xe2\x86\x92L"},
    {"the conversion to gallons of", "\xe2\x86\x92GAL"}, {"the conversion to miles of", "\xe2\x86\x92MILE"},
    {"the conversion to kilometers of", "\xe2\x86\x92KM"}, {"the conversion to kilograms of", "\xe2\x86\x92KG"},
    {"the conversion to pounds of", "\xe2\x86\x92LB"}, {"the conversion to Celsius of", "\xe2\x86\x92\xc2\xb0" "C"},
    {"the conversion to Fahrenheit of", "\xe2\x86\x92\xc2\xb0" "F"}, {"the conversion to centimeters of", "\xe2\x86\x92" "CM"},
    {"the conversion to inches of", "\xe2\x86\x92IN"}, {"the seeding with", "SEED"}, {"the argument of", "ARG"},
    {"x hat of", "x\xcc\x82"}, {"y hat of", "\xc5\xb7"}, {"the logical not of", "NOT"}, {"the cube of", "CB"},
    {"the cube root of", "CBRT"},
};
typedef struct { const char *say, *name, *mid, *post; } phrase2;
static const phrase2 FN2[] = {
    {"the integer quotient of", "IDIV", "and", NULL}, {"the remainder of", "RMDR", "divided by", NULL},
    {"the percent change from", "%CHG", "to", NULL}, {"the root of index", "XROOT", "of", NULL},
    {"the combinations of", "nCr", "choose", NULL}, {"the permutations of", "nPr", "taken", "at a time"},
    {"the logical and of", "AND", "with", NULL}, {"the logical exclusive or of", "XOR", "with", NULL},
    {"the logical or of", "OR", "with", NULL}, {"the logical nand of", "NAND", "with", NULL},
    {"the logical nor of", "NOR", "with", NULL}, {"the percentage of", "%", "at", "percent"},
};
static const phrase NAME0[] = {
    {"a random number", "RAND"}, {"pi", "\xcf\x80"}, {"s x", "sx"}, {"s y", "sy"}, {"sigma x", "\xcf\x83x"},
    {"sigma y", "\xcf\x83y"}, {"x bar w", "x\xcc\x84w"}, {"x bar", "x\xcc\x84"}, {"y bar", "\xc8\xb3"},
    {"the correlation r", "r"}, {"the slope m", "m"}, {"the intercept b", "b"}, {"the count n", "n"},
    {"the sum of x squares", "\xce\xa3x\xc2\xb2"}, {"the sum of y squares", "\xce\xa3y\xc2\xb2"},
    {"the sum of x y products", "\xce\xa3xy"}, {"the sum of x", "\xce\xa3x"}, {"the sum of y", "\xce\xa3y"},
    {"register X", "REGX"}, {"register Y", "REGY"}, {"register Z", "REGZ"}, {"register T", "REGT"},
    {"the last answer", "ANS"}, {"indirect I", "\xe2\x92\xa4"}, {"indirect J", "\xe2\x92\xa5"},
};

#define MAXW 4096
typedef struct {
    char *w[MAXW];
    int n, i;
    bool m33, bad, plant;
    char out[16384];
    int o;
} rd;

static void put(rd *r, const char *s)
{
    size_t k = strlen(s);
    if (r->o + (int)k >= (int)sizeof r->out) {
        r->bad = true;
        return;
    }
    memcpy(r->out + r->o, s, k + 1);
    r->o += (int)k;
}

/* the phrase's words at the cursor: their count, or 0 */
static int peek(const rd *r, const char *phrase)
{
    char buf[128];
    snprintf(buf, sizeof buf, "%s", phrase);
    int k = 0;
    for (char *t = strtok(buf, " "); t; t = strtok(NULL, " "), k++)
        if (r->i + k >= r->n || strcmp(r->w[r->i + k], t)) return 0;
    return k;
}
static bool take(rd *r, const char *phrase)
{
    int k = peek(r, phrase);
    r->i += k;
    return k > 0;
}
static void need(rd *r, const char *phrase)
{
    if (!take(r, phrase)) r->bad = true;
}
static const char *word(rd *r)
{
    if (r->i >= r->n) {
        r->bad = true;
        return "";
    }
    return r->w[r->i++];
}

static void add(rd *r);
static void group(rd *r)
{
    need(r, "the quantity");
    put(r, "(");
    add(r);
    need(r, "end quantity");
    put(r, ")");
}

/* ordinal words (to nine digits) as an integer, -1 if these words aren't one */
static long ordinal(char **w, int n)
{
    static const char *const ONES[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
        "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
    static const char *const TENS[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    static const struct { const char *o, *c; } IRR[] = {{"first", "one"}, {"second", "two"}, {"third", "three"},
        {"fifth", "five"}, {"eighth", "eight"}, {"ninth", "nine"}, {"twelfth", "twelve"}};
    long total = 0, cur = 0;
    for (int k = 0; k < n; k++) {
        char t[64];
        snprintf(t, sizeof t, "%s", w[k]);
        if (k == n - 1) {                       /* the last word (its last part) is the ordinal */
            char *last = strrchr(t, '-');
            last = last ? last + 1 : t;
            size_t L = strlen(last);
            bool done = false;
            for (size_t q = 0; q < sizeof IRR / sizeof IRR[0] && !done; q++)
                if (!strcmp(last, IRR[q].o)) strcpy(last, IRR[q].c), done = true;
            if (!done && L > 4 && !strcmp(last + L - 4, "ieth")) strcpy(last + L - 4, "y"), done = true;
            if (!done && L > 2 && !strcmp(last + L - 2, "th")) last[L - 2] = '\0', done = true;
            if (!done) return -1;
        }
        if (!strcmp(t, "hundred")) {
            cur *= 100;
            continue;
        }
        if (!strcmp(t, "thousand") || !strcmp(t, "million")) {
            total += cur * (t[0] == 't' ? 1000 : 1000000);
            cur = 0;
            continue;
        }
        long v = 0;
        for (char *part = strtok(t, "-"); part; part = strtok(NULL, "-")) {
            bool hit = false;
            for (int q = 0; q < 20 && !hit; q++)
                if (!strcmp(part, ONES[q])) v += q, hit = true;
            for (int q = 2; q < 10 && !hit; q++)
                if (!strcmp(part, TENS[q])) v += 10 * q, hit = true;
            if (!hit) return -1;
        }
        cur += v;
    }
    return total + cur;
}

static bool is_digits(const char *s, bool point)
{
    if (!*s) return false;
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (point && *s == '.'))) return false;
    return true;
}

static void lone(rd *r)
{
    int best = -1, bl = 0;
    for (size_t k = 0; k < sizeof NAME0 / sizeof NAME0[0]; k++) {
        int n = peek(r, NAME0[k].say);
        if (n > bl) best = (int)k, bl = n;
    }
    if (best >= 0) {
        r->i += bl;
        put(r, r->m33 && !strcmp(NAME0[best].name, "RAND") ? "RANDOM" : NAME0[best].name);
        return;
    }
    static const struct { const char *w, *suf; } BASE[] = {{"hex", "h"}, {"binary", "b"}, {"octal", "o"}, {"decimal", "d"}};
    for (size_t k = 0; k < 4; k++)
        if (take(r, BASE[k].w)) {
            put(r, word(r));
            put(r, BASE[k].suf);
            return;
        }
    if (take(r, "the complex")) {
        put(r, word(r));
        if (take(r, "i")) put(r, "i");
        else if (take(r, "at angle")) put(r, "\xce\xb8");
        else r->bad = true;
        if (take(r, "negative")) put(r, "-");
        put(r, word(r));
        return;
    }
    if (take(r, "the fraction")) {
        const char *a = word(r);
        if (take(r, "and")) {
            put(r, a);
            put(r, ".");
            put(r, word(r));
        } else {
            put(r, ".");
            put(r, a);
        }
        need(r, "over");
        put(r, ".");
        put(r, word(r));
        return;
    }
    const char *w = word(r);
    if ((w[0] >= 'A' && w[0] <= 'Z' && !w[1]) || !strcmp(w, "i")) {
        put(r, w);
        return;
    }
    if (is_digits(w, true)) {
        put(r, w);
        if (take(r, "E")) {
            put(r, "E");
            if (take(r, "negative")) put(r, "-");
            put(r, word(r));
        }
        return;
    }
    r->bad = true;
}

/* a call's argument: a group or a lone operand (a two-argument call's may be negative and lone) */
static void arg(rd *r, bool neg_ok)
{
    if (peek(r, "the quantity")) group(r);
    else {
        if (neg_ok && take(r, "negative")) put(r, "-");
        lone(r);
    }
}

static bool call(rd *r)
{
    int b2 = -1, l2 = 0, b1 = -1, l1 = 0;
    for (size_t k = 0; k < sizeof FN2 / sizeof FN2[0]; k++) {
        int n = peek(r, FN2[k].say);
        if (n > l2) b2 = (int)k, l2 = n;
    }
    for (size_t k = 0; k < sizeof FN1 / sizeof FN1[0]; k++) {
        int n = peek(r, FN1[k].say);
        if (n > l1) b1 = (int)k, l1 = n;
    }
    if (b2 >= 0) {
        const phrase2 *f = &FN2[b2];
        const char *name = f->name;
        if (r->m33 && !strcmp(name, "nCr")) name = "Cn,r";
        if (r->m33 && !strcmp(name, "nPr")) name = "Pn,r";
        r->i += l2;
        put(r, name);
        put(r, "(");
        arg(r, true);
        need(r, f->mid);
        put(r, r->m33 ? ":" : ",");
        arg(r, true);
        if (f->post) need(r, f->post);
        put(r, ")");
        return true;
    }
    if (b1 < 0) return false;
    r->i += l1;
    bool rnd = !strcmp(FN1[b1].name, "RND");
    put(r, r->m33 && !strcmp(FN1[b1].name, "HMS\xe2\x86\x92") ? "\xe2\x86\x92HR" : FN1[b1].name);
    put(r, "(");
    arg(r, false);
    if (rnd && r->m33 && take(r, "to")) {      /* the 33s's RND(x:n) */
        put(r, ":");
        arg(r, false);
        need(r, "digits");
    }
    put(r, ")");
    return true;
}

static void prim(rd *r)
{
    if (peek(r, "the quantity")) {
        group(r);
        return;
    }
    if (take(r, "the vector")) {
        put(r, "[");
        add(r);
        while (take(r, "and")) put(r, ","), add(r);
        need(r, "end vector");
        put(r, "]");
        return;
    }
    if (!call(r)) lone(r);
}

static void post(rd *r)
{
    prim(r);
    while (take(r, "factorial")) put(r, "!");
}

static void power(rd *r)
{
    post(r);
    for (;;) {
        if (take(r, "squared")) put(r, "^2");
        else if (take(r, "cubed")) put(r, "^3");
        else if (take(r, "to the power")) put(r, "^"), group(r);
        else if (take(r, "to the")) {
            int j = r->i;
            while (j < r->n && strcmp(r->w[j], "power")) j++;
            long n = ordinal(r->w + r->i, j - r->i);
            put(r, "^");
            if (n >= 0) {
                char b[24];
                snprintf(b, sizeof b, "%ld", r->plant ? n + 1 : n);   /* AB_SAY_PLANT: the reader's control */
                put(r, b);
                r->i = j;
            } else {
                if (take(r, "negative")) put(r, "-");
                prim(r);
            }
            need(r, "power");
        } else return;
    }
}

static void neg(rd *r)
{
    if (take(r, "negative")) {
        put(r, "-");
        neg(r);
        return;
    }
    power(r);
}

static void mul(rd *r)
{
    neg(r);
    for (;;) {
        if (take(r, "times")) put(r, "\xc3\x97"), neg(r);
        else if (take(r, "divided by")) put(r, "\xc3\xb7"), neg(r);
        else return;
    }
}

static void add(rd *r)
{
    mul(r);
    for (;;) {
        if (take(r, "plus")) put(r, "+"), mul(r);
        else if (take(r, "minus")) put(r, "-"), mul(r);
        else return;
    }
}

/* the words back to text: 0, or -1 when they don't read */
static int read_back(char *words, bool m33, bool plant, char *out, int size)
{
    static rd r;
    memset(&r, 0, sizeof r);
    r.m33 = m33, r.plant = plant;
    for (char *t = strtok(words, " "); t && r.n < MAXW; t = strtok(NULL, " ")) r.w[r.n++] = t;
    add(&r);
    if (take(&r, "equals")) put(&r, "="), add(&r);
    if (r.bad || r.i != r.n) return -1;
    snprintf(out, (size_t)size, "%s", r.out);
    return 0;
}

static ab_calc c;

/* the two texts' values at four sets of A-Z: 1 the same at all four, 0 not, -1 the second doesn't evaluate as the
   first does because it nests too deep */
static int same_values(const char *t1, const char *t2, int model)
{
    for (int k = 0; k < 4; k++) {
        srand(1234 + k);
        for (int v = 0; v < 26; v++) {
            char b[40];
            abn x;
            snprintf(b, sizeof b, "%d.%03d", rand() % 7 - 3, rand() % 1000);
            abn_from_text(&x, b);
            memset(&c.var[v], 0, sizeof c.var[v]);
            c.var[v].kind = AB_REAL;
            c.var[v].re = x;
        }
        ab_val y1, y2;
        int e1, e2;
        abn_status s1 = eqn_eval(&c, t1, model, false, false, &y1, &e1), s2 = eqn_eval(&c, t2, model, false, false, &y2, &e2);
        if (s2 == ABN_TOO_DEEP && s1 != ABN_TOO_DEEP) return -1;
        if (s1 != s2) return 0;
        if (s1 != ABN_OK) continue;
        if (y1.kind != y2.kind || abn_cmp(&y1.re, &y2.re) || (y1.kind != AB_REAL && abn_cmp(&y1.im, &y2.im)) ||
            (y1.kind == AB_VECTOR && (y1.len != y2.len || abn_cmp(&y1.p3, &y2.p3))))
            return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s CORPUS\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    bool plant = getenv("AB_SAY_PLANT") != NULL;
    ab_init(&c);
    static char line[8192], words[16384], copy[16384], back[16384], again[16384];
    int lines = 0, spoken = 0, refused = 0, random = 0, deep = 0, bad = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#' || !line[0]) continue;
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        const char *text = tab + 1;
        int model = !strcmp(line, "STU") ? AB_MODEL_STU : atoi(line);
        lines++;
        ab_val v;
        int ep;
        bool parses = eqn_eval(&c, text, model, false, true, &v, &ep) == ABN_OK;
        int n = ab_eqn_words(&c, text, model, words, sizeof words);
        if (n < 0) {                            /* no words: the parser must refuse it too */
            if (parses) {
                printf("eqn_say_test: FAIL %s %s: it parses, but has no words\n", line, text);
                bad++;
            } else {
                refused++;
            }
            continue;
        }
        spoken++;
        if (!parses) {
            printf("eqn_say_test: FAIL %s %s: spoken, but the parser refuses it\n", line, text);
            bad++;
            continue;
        }
        snprintf(copy, sizeof copy, "%s", words);
        if (read_back(copy, model == 33, plant, back, sizeof back) < 0) {
            printf("eqn_say_test: FAIL %s %s: the words don't read back: %s\n", line, text, words);
            bad++;
            continue;
        }
        int same = same_values(text, back, model);
        bool rnd = strstr(text, "RAND") != NULL;
        if (same < 0) {
            deep++;
            continue;
        }
        if (!same && !rnd) {
            printf("eqn_say_test: FAIL %s %s: read back as %s, which evaluates otherwise\n", line, text, back);
            bad++;
            continue;
        }
        if (rnd && !same) random++;
        if (ab_eqn_words(&c, back, model, again, sizeof again) < 0 || strcmp(again, words)) {
            printf("eqn_say_test: FAIL %s %s: read back as %s, which is said otherwise: %s\n", line, text, back, again);
            bad++;
        }
    }
    fclose(f);
    printf("eqn_say_test: %d equations: %d spoken and read back (%d RAND apart, %d too deep to re-read), %d refused "
           "by the parser too; %s\n", lines, spoken, random, deep, refused, bad || !spoken ? "FAILED" : "every one the same");
    return bad || !spoken ? 1 : 0;
}
