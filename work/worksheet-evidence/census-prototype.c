/* THROWAWAY prototype (worksheet rows): for every op, which stack levels does it READ (perturbation), and which levels
   hold a NEW value after it (content differs from the level's value before at the same position, or moved). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "calc.h"
#include "opnames.h"
static int DIVV=1; static ab_val R(int v){ ab_val a; memset(&a,0,sizeof a); a.kind=AB_REAL; abn n=abn_from_int(v), d=abn_from_int(DIVV); abn_div(&a.re,&n,&d); return a; }
static int same(const ab_calc *a,const ab_val *p,const ab_calc *b,const ab_val *q){
  if(p->kind!=q->kind||p->len!=q->len) return 0;
  if(p->kind==AB_EXPR) return !strcmp(a->exprs[p->len],b->exprs[q->len]);
  return !memcmp(&p->re,&q->re,sizeof p->re)&&!memcmp(&p->im,&q->im,sizeof p->im)&&!memcmp(&p->p3,&q->p3,sizeof p->p3);
}
static ab_calc base,c0,c1,c2;
static void setup(ab_calc *c,int t,int z,int y,int x){ *c=base; c->t=R(t); c->z=R(z); c->y=R(y); c->x=R(x); c->lift=true; }
static ab_val *lv(ab_calc *c,int i){ return i==0?&c->x:i==1?&c->y:i==2?&c->z:&c->t; }
int main(int argc,char**argv){
  int X0=argc>1?atoi(argv[1]):3; int div=argc>2?atoi(argv[2]):1;
  DIVV=div; ab_init(&base); ab_do_arg(&base,AB_MODEL,AB_MODEL_STU);
  int vals[4]={X0,5,7,11};
  printf("# x=%d y=5 z=7 t=11, STU, RPN; reads: levels whose +100 changes a new value; new: levels not equal to any level before\n",X0);
  for(int op=0;op<=AB_OP_LAST;op++){
    setup(&c0,vals[3],vals[2],vals[1],vals[0]);
    abn_status s=ab_do_arg(&c0,(ab_op)op,0);
    /* new levels: after-level value equal to no before-level value and not a pure copy */
    ab_calc pre; setup(&pre,vals[3],vals[2],vals[1],vals[0]);
    int nw[4]={0}, any=0;
    for(int i=0;i<4;i++){ int found=0; for(int j=0;j<4;j++) if(same(&c0,lv(&c0,i),&pre,lv(&pre,j))) found=1; nw[i]=!found; any|=nw[i]; }
    int rd[4]={0};
    for(int L=0;L<4;L++){
      int v[4]; memcpy(v,vals,sizeof v); v[L]+=100;
      setup(&c1,v[3],v[2],v[1],v[0]); ab_do_arg(&c1,(ab_op)op,0);
      for(int i=0;i<4;i++) if(nw[i] && !same(&c0,lv(&c0,i),&c1,lv(&c1,i))) rd[L]=1;
    }
    printf("%-10s st=%d new=%c%c%c%c reads=%c%c%c%c\n", op<NOPNAMES?opname[op]:"?", (int)s,
      nw[0]?'X':'.',nw[1]?'Y':'.',nw[2]?'Z':'.',nw[3]?'T':'.', rd[0]?'X':'.',rd[1]?'Y':'.',rd[2]?'Z':'.',rd[3]?'T':'.');
  }
}
