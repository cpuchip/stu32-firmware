"""THROWAWAY: the library's erf and erfc against mpmath (60 digits), the error in units of the result's 34th digit."""
import subprocess, sys, random
from mpmath import mp, mpf, erf, erfc, floor, log10, fabs
mp.dps = 60
R = random.Random(95)
xs = [f"{i / 100:.2f}" for i in range(-600, 601)]                      # the centre, every 0.01
xs += [f"{R.uniform(-10, 10):.33f}" for _ in range(2000)]                 # 34-digit arguments
xs += [str(t) for t in range(6, 107)] + [f"{t}.5" for t in range(6, 106)]   # erfc's far tail (to about 1E-4900)
xs += [f"1E-{k}" for k in range(1, 60)] + [f"-1E-{k}" for k in range(1, 60)]
out = subprocess.run([sys.argv[1]], input='\n'.join(xs), capture_output=True, text=True).stdout.split('\n')
def ulps(res, tv):
    r = mpf(res.replace('E', 'e'))
    if tv == 0: return 0 if r == 0 else mpf('inf')
    return fabs(r - tv) / mpf(10) ** (floor(log10(fabs(tv))) - 33)
worst = {'erf': (0, None), 'erfc': (0, None)}
hist = {'erf': {}, 'erfc': {}}
for line in out:
    p = line.split()
    if len(p) != 5: continue
    x = mpf(p[0].replace('E', 'e'))
    for name, res, f in (('erf', p[1], erf), ('erfc', p[2], erfc)):
        u = ulps(res, f(x))
        b = '0' if u < mpf('0.5000001') else '<1' if u < 1 else '<2' if u < 2 else '<10' if u < 10 else '>=10'
        hist[name][b] = hist[name].get(b, 0) + 1
        if u > worst[name][0]: worst[name] = (u, (p[0], res))
for n in hist:
    print(n, 'errors in units of the 34th digit:', hist[n], ' worst %.3g at x=%s (%s)' % (float(worst[n][0]), *worst[n][1]))
