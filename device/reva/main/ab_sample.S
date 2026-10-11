/* ab_sample: the level-5 stack sampler of the AB_MASK bench image (head-less only, never a product image; casim
 * docs/stack-gate.md's S, soroban #6930). On the S3 the level-5 vector calls a user xt_highint5 (IDF v5.4.2
 * xtensa_vectors.S: XCHAL_EXCM_LEVEL < 5); the CPU's internal timer 2 (CCOMPARE2) raises it on the core that armed it.
 * Each sample re-arms the timer, then keeps the interrupted A1 when it lies in [lo, hi) and is the lowest seen, with
 * EPC_5 beside it. It writes nothing on any task's stack: a2 and a3 are saved here, a0 is in EXCSAVE_5.
 * ab_sample words: 0 a2, 1 a3, 2 period (cycles), 3 lo, 4 hi, 5 min A1, 6 its PC, 7 samples in [lo, hi), 8 samples. */
    .data
    .align  4
    .global ab_sample
ab_sample:
    .space  36

    .section .iram1, "ax"
    .global xt_highint5
    .type   xt_highint5, @function
    .align  4
xt_highint5:
    movi    a0, ab_sample
    s32i    a2, a0, 0
    s32i    a3, a0, 4
    rsr     a2, ccount              /* the next sample: now + period (the write also clears this one) */
    l32i    a3, a0, 8
    add     a2, a2, a3
    wsr     a2, ccompare2
    l32i    a2, a0, 32
    addi    a2, a2, 1
    s32i    a2, a0, 32
    l32i    a2, a0, 12              /* the interrupted A1 inside [lo, hi)? */
    bltu    a1, a2, 1f
    l32i    a2, a0, 16
    bgeu    a1, a2, 1f
    l32i    a2, a0, 28
    addi    a2, a2, 1
    s32i    a2, a0, 28
    l32i    a2, a0, 20              /* the lowest so far? */
    bgeu    a1, a2, 1f
    s32i    a1, a0, 20
    rsr     a3, epc5
    s32i    a3, a0, 24
1:
    l32i    a2, a0, 0
    l32i    a3, a0, 4
    rsync
    rsr     a0, excsave5
    rfi     5

/* nothing references this file, so the link names this symbol (main/CMakeLists.txt) */
    .global ld_include_ab_sample
ld_include_ab_sample:
