/* tests/golden-v21-087-div0.img's maker (the CAS 038 repin, abacus #13773): an image saved by 0a7de57's own core (VER
   0.87, image version 21: the expression 3×X^2-4 in Z) with DIVIDE BY 0 the message showing. Not built by make: it is
   built against 0a7de57's tree, in a worktree of that commit (scripts/resources.py's build/res/0a7de57), as
     cc -std=c11 -O2 -DAB_CASIM -Ibuild/casim -Icore -Ifirmware -DDECIMAL_CALL_BY_REFERENCE=0 -DDECIMAL_GLOBAL_ROUNDING=0
        -DDECIMAL_GLOBAL_EXCEPTION_FLAGS=0 -isystem <main>/third_party/intel-dfp/IntelRDFPMathLib20U5/LIBRARY/src
        -o mkdiv0 <this file> core/{calc,fmt,fn,bigint,const,xnum,stat,frac,base,cplx,hp,sbig,arena,eqn,lin,state,msg,
        list,abn_intel}.c build/casim/casim.c <main>/build/intel/libbid.a -lm
     ./mkdiv0 golden-v21-087-div0.img     (prints: X kind 0, Z kind 3, status 1 msg 1 err_status 1, 773 B, version 21) */
#include <stdio.h>
#include "calc.h"
#include "state.h"
#include "eqn.h"
static ab_calc c;
static uint8_t buf[65536];
int main(int argc, char **argv)
{
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    ab_eqn_add(&c, "3\xc3\x97X^2-4");
    ab_do(&c, AB_EQDROP);                   /* X: an expression, so the image needs 21 */
    ab_do(&c, AB_K1); ab_do(&c, AB_ENTER); ab_do(&c, AB_K0);
    abn_status st = ab_do(&c, AB_DIV);
    size_t n = ab_state_save(&c, buf, sizeof buf);
    FILE *f = fopen(argv[1], "wb");
    fwrite(buf, 1, n, f);
    fclose(f);
    printf("X kind %d, ", (int)c.x.kind);
    printf("Z kind %d, ", (int)c.z.kind);
    printf("status %d msg %d err_status %d, %zu B, version %u\n", (int)st, (int)c.msg, (int)c.err_status, n, buf[4] | buf[5] << 8);
    return 0;
}
