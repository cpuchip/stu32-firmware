import subprocess, sys, random
from mpmath import mp, mpf, ncdf, erfc, sqrt, floor, log10, fabs
mp.dps = 80
R = random.Random(95)
zs = [f"{i / 100:.2f}" for i in range(-800, 801)] + [f"{R.uniform(-8, 8):.30f}" for _ in range(1500)] + [str(t) for t in range(9, 40)]
out = subprocess.run([sys.argv[1]], input='\n'.join(zs), capture_output=True, text=True).stdout.split('\n')
def ulps(res, tv):
    r = mpf(res.replace('E', 'e'))
    return fabs(r - tv) / mpf(10) ** (floor(log10(fabs(tv))) - 33) if tv else 0
for name, col, f in (('PHI', 1, lambda z: erfc(-z / sqrt(2)) / 2), ('Q', 2, lambda z: erfc(z / sqrt(2)) / 2)):
    h, worst = {}, (0, None)
    for line in out:
        p = line.split()
        if len(p) != 3: continue
        z = mpf(p[0]); tv = f(z)
        u = ulps(p[col], tv)
        b = '<1' if u < 1 else '<10' if u < 10 else '<100' if u < 100 else '<1e3' if u < 1000 else '>=1e3'
        h[b] = h.get(b, 0) + 1
        if u > worst[0]: worst = (u, p[0])
    print(name, 'errors in units of the 34th digit:', h, ' worst %.3g at z=%s' % (float(worst[0]), worst[1]))
