/* abn_dec: the decimal engine behind abn.h. STUB: the oracle lands first. */
#include "abn.h"

abn abn_zero(void) { abn z = {0, 0, 0, {0}}; return z; }
bool abn_is_zero(const abn *a) { (void)a; return true; }
int abn_cmp(const abn *a, const abn *b) { (void)a; (void)b; return 0; }
abn abn_neg(const abn *a) { (void)a; return abn_zero(); }
abn_status abn_add(abn *out, const abn *a, const abn *b) { (void)a; (void)b; *out = abn_zero(); return ABN_OK; }
abn_status abn_sub(abn *out, const abn *a, const abn *b) { (void)a; (void)b; *out = abn_zero(); return ABN_OK; }
abn_status abn_mul(abn *out, const abn *a, const abn *b) { (void)a; (void)b; *out = abn_zero(); return ABN_OK; }
abn_status abn_div(abn *out, const abn *a, const abn *b) { (void)a; (void)b; *out = abn_zero(); return ABN_OK; }
abn_status abn_from_text(abn *out, const char *s) { (void)s; *out = abn_zero(); return ABN_OK; }
int abn_to_text(const abn *a, char *buf, int size) { (void)a; if (size > 1) { buf[0] = '0'; buf[1] = '\0'; } return 1; }
