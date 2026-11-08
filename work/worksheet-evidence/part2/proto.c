/* THROWAWAY (part 2 blueprint's prototype): mock frames of the worksheet page from real rows. Never committed. */
#include "screen.c"
#include "rows.h"
#include "state.h"
#include "opnames.h"
#include <stdlib.h>

static void be32(uint8_t *p, uint32_t v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static int png_chunk(FILE *o, const char *type, const uint8_t *data, size_t n)
{
    static uint8_t buf[4 + 16384]; uint8_t len[4], crc[4];
    memcpy(buf, type, 4); if (n) memcpy(buf + 4, data, n);
    be32(len, (uint32_t)n); be32(crc, ab_crc32(buf, 4 + n));
    return fwrite(len,1,4,o)==4 && fwrite(buf,1,4+n,o)==4+n && fwrite(crc,1,4,o)==4;
}
static void write_png(const char *path, const ls027_frame *f)
{
    enum { ROW = 1 + LS027_ROW_BYTES, RAW = ROW * LS027_H };
    static uint8_t z[2 + 5 + RAW + 4]; uint8_t hdr[13] = {0}; uint32_t a = 1, b = 0;
    be32(hdr, LS027_W); be32(hdr + 4, LS027_H); hdr[8] = 1;
    z[0]=0x78; z[1]=1; z[2]=1; z[3]=RAW&0xFF; z[4]=RAW>>8; z[5]=~z[3]; z[6]=~z[4];
    for (int y = 0; y < LS027_H; y++) { uint8_t *r = z + 7 + y*ROW; r[0]=0;
        for (int i = 0; i < LS027_ROW_BYTES; i++) { uint8_t v=f->px[y][i], o=0;
            for (int k=0;k<8;k++) o |= (uint8_t)((~v>>k&1)<<(7-k)); r[1+i]=o; } }
    for (int i = 0; i < RAW; i++) { a=(a+z[7+i])%65521; b=(b+a)%65521; }
    be32(z + 7 + RAW, b << 16 | a);
    FILE *o = fopen(path, "wb");
    fwrite("\x89PNG\r\n\x1a\n",1,8,o); png_chunk(o,"IHDR",hdr,13); png_chunk(o,"IDAT",z,sizeof z); png_chunk(o,"IEND",NULL,0);
    fclose(o);
}

static const char *opname(const ab_row *r, char *buf, size_t n)
{
    if (r->op == AB_ROW_OP_NUM) return "";
    if (r->op == AB_ROW_OP_ALG) return "ALG";
    static const struct { int op; const char *s; } M[] = {
        {AB_ADD,"+"},{AB_SUB,"\xe2\x88\x92"},{AB_MUL,"\xc3\x97"},{AB_DIV,"\xc3\xb7"},{AB_SQRT,"\xe2\x88\x9ax"},
        {AB_SQ,"x\xc2\xb2"},{AB_INV,"1/x"},{AB_LASTX,"LASTx"},{AB_RCL,"RCL"},{AB_SIN,"SIN"},{AB_TOPOL,"\xe2\x86\x92POL"}};
    for (size_t i = 0; i < sizeof M / sizeof M[0]; i++)
        if ((int)r->op == M[i].op) {
            if (r->op == AB_RCL) { snprintf(buf, n, "RCL %c", 'A' + r->arg); return buf; }
            return M[i].s;
        }
    return r->op < AB_OPNAMES_N ? AB_OPNAMES[r->op] : "?";
}

#include "mock.inc"
static void K(ab_calc *c, const char *s)
{
    char buf[512]; snprintf(buf, sizeof buf, "%s", s);
    for (char *t = strtok(buf, " "); t; t = strtok(NULL, " ")) {
        static const struct { const char *n; int op; } M[] = {{"ENTER",AB_ENTER},{"+",AB_ADD},{"-",AB_SUB},{"*",AB_MUL},
            {"/",AB_DIV},{"SQRT",AB_SQRT},{"SQ",AB_SQ},{".",AB_KPOINT},{"XY",AB_XY},{"RDN",AB_RDN},{"LASTX",AB_LASTX},
            {"CHS",AB_KCHS},{"TOPOL",AB_TOPOL},{"PI",AB_PI},{"SIN",AB_SIN},{"ANS",AB_ANS}};
        if (!strcmp(t, "STU")) { ab_do_arg(c, AB_MODEL, AB_MODEL_STU); continue; }
        if (!strcmp(t, "ALG")) { ab_do_arg(c, AB_ENTRYM, 1); continue; }
        if (!strncmp(t, "STO:", 4)) { ab_do_arg(c, AB_STO, t[4]-'A'); continue; }
        if (!strncmp(t, "RCL:", 4)) { ab_do_arg(c, AB_RCL, t[4]-'A'); continue; }
        if (t[0] >= '0' && t[0] <= '9') { for (char *d = t; *d; d++) ab_do(c, (ab_op)(AB_K0 + *d - '0')); continue; }
        size_t i; for (i = 0; i < sizeof M / sizeof M[0]; i++) if (!strcmp(M[i].n, t)) { ab_do(c, (ab_op)M[i].op); break; }
        if (i == sizeof M / sizeof M[0]) { fprintf(stderr, "no key %s\n", t); exit(2); }
    }
}

int main(void)
{
    static ab_calc c;
    struct { const char *name, *keys; int drop; } S[] = {
        {"p1-rpn-chain", "STU 3 ENTER 4 * 5 + 2 / SQRT", 0},
        {"p2-rpn-y-far-back", "STU 2 ENTER 3 ENTER 4 * 1 + 7 SQRT", 0},
        {"p3-rpn-t-copy-down", "STU 1 ENTER 2 ENTER 3 ENTER 4 +", 0},
        {"p4-rpn-typing", "STU 3 ENTER 4 * 5 + 2", 0},
        {"p5-rpn-dropped", "STU 6 ENTER 7 *", 230},
        {"p6-alg", "STU ALG 2 + 3 ENTER ANS * 4 ENTER SQRT ANS ENTER 7", 0},
        {"p7-mixed", "STU 9 ENTER 16 + ALG ANS / 5 ENTER", 0},
    };
    for (size_t i = 0; i < sizeof S / sizeof S[0]; i++) {
        ab_init(&c); K(&c, S[i].keys); c.rows_dropped = (uint32_t)S[i].drop; mock(S[i].name, &c, false); mock(S[i].name, &c, true);
        static ls027_frame f; screen_ui ui; screen_ui_default(&ui); screen_render(&c, &ui, &f);
        char p[256]; snprintf(p, sizeof p, "%s/%s-today.png", getenv("OUT"), S[i].name); write_png(p, &f);
    }
    return 0;
}
