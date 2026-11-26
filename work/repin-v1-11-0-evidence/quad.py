# Independent check of casim 1.11.0's exact defint answers: composite Simpson (200,000 panels) in floats,
# against each printed form evaluated by hand. Angle modes scale the argument.
import math
pi=math.pi
def simpson(f,a,b,n=200000):
    h=(b-a)/n; s=f(a)+f(b)
    for i in range(1,n): s+= (4 if i%2 else 2)*f(a+i*h)
    return s*h/3
D=pi/180; G=pi/200
A=1.7  # a value for the letter A
cases=[
 ("R01 SIN^2 0..pi", lambda x:math.sin(x)**2, 0, pi, pi/2),
 ("R02 COS^4 0..pi/2", lambda x:math.cos(x)**4, 0, pi/2, 3*pi/16),
 ("R03 SIN^3 0..pi", lambda x:math.sin(x)**3, 0, pi, 4/3),
 ("R04 SIN^5 0..pi/3", lambda x:math.sin(x)**5, 0, pi/3, 53/480),
 ("R05 SIN*COS 0..pi/2", lambda x:math.sin(x)*math.cos(x), 0, pi/2, 1/2),
 ("R06 SIN^2 pi..0", lambda x:math.sin(x)**2, pi, 0, -pi/2),
 ("R07 SIN^2 0..1", lambda x:math.sin(x)**2, 0, 1, -math.sin(2)/4+1/2),
 ("R08 DEG SIN^2 0..45", lambda x:math.sin(x*D)**2, 0, 45, -45/pi+45/2),
 ("R09 GRAD SIN^2 0..200", lambda x:math.sin(x*G)**2, 0, 200, 100),
 ("R10 A*SIN^2 0..pi (A=1.7)", lambda x:A*math.sin(x)**2, 0, pi, A*pi/2),
 ("R11 SIN^3 -1..1", lambda x:math.sin(x)**3, -1, 1, math.cos(-1)-math.cos(1)-math.cos(-1)**3/3+math.cos(1)**3/3),
 ("R12 SIN*SIN3 0..pi", lambda x:math.sin(x)*math.sin(3*x), 0, pi, 0),
]
bad=0
for name,f,a,b,v in cases:
    q=simpson(f,a,b); d=abs(q-v)
    ok=d<1e-10*max(1,abs(v))
    bad+= not ok
    print(f"{name:28s} quad {q:.15g}  form {v:.15g}  diff {d:.1e}  {'ok' if ok else 'MISMATCH'}")
print("mismatches", bad)
