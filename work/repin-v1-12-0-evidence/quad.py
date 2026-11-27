# Independent check of casim 1.12.0's exact defint answers (CAS 045) as the firmware prints them: composite Simpson
# (200,000 panels) in floats over the integrand as typed (LN(EXP(u)) and LOG(ALOG(u)) computed as typed, not
# simplified), against each printed form evaluated by hand. Angle modes scale the argument; K is read at 1.37.
import math
pi=math.pi
def simpson(f,a,b,n=200000):
    h=(b-a)/n; s=f(a)+f(b)
    for i in range(1,n): s+= (4 if i%2 else 2)*f(a+i*h)
    return s*h/3
D=pi/180; G=pi/200
K=1.37
ln,ex,sin,cos,sq=math.log,math.exp,math.sin,math.cos,math.sqrt
log10=math.log10
cases=[
 ("R01 COS(LN(EXP(X+1))) 1..2", lambda x:cos(ln(ex(x+1))), 1, 2, -sin(2)+sin(3)),
 ("R02 COS(LN(EXP(X+1))) -1..1", lambda x:cos(ln(ex(x+1))), -1, 1, sin(2)),
 ("R03 DEG COS(LN(EXP(X+1))) 1..2", lambda x:cos(ln(ex(x+1))*D), 1, 2, -180*sin(2*D)/pi+180*sin(3*D)/pi),
 ("R04 LN(EXP(SIN(X))) 0..2", lambda x:ln(ex(sin(x))), 0, 2, -cos(2)+1),
 ("R05 LN(EXP(SIN(X))) 2..0", lambda x:ln(ex(sin(x))), 2, 0, cos(2)-1),
 ("R06 DEG LN(EXP(SIN(X))) 0..90", lambda x:ln(ex(sin(x*D))), 0, 90, 180/pi),
 ("R07 GRAD LN(EXP(SIN(X))) 0..100", lambda x:ln(ex(sin(x*G))), 0, 100, 200/pi),
 ("R08 LN(EXP(SIN(X)))^3 0..pi", lambda x:ln(ex(sin(x)))**3, 0, pi, 4/3),
 ("R09 LOG(ALOG(SIN(X))) 0..pi", lambda x:log10(10**sin(x)), 0, pi, 2),
 ("R10 SIN(X)*LN(EXP(COS(X))) 0..pi/2", lambda x:sin(x)*ln(ex(cos(x))), 0, pi/2, 1/2),
 ("R11 LN(EXP(COS(X+1))) -1..0.5", lambda x:ln(ex(cos(x+1))), -1, 0.5, sin(1.5)),
 ("L01 LN(EXP(K))*SIN(X) 0..1 (K=1.37)", lambda x:ln(ex(K))*sin(x), 0, 1, K-K*cos(1)),
 ("L02 SQRT(EXP(K))^2*SIN(X) 0..1", lambda x:sq(ex(K))**2*sin(x), 0, 1, ex(K)-ex(K)*cos(1)),
 ("L03 LN(EXP(K))*X 0..1", lambda x:ln(ex(K))*x, 0, 1, K/2),
]
bad=0
for name,f,a,b,v in cases:
    q=simpson(f,a,b); d=abs(q-v)
    ok=d<1e-10*max(1,abs(v))
    bad+= not ok
    print(f"{name:36s} quad {q:.15g}  form {v:.15g}  diff {d:.1e}  {'ok' if ok else 'MISMATCH'}")
print("mismatches", bad)
