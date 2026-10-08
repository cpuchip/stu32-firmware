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
   right of an assignment's = only (the ENTER prompts, p.6-11); with with_i (33s mode, unit 025), a
   lone i is the variable i, 26 (33s p.13-23). Returns the count. */
int eqn_vars(const char *text, bool rhs_only, bool with_i, int model, int out[27]);    /* model: its names (041 D) */

/* The length in bytes of a function name and its "(" ending at byte pos of s, or 0 (for the
   editor's backspace, which removes a function as one element). */
int eqn_name_before(const char *s, int pos);

/* An assignment's variable (a single letter, then =; p.6-9; i too with with_i), or -1. */
int eqn_assign_var(const char *text, bool with_i);

/* The equation's value (p.6-10): left minus right for an =, or with rhs_only the right side alone.
   With check_only, the syntax is checked and nothing is computed. SYNTAX ERROR: ABN_SYNTAX with
   *errpos the character offset of the first error (p.6-19). Every operation is the keyboard's own
   function (unit 014 rule 2). */
abn_status eqn_eval(ab_calc *c, const char *text, int model, bool rhs_only, bool check_only, ab_val *out, int *errpos);

/* Unit 031. model is the model the text was typed in (33, 35, AB_MODEL_STU; 0 as 35): STU's equations
   group ^ right to left and read two operands side by side as a product (rules 1, 5, 6).
   eqn_write_implied: text with an explicit x written at each implied product (rule 7) into out: 1;
   0 when the text does not parse (a message, kept as typed: answer J); -1 when it would not fit
   (answer I). eqn_has_chain: whether the text has a^b^c at one level (rule 3's mark). */
int eqn_write_implied(ab_calc *c, const char *text, char *out, int size);
bool eqn_has_chain(ab_calc *c, const char *text, int model);
/* Unit 047: whether the text, parsed as its model reads it, nests past the parser's bound (NESTED TOO DEEP): a store
   refuses it. The check's own parse is bounded too. */
bool eqn_too_deep(ab_calc *c, const char *text, int model);
#ifndef ESP_PLATFORM
/* The units an unbounded parse reaches (+8 a group, vector or argument list, +1 a step of STU's power chain): tests'
   only, for tests/eqn_depth.py's check; on a host, whose stack is large. */
int eqn_depth_peak(ab_calc *c, const char *text, int model);
#endif

#endif
