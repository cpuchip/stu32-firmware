/* lin: the built-in linear solvers (UG p.7-6): n = 2 or 3 equations in x, y (z), the coefficients
 * row by row with each row's constant last (Ax+By=C, Dx+Ey=F; Ax+By+Cz=D, Ex+Fy+Gz=H,
 * Ix+Jy+Kz=L). Exact: the determinant and Cramer's numerators are formed without rounding and
 * each solution is rounded once, half to even, to 34 digits. A singular system is NO SOLUTION
 * when the constants make it inconsistent and MULT SOLUTION when they do not, decided by the
 * exact ranks of the matrix and the augmented matrix. */
#ifndef LIN_H
#define LIN_H
#include "abn.h"

/* coef holds n * (n + 1) numbers; out receives n. ABN_OK, ABN_NO_SOLUTION, ABN_MULT_SOLUTION,
   or ABN_RANGE when an exact value would not fit (out untouched unless ABN_OK). */
abn_status lin_solve(int n, const abn *coef, abn *out);

#endif
