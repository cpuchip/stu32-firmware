/* make clang-check's planted control: gcc -Wall -Wextra accepts this comparison, clang's -Wsign-compare refuses it (the
   class primer's wasm build found in casim and draw_test). The check must fail on it, or its flags are not biting. */
int clang_control(unsigned m, int k);
int clang_control(unsigned m, int k)
{
    return __builtin_popcount(m) != (unsigned)k;     /* casim 9cc5d45's line 4263 */
}
