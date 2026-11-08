import sys, subprocess, os, time, re
S=sys.argv[1]
M={
 'M1':('core/calc.c',"    bool onx = !c->eqn_mode && ab_is_sym(&c->x);   /* unit 088: the CAS on the stack */","    bool onx = false;"),
 'M2':('core/calc.c',"            c->lastx = c->x;\n            c->x = v;\n            c->lift = true;\n            return ABN_OK;\n        }\n        abn_status st = eqn_insert(c, out);","            c->x = v;\n            c->lift = true;\n            return ABN_OK;\n        }\n        abn_status st = eqn_insert(c, out);"),
 'M3':('core/calc.c',"            c->lastx = c->x;\n            c->x = v;\n            c->lift = true;\n            return ABN_OK;\n        }\n        abn_status st = eqn_insert(c, out);","            c->lastx = c->x;\n            c->t = c->z; c->z = c->y; c->y = c->x;\n            c->x = v;\n            c->lift = true;\n            return ABN_OK;\n        }\n        abn_status st = eqn_insert(c, out);"),
 'M4':('firmware/app.c',"    if (a->c->stu && !a->c->prgm && !a->c->entry.active && ab_is_sym(&a->c->x))","    if (a->c->stu && !a->c->prgm && !a->c->entry.active)"),
 'M5':('core/calc.c',"static bool cas_plain_number(const char *t)\n{","static bool cas_plain_number(const char *t)\n{\n    for (const char *q = t; *q; q++) if (*q >= 'A' && *q <= 'Z') return false;\n    return true;"),
 'M6':('core/calc.c',"static bool cas_plain_number(const char *t)\n{","static bool cas_plain_number(const char *t)\n{\n    if (t) return false;"),
 'M7':('firmware/app.c',"    if (a->c->stu && !a->c->prgm && !a->c->entry.active && ab_is_sym(&a->c->x))","    if (a->c->stu && !a->c->prgm && ab_is_sym(&a->c->x))"),
 'M8':('firmware/app.c',"                      (c->eqn_mode || (op == AB_CASSOLVE && ab_is_sym(&c->x))));","                      c->eqn_mode);"),
 'M9':('core/calc.c',"        return ab_is_sym(&c->x) ? eqn_store_x(c) : ABN_NO_EQN;","        return ABN_NO_EQN;"),
}
env=dict(os.environ, GAMES_REPOS=S+'/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
def run(cmd): return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env)
for name in sys.argv[2:]:
    f,o,n=M[name]; orig=open(S+'/088/'+os.path.basename(f)+'.orig').read()
    assert orig.count(o)==1, name
    open(f,'w').write(orig.replace(o,n))
    t0=time.time(); b=run('make build/vectors build/app_test build/expr_test 2>&1')
    need=['build/app_test'] if f=='firmware/app.c' else ['build/vectors','build/app_test','build/expr_test']
    ok=all(os.path.getmtime(x)>=t0-1 for x in need)
    red=[]
    if not ok: red.append('BUILD FAILED: '+'|'.join(l for l in b.stdout.split('\n') if 'error' in l)[:200])
    else:
        v=run('./build/vectors tests/088-soroban.txt'); red += sorted(set(re.findall(r'^FAIL (\w+):',v.stdout,re.M)))
        v=run('./build/vectors tests/048-soroban.txt'); r48=sorted(set(re.findall(r'^FAIL (\w+):',v.stdout,re.M)))
        if r48: red.append('048:'+','.join(r48))
        a=run('./build/app_test'); out=(a.stdout+a.stderr).split('\n')
        summ=[l for l in out if re.match(r'app_test: \d+/\d+ checks',l)]
        m=re.match(r'app_test: (\d+)/(\d+)',summ[-1]) if summ else None
        fa=[] if (m and m.group(1)==m.group(2)) else [l for l in out if l.startswith('app_test:') and l not in summ] or ['no summary']
        if fa: red.append(f'app_test {len(fa)} fail: '+' || '.join(x[:90] for x in fa[:4]))
        e=run('./build/expr_test'); fe=[l for l in (e.stdout+e.stderr).split('\n') if 'FAIL' in l]
        if fe: red.append(f'expr_test {len(fe)} fail: '+' || '.join(x[:90] for x in fe[:3]))
    print(name, 'RED:' if red else 'SURVIVED', ' '.join(red), flush=True)
    if not ok: print(b.stdout[-1500:])
    open(f,'w').write(orig)
print('restored', open('core/calc.c').read()==open(S+'/088/calc.c.orig').read() and open('firmware/app.c').read()==open(S+'/088/app.c.orig').read())
