/* eqn: equations as text, parsed and evaluated (unit 014; UG ch.6).
 *
 * The text is the guide's notation in UTF-8: variables A..Z, numbers (digits, a point, E and an
 * exponent; two points make a fraction a.b.c as at the keyboard), + - × ÷ ^ = ( ) , and !, the
 * function names of p.6-16 followed by "(", and the names without arguments (π, RAND, the
 * statistics). The grammar follows p.6-15's precedence table; where it is silent (ours): left to
 * right within each level, ^ included; unary minus may begin any operand of + - × ÷ (B+-A,
 * A--B, 2×-A) and the exponent of ^ (p.17-2's ^-N); ! postfix; no implied multiplication; one =
 * at the top level only.
 *
 *   eq   := add [ "=" add ]
 *   add  := mul { ("+" | "-") mul }
 *   mul  := neg { ("×" | "÷") neg }
 *   neg  := "-" neg | pow                    (power before unary minus: -A^2 is -(A^2))
 *   pow  := post { "^" parg }
 *   parg := "-" parg | post
 *   post := prim { "!" }
 *   prim := number | variable | name | name "(" add [ "," add ] ")" | "(" add ")"
 *         | "[" add { "," add } "]"           (a vector, unit 015)
 *
 * Unit 015: values are whole stack values (real, complex, vector), and each operation is done by
 * the keyboard itself (ab_eqn_op), so complex numbers, vectors, base modes and INVALID DATA follow
 * the keys exactly. Numbers may be complex (1i2, 3θ10) or marked with a base (1Fh, 1100b, 17o,
 * 12d; unmarked is decimal, p.11-3). The logic functions take the function form of p.11-4, and %
 * is %(y,x) (ours).
 */
#ifndef EQN_H
#define EQN_H

#include "calc.h"

/* The variables an equation uses, in order of first appearance (UG p.6-13); with rhs_only, those
   right of an assignment's = only (the ENTER prompts, p.6-11). Returns the count. */
int eqn_vars(const char *text, bool rhs_only, int out[26]);

/* The length in bytes of a function name and its "(" ending at byte pos of s, or 0 (for the
   editor's backspace, which removes a function as one element). */
int eqn_name_before(const char *s, int pos);

/* An assignment's variable (a single letter, then =; p.6-9), or -1. */
int eqn_assign_var(const char *text);

/* The equation's value (p.6-10): left minus right for an =, or with rhs_only the right side alone.
   With check_only, the syntax is checked and nothing is computed. SYNTAX ERROR: ABN_SYNTAX with
   *errpos the character offset of the first error (p.6-19). Every operation is the keyboard's own
   function (unit 014 rule 2). */
abn_status eqn_eval(ab_calc *c, const char *text, bool rhs_only, bool check_only, ab_val *out, int *errpos);

#endif
