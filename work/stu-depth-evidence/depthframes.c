/* THROWAWAY (stu-depth blueprint's frames, abacus #14992): STU's home from the prototype (prototype.diff), (A) and (B).
   Never committed as src; kept in work/stu-depth-evidence/ so the PNGs can be remade. */
#include <stdio.h>
#include <string.h>
#include "calc.h"
#include "screen.h"
#include "ls027.h"
#include "state.h"
extern int proto_mode_b;
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
static void K(ab_calc *c, const char *s)
{
    char buf[512]; snprintf(buf, sizeof buf, "%s", s);
    for (char *t = strtok(buf, " "); t; t = strtok(NULL, " ")) {
        static const struct { const char *n; int op; } M[] = {{"ENTER",AB_ENTER},{"+",AB_ADD},{"-",AB_SUB},{"*",AB_MUL},
            {"/",AB_DIV},{"SQRT",AB_SQRT},{"SQ",AB_SQ},{".",AB_KPOINT},{"XY",AB_XY},{"RDN",AB_RDN},{"CLSTK",AB_CLSTK}};
        if (!strcmp(t, "STU")) { ab_do_arg(c, AB_MODEL, AB_MODEL_STU); continue; }
        if (t[0] >= '0' && t[0] <= '9') { for (char *d = t; *d; d++) ab_do(c, (ab_op)(AB_K0 + *d - '0')); continue; }
        size_t i; for (i = 0; i < sizeof M / sizeof M[0]; i++) if (!strcmp(M[i].n, t)) { ab_do(c, (ab_op)M[i].op); break; }
        if (i == sizeof M / sizeof M[0]) { fprintf(stderr, "unknown key %s\n", t); }
    }
}
static ab_calc c;
int main(int argc, char **argv)
{
    static const struct { const char *name; const char *keys; } S[] = {
        {"d1-seven-live", "STU 2 SQRT ENTER 3 * 5 ENTER 6 ENTER 7 ENTER 8 ENTER 9 ENTER 10"},
        {"d2-sixteen-live", "STU 1 ENTER 2 ENTER 3 ENTER 4 ENTER 5 ENTER 6 ENTER 7 ENTER 8 ENTER 9 ENTER 10 ENTER 11 ENTER 12 ENTER 13 ENTER 14 ENTER 15 ENTER 16"},
        {"d0-four-live", "STU 2 SQRT ENTER 3 * 5 ENTER 6 ENTER 7"},
    };
    const char *dir = argc > 1 ? argv[1] : ".";
    for (int m = 0; m < 2; m++)
        for (size_t i = 0; i < sizeof S / sizeof S[0]; i++) {
            static ls027_frame f; screen_ui ui; static screen_page pg; char path[512];
            proto_mode_b = m;
            ab_init(&c); K(&c, S[i].keys);
            screen_ui_default(&ui); screen_render(&c, &ui, &f); screen_lines(&c, &ui, &pg);
            snprintf(path, sizeof path, "%s/%s-%c.png", dir, S[i].name, m ? 'B' : 'A');
            write_png(path, &f);
            printf("%s-%c: keys \"%s\"; levels %s, page lines %d\n", S[i].name, m ? 'B' : 'A', S[i].keys, pg.levels, pg.page_n);
        }
    return 0;
}
