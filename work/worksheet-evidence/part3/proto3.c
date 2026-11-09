/* THROWAWAY (part 3 blueprint's prototype): selection drawn over today's page. Never committed as code. */
#include "screen.c"
#include <stdlib.h>
static void be32(uint8_t *p, uint32_t v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static int png_chunk(FILE *o, const char *type, const uint8_t *data, size_t n)
{ static uint8_t buf[4 + 16384]; uint8_t len[4], crc[4]; memcpy(buf, type, 4); if (n) memcpy(buf + 4, data, n);
  be32(len, (uint32_t)n); be32(crc, ab_crc32(buf, 4 + n)); return fwrite(len,1,4,o)==4 && fwrite(buf,1,4+n,o)==4+n && fwrite(crc,1,4,o)==4; }
static void write_png(const char *path, const ls027_frame *f)
{ enum { ROW = 1 + LS027_ROW_BYTES, RAW = ROW * LS027_H }; static uint8_t z[2 + 5 + RAW + 4]; uint8_t hdr[13] = {0}; uint32_t a = 1, b = 0;
  be32(hdr, LS027_W); be32(hdr + 4, LS027_H); hdr[8] = 1; z[0]=0x78; z[1]=1; z[2]=1; z[3]=RAW&0xFF; z[4]=RAW>>8; z[5]=~z[3]; z[6]=~z[4];
  for (int y = 0; y < LS027_H; y++) { uint8_t *r = z + 7 + y*ROW; r[0]=0; for (int i = 0; i < LS027_ROW_BYTES; i++) { uint8_t v=f->px[y][i], o=0;
    for (int k=0;k<8;k++) o |= (uint8_t)((~v>>k&1)<<(7-k)); r[1+i]=o; } }
  for (int i = 0; i < RAW; i++) { a=(a+z[7+i])%65521; b=(b+a)%65521; } be32(z + 7 + RAW, b << 16 | a);
  FILE *o = fopen(path, "wb"); fwrite("\x89PNG\r\n\x1a\n",1,8,o); png_chunk(o,"IHDR",hdr,13); png_chunk(o,"IDAT",z,sizeof z); png_chunk(o,"IEND",NULL,0); fclose(o); }
static void invert(ls027_frame *f, int y0, int y1) { for (int y = y0; y < y1; y++) for (int i = 0; i < LS027_ROW_BYTES; i++) f->px[y][i] ^= 0xFF; }
static void dot(ls027_frame *f, int bot) { screen_fill(f, 4, bot - 9, 4, 4, true); }
/* the page line i's bottom, and which page line holds serial s (-1 none) */
static int line_of(const ab_calc *c, uint32_t s) { for (int i = 0; i < comp.pg_n; i++) if (comp.pg[i].part != 3 && rd32(c->rows + comp.pg[i].at + 2) == s && comp.pg[i].part != 1) return i; return -1; }
static void mock(const char *name, ab_calc *c, int sel_line, const char **soft)
{
  static ls027_frame f; screen_ui ui; screen_ui_default(&ui);
  for (int i = 0; i < 6; i++) ui.soft[i] = soft ? soft[i] : NULL;
  screen_render(c, &ui, &f);
  if (sel_line >= 0 && sel_line < comp.pg_n) {
    int bot = comp.pg_bot0 - sel_line * PG_PITCH;
    invert(&f, bot - 17, bot + 2);
    ab_row r; ab_rows_parse(c->rows + comp.pg[sel_line].at, c->rows_len - comp.pg[sel_line].at, &r);
    for (int k = 0; k < r.nlink; k++) { int li = line_of(c, r.link[k]); if (li >= 0) dot(&f, comp.pg_bot0 - li * PG_PITCH);
      for (int lv = 0; lv < comp.levels; lv++) if (comp.lvl_at[lv] != UINT32_MAX && rd32(c->rows + comp.lvl_at[lv] + 2) == r.link[k]) dot(&f, comp.lvl_bot[lv]); }
    screen_text(&f, 300, 4, 2, "SEL", true);
  }
  char p[256]; snprintf(p, sizeof p, "%s/%s.png", getenv("OUT"), name); write_png(p, &f);
}
static void K(ab_calc *c, const char *s) { char buf[512]; snprintf(buf, sizeof buf, "%s", s);
  for (char *t = strtok(buf, " "); t; t = strtok(NULL, " ")) {
    static const struct { const char *n; int op; } M[] = {{"ENTER",AB_ENTER},{"+",AB_ADD},{"-",AB_SUB},{"*",AB_MUL},{"/",AB_DIV},{"SQRT",AB_SQRT},{"SQ",AB_SQ}};
    if (!strcmp(t, "STU")) { ab_do_arg(c, AB_MODEL, AB_MODEL_STU); continue; }
    if (t[0] >= '0' && t[0] <= '9') { for (char *d = t; *d; d++) ab_do(c, (ab_op)(AB_K0 + *d - '0')); continue; }
    size_t i; for (i = 0; i < sizeof M / sizeof M[0]; i++) if (!strcmp(M[i].n, t)) { ab_do(c, (ab_op)M[i].op); break; } } }
int main(void)
{
  static ab_calc c;
  static const char *num[6] = {"COPY", "STO", "SHOW", "\xe2\x86\x92" "FRAC", "", "INFO"};
  ab_init(&c); K(&c, "STU 3 ENTER 4 * 5 + 2 / SQRT");
  mock("q1-home", &c, -1, NULL);
  mock("q2-select-newest-op", &c, 0, num);       /* ÷ 8.5 selected: its links 17 and 2 marked */
  mock("q3-select-older", &c, 2, num);           /* + 17 selected: links 12 and 5 */
  return 0;
}
