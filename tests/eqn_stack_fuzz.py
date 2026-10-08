#!/usr/bin/env python3
"""eqn_stack_fuzz: unit 047's census check. Every equation's guarded parse, on a painted host stack (tests/eqn_stack.c),
must take at most the base plus the bytes its counted units cost: a recursion the guard does not count (the census
missed it) shows as stack beyond the line. The shapes are tests/eqn_depth_fuzz.py's: token soup, recursing
structures, the shapes at the limit, in each model.

    python3 tests/eqn_stack_fuzz.py build/eqn_stack SEED N        exit 1 on any equation over the bound
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eqn_depth  # noqa: E402
import eqn_depth_fuzz as shapes  # noqa: E402

DRIVE, N = sys.argv[1], int(sys.argv[3])
shapes.R.seed(int(sys.argv[2]))
cs = shapes.cases(N) + [("STU", "X"), ("35", "X"), ("33", "X")]
out = subprocess.run([DRIVE], input="".join(f"{m} {t}\n" for m, t in cs), capture_output=True, text=True).stdout.split("\n")
rows = [(m, t, int(o.split()[0]), int(o.split()[1])) for (m, t), o in zip(cs, out) if o]
base = max(r[2] for r in rows if r[1] == "X")
PER_UNIT, SLACK = 132, 512           # host bytes a unit (measured 125-129) and the leaf calls' spread
bad, worst = 0, 0.0
for m, t, used, units in rows:
    bound = base + PER_UNIT * min(units, eqn_depth.LIMIT) + SLACK
    worst = max(worst, (used - base) / max(1, min(units, eqn_depth.LIMIT)))
    if used > bound:
        bad += 1
        if bad <= 8:
            print(f"OVER {m} {t!r}: {used} B > {bound} (units {units})")
top = max(rows, key=lambda r: r[2])
print(f"eqn_stack_fuzz: {len(rows)} equations, base {base} B, most {top[2]} B (units {top[3]}), worst {worst:.0f} B a "
      f"unit; {bad} over the bound")
sys.exit(1 if bad or len(rows) != len(cs) else 0)
