/* THROWAWAY: CMPLX per argument, per new level, which levels it reads (perturbation). */
#include <stdio.h>
#include <string.h>
#include "calc.h"
static ab_val R(int v,int d){ ab_val a; memset(&a,0,sizeof a); a.kind=AB_REAL; abn n=abn_from_int(v), q=abn_from_int(d); abn_div(&a.re,&n,&q); return a; }
static ab_calc base,c0,c1;
static void setup(ab_calc *c,const int *v,int d){ *c=base; c->x=R(v[0],d); c->y=R(v[1],d); c->z=R(v[2],d); c->t=R(v[3],d); c->lift=true; }
static ab_val *lv(ab_calc *c,int i){ return i==0?&c->x:i==1?&c->y:i==2?&c->z:&c->t; }
static int same(const ab_val *p,const ab_val *q){ return p->kind==q->kind&&!memcmp(&p->re,&q->re,16)&&!memcmp(&p->im,&q->im,16); }
int main(void){
  ab_init(&base); ab_do_arg(&base,AB_MODEL,AB_MODEL_STU);
  const char *nm[]={"ADD","SUB","MUL","DIV","POW","CHS","INV","LN","EXP","SIN","COS","TAN"};
  int vals[4]={3,5,7,11}, d=4;
  for(int a=0;a<AB_CXP_N;a++){
    setup(&c0,vals,d); abn_status s=ab_do_arg(&c0,AB_CMPLX,a);
    printf("arg %2d %-4s st=%d:",a,a<12?nm[a]:"?",(int)s);
    for(int i=0;i<2;i++){
      printf("  %s reads ", i?"Y(im)":"X(re)");
      for(int L=0;L<4;L++){ int v[4]; memcpy(v,vals,sizeof v); v[L]+=100; setup(&c1,v,d); ab_do_arg(&c1,AB_CMPLX,a);
        putchar(!same(lv(&c0,i),lv(&c1,i))?"XYZT"[L]:'.'); }
    }
    putchar('\n');
  }
}
