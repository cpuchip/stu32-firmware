#!/usr/bin/env python3
"""resources: a unit's cost in each resource (abacus decision 68, unit 052).

    scripts/resources.py static BASE NEW [--targets standin,reva] [--env K=V ...]
        flash (the image, and each library's change), static DIRAM and IRAM, and the internal-RAM symbols that moved
        (nm, 0x3FC88000-0x3FD00000), for two commits; no board. --env sets a build option in both builds: with
        STU_WASM=1, WAMR is fetched into each worktree that has scripts/fetch-wamr.sh (WAMR_REPO from the
        environment, default ../wasm-micro-runtime); the device CMakeLists append device/sdkconfig.wasm
    scripts/resources.py board BASE NEW --port PORT [--targets standin,reva]
        on the stand-in's board: the internal heap free after boot and its largest block, the worst stack through the
        keys (047's deepest site, 048's CAS keys) and the speed benches (device/standin/speed.txt), each commit flashed
        in turn, for the stand-in and for Rev A's main head-less (device/reva/headless.sh, unit 053). Rev A's figure is
        the one a unit reports (unit 059: Rev A's 32 KB instruction cache makes it ~40% faster than the stand-in); the
        board is left on NEW's last target
    scripts/resources.py iram-options [--commit C]
        Rev A built once for each of ESP-IDF's placement options that would move code out of IRAM: the bytes each frees,
        with the option's own help text (what it costs). A measurement for a proposal: nothing is changed.

Each commit is built in a worktree under build/res/ (Espressif's pinned image, as the device scripts do); the casim
clone and the staged games are the main tree's. The tables print as Markdown, for the unit's work file.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
IMAGE = "espressif/idf:v5.4.2"
RES = os.path.join(ROOT, "build", "res")
INTERNAL = (0x3FC88000, 0x3FD00000)
TARGETS = {"standin": ("device/standin", "abacus_standin"), "reva": ("device/reva", "abacus_reva")}


def sh(cmd, cwd=ROOT, check=True, capture=False):
    r = subprocess.run(cmd, cwd=cwd, shell=isinstance(cmd, str), text=True,
                       stdout=subprocess.PIPE if capture else None, stderr=subprocess.STDOUT if capture else None)
    if check and r.returncode != 0:
        sys.exit(f"resources: failed ({r.returncode}): {cmd}\n{r.stdout or ''}")
    return r.stdout if capture else r.returncode


def rev(c):
    return sh(["git", "rev-parse", "--short", c], capture=True).strip()


def worktree(commit):
    """the commit checked out under build/res/<sha>, its games and casim prepared as the main tree's"""
    sha = rev(commit)
    wt = os.path.join(RES, sha)
    if not os.path.isdir(wt):
        os.makedirs(RES, exist_ok=True)
        sh(["git", "worktree", "add", "--detach", wt, sha])
    games = os.path.join(ROOT, "build", "games")
    if os.path.isdir(games) and not os.path.isdir(os.path.join(wt, "build", "games")):
        shutil.copytree(games, os.path.join(wt, "build", "games"), symlinks=True)
    sh(["python3", "scripts/gen_games.py", "games.list", "build/games"], cwd=wt)
    env = dict(os.environ, CASIM_REPO=os.path.join(ROOT, "..", "casim"))
    subprocess.run(["scripts/fetch-casim.sh"], cwd=wt, env=env, check=True)
    return sha, wt


def docker(wt, script, port=None, env=None):
    cmd = ["docker", "run", "--rm", "-v", f"{wt}:/project", "-w", "/project", "--user", f"{os.getuid()}:{os.getgid()}",
           "-e", "HOME=/tmp"]
    for k, v in sorted((env or {}).items()):
        cmd += ["-e", f"{k}={v}"]
    if port:
        cmd += ["--group-add", str(os.stat(port).st_gid), "--device", port, "-e", f"PORT={port}"]
    cmd += [IMAGE, "bash", "-c", ". \"$IDF_PATH/export.sh\" >/dev/null 2>&1\nset -euo pipefail\n" + script]
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def build(wt, target, bdir="build-res", extra_defaults=None, flash_port=None, env=None):
    """the target built in <wt>/<dir>/<bdir> with a fresh sdkconfig; its size JSON and text, and the ELF's nm"""
    d, name = TARGETS[target]
    defaults = "sdkconfig.defaults"
    if env:                                     # (its own build directory: a commit's build differs per option)
        bdir += "-" + "-".join(f"{k}{v}" for k, v in sorted(env.items())).lower()
    wasm = (env or {}).get("STU_WASM") == "1" and os.path.exists(os.path.join(wt, "scripts", "fetch-wamr.sh"))
    if wasm:
        wamr = os.environ.get("WAMR_REPO", os.path.join(ROOT, "..", "wasm-micro-runtime"))
        subprocess.run(["scripts/fetch-wamr.sh"], cwd=wt, env=dict(os.environ, WAMR_REPO=wamr), check=True)
    if extra_defaults:
        with open(os.path.join(wt, d, "res-extra.defaults"), "w") as f:
            f.write(extra_defaults)
        defaults += ";res-extra.defaults"
    done = os.path.join(wt, d, bdir, "res-nm.txt")
    if extra_defaults or flash_port or not os.path.exists(done):   # (a commit's plain build is kept: it can't change)
        shutil.rmtree(os.path.join(wt, d, bdir), ignore_errors=True)
    else:
        b = os.path.join(wt, d, bdir)
        if not os.path.exists(os.path.join(b, "res-calc.txt")):    # (cached before the frame column existed)
            docker(wt, f"xtensa-esp32s3-elf-objdump -d --disassemble=calc_task {d}/{bdir}/{name}.elf | "
                       f"grep -A1 '<calc_task>:' > {d}/{bdir}/res-calc.txt || true")
        return read_build(b, wasm)
    flash = f'idf.py -B {bdir} -DSDKCONFIG={bdir}/sdkconfig -p "$PORT" flash >/dev/null\n' if flash_port else ""
    r = docker(wt, f"""
TARGET=esp32s3 scripts/fetch-intel-dfp.sh >/dev/null
cd {d}
idf.py -B {bdir} -DSDKCONFIG={bdir}/sdkconfig "-DSDKCONFIG_DEFAULTS={defaults}" build >/dev/null
idf.py -B {bdir} -DSDKCONFIG={bdir}/sdkconfig size --format json2 --output-file {bdir}/res-size.json >/dev/null
idf.py -B {bdir} -DSDKCONFIG={bdir}/sdkconfig size-components --format json2 --output-file {bdir}/res-arch.json >/dev/null
idf.py -B {bdir} -DSDKCONFIG={bdir}/sdkconfig size > {bdir}/res-size.txt
xtensa-esp32s3-elf-nm -t d -S {bdir}/{name}.elf > {bdir}/res-nm.txt
xtensa-esp32s3-elf-objdump -d --disassemble=calc_task {bdir}/{name}.elf | grep -A1 "<calc_task>:" > {bdir}/res-calc.txt || true
{flash}""", port=flash_port, env=env)
    if r.returncode != 0:
        sys.exit(f"resources: {target} at {wt} did not build:\n{r.stdout[-3000:]}")
    return read_build(os.path.join(wt, d, bdir), wasm)


def read_build(b, wasm=False):
    if wasm:                                    # an off build must not pass as on (soroban #5527)
        cfg = open(os.path.join(b, "sdkconfig")).read()
        if "CONFIG_WAMR_ENABLE_AOT=y" not in cfg or "CONFIG_WAMR_ENABLE_INTERP=y" in cfg:
            sys.exit(f"resources: {b} was built with STU_WASM=1 but its sdkconfig is not WAMR's AOT-only one")
    size = json.load(open(os.path.join(b, "res-size.json")))
    arch = json.load(open(os.path.join(b, "res-arch.json")))
    bins = [f for f in os.listdir(b) if f.endswith(".bin")]      # the app's image as flashed (padded to 16 B)
    image = os.path.getsize(os.path.join(b, bins[0])) if len(bins) == 1 else None
    region = {l["name"]: l for l in size["layout"]}
    syms = {}
    for line in open(os.path.join(b, "res-nm.txt")):
        p = line.split()
        if len(p) == 4 and INTERNAL[0] <= int(p[0]) < INTERNAL[1]:
            syms[p[3]] = syms.get(p[3], 0) + int(p[1])
    frame = None                                # calc_task's frame: xtensa's prologue, `entry a1, N` (unit 052)
    cf = os.path.join(b, "res-calc.txt")
    if os.path.exists(cf):
        m = re.search(r"entry\s+a1,\s*(0x[0-9a-f]+|\d+)", open(cf).read())
        frame = int(m.group(1), 0) if m else None
    return {"image": image, "region": region, "arch": {k: v["size"] for k, v in arch.items()}, "syms": syms,
            "frame": frame}


def iram_code(r):
    """IRAM code bytes: idf_size's IRAM region is a fixed 16 KB block the linker fills first; the rest of the IRAM code
    lies in the shared SRAM and is booked to DIRAM's .text (unit 052's control: a 1,163 B IRAM_ATTR function left the
    block at 16,383 and added 1,172 to DIRAM). So IRAM code costs internal RAM byte for byte. With a 32 KB instruction
    cache that block is cache, and idf_size lists no IRAM region: all the IRAM code is DIRAM's, the vectors (1,027 B)
    booked there as .vectors (unit 059: without them the same code read 1,027 B smaller)."""
    parts = r["region"]["DIRAM"]["parts"]
    return iram_block(r) + sum(parts.get(k, {}).get("size", 0) for k in (".text", ".vectors"))


def iram_block(r):
    """the 16 KB block's used bytes; 0 when the build has no such block (a 32 KB instruction cache)"""
    return r["region"].get("IRAM", {}).get("used", 0)


def fmt(n):
    return f"{n:,}" if isinstance(n, int) else "?"


def delta(a, b):
    return "?" if a is None or b is None else f"{b - a:+,}"


def static(args):
    shas = []
    for c in (args.base, args.new):
        shas.append(worktree(c))
    env = dict(kv.split("=", 1) for kv in args.env)
    for target in args.targets.split(","):
        (s0, w0), (s1, w1) = shas
        a, b = build(w0, target, env=env), build(w1, target, env=env)
        print(f"\n### {target}: {s0} -> {s1}" + "".join(f", {k}={v}" for k, v in sorted(env.items())) + "\n")
        print("| resource | before | after | change |\n|---|---|---|---|")
        print(f"| flash (the app's .bin) | {fmt(a['image'])} | {fmt(b['image'])} | {delta(a['image'], b['image'])} |")
        ua, ub = a["region"]["DIRAM"]["used"], b["region"]["DIRAM"]["used"]
        print(f"| internal RAM static (DIRAM, of {fmt(b['region']['DIRAM']['total'])}) | {fmt(ua)} | {fmt(ub)} | "
              f"{delta(ua, ub)} |")
        ia, ib = iram_code(a), iram_code(b)
        print(f"| IRAM code (the 16 KB block + DIRAM's .text and .vectors) | {fmt(ia)} | {fmt(ib)} | {delta(ia, ib)} |")
        print(f"| calc_task's frame (under every deep path's stack), B | {fmt(a['frame'])} | {fmt(b['frame'])} | "
              f"{delta(a['frame'], b['frame'])} |")
        libs = sorted(set(a["arch"]) | set(b["arch"]), key=lambda k: -abs(b["arch"].get(k, 0) - a["arch"].get(k, 0)))
        moved = [k for k in libs if b["arch"].get(k, 0) != a["arch"].get(k, 0)]
        print("\nLibraries that changed (all memory types):\n")
        for k in moved[:12] or ["(none)"]:
            if k != "(none)":
                print(f"- {k}: {fmt(a['arch'].get(k, 0))} -> {fmt(b['arch'].get(k, 0))} "
                      f"({delta(a['arch'].get(k, 0), b['arch'].get(k, 0))})")
            else:
                print("- none")
        def grouped(syms):                          # a static's number ($19, $20) is not a change: by name
            g = {}
            for k, v in syms.items():
                n = re.sub(r"\$\d+$", "", k)
                g[n] = g.get(n, 0) + v
            return g
        sa, sb = grouped(a["syms"]), grouped(b["syms"])
        ch = [k for k in sorted(set(sa) | set(sb)) if sa.get(k, 0) != sb.get(k, 0)]
        print(f"\nInternal-RAM symbols (nm, 0x3FC88000-0x3FD00000): {fmt(sum(sa.values()))} -> {fmt(sum(sb.values()))} "
              f"({sum(sb.values()) - sum(sa.values()):+,})" + ("" if ch else ", none changed"))
        for k in ch[:20]:
            print(f"- {k}: {sa.get(k, 0)} -> {sb.get(k, 0)}")


# ---- board ----

STACK = [("the 153-unit mix at 047's deepest site", "m", 9), ("19 levels of SIN( there", "s", 19)]


RUNNERS = {"standin": ("device/standin/run.sh", "build/standin.log", "the stand-in"),
           "reva": ("device/reva/headless.sh", "build/reva-headless.log", "Rev A head-less")}


def talk(wt, port, script_lines, flash, target="standin"):
    """the target (flashed from <wt> when flash) running the script on the board; its log"""
    runner, logf, _ = RUNNERS[target]
    path = os.path.join(wt, "build", "res-script.txt")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(script_lines) + "\n")
    env = dict(os.environ, FLASH="1" if flash else "0", CASIM_REPO=os.path.join(ROOT, "..", "casim"))
    import time
    time.sleep(3)                               # (after a flash the board's port comes back, perhaps renumbered)
    if os.path.exists(os.path.join(wt, logf)):
        os.remove(os.path.join(wt, logf))       # a failed run leaves no log, never the last run's
    subprocess.run([runner, os.path.realpath(port), "build/res-script.txt"], cwd=wt, env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        return open(os.path.join(wt, logf), errors="replace").read()
    except FileNotFoundError:
        return ""


def board(args):
    port = args.port                            # the by-id link, resolved at each use
    gen = os.path.join(ROOT, "build", "stackkeys")
    if not os.path.exists(gen):
        sh(["make", "build/stackkeys"])
    targets = [t for t in args.targets.split(",") if t]
    rows = {t: {} for t in targets}
    for t in targets:
        project = TARGETS[t][1]
        for c in (args.base, args.new):
            sha, wt = worktree(c)
            res = {}

            def run(lines, flash):
                log = talk(wt, port, lines, flash, t)
                names = re.findall(r"Project name:\s+(\S+)", log)
                if not log or (names and names[-1] != project):
                    # 2026-10-10: a failed build left the last image on the board, and it was benched as the new one
                    sys.exit(f"resources: {t} at {sha}: the board ran {names[-1] if names else 'nothing'}, not {project}")
                return log
            log = run(["sleep 3", "wait : heap internal 30", "sleep 2", "send X", "wait state: 10"], True)
            m = re.findall(r"heap=(\d+) stack-free=(\d+)", log)
            res["heap"] = int(m[-1][0]) if m else None
            m = re.findall(r"heap internal \d+ \(largest block (\d+)\)", log)
            res["block"] = int(m[-1]) if m else None
            worst = None
            for label, shape, depth in STACK:
                out = sh([gen, "prog", str(depth), shape], capture=True)
                keys = re.search(r"keys \(entry (\d+), then XEQ A\):(.*)", out)
                mark, ks = int(keys.group(1)), keys.group(2).split()
                lines = ["sleep 3", "wait : heap internal 30", "sleep 2"]
                for i, k in enumerate(ks, 1):
                    lines += [f"send K{k}", "sleep 0.25"]
                    if i == mark:
                        lines += ["sleep 2"]
                lines += ["sleep 20", "send X", "wait state: 10"]
                m = re.findall(r"stack-free=(\d+)", run(lines, False))
                v = int(m[-1]) if m else None
                res[label] = v
                worst = v if worst is None or (v is not None and v < worst) else worst
            res["worst"] = worst
            log = "" if args.no_speed else run(open(os.path.join(ROOT, "device", "standin", "speed.txt")).read().splitlines(), False)
            for key, pat in (("SOLVE", r"eqnbench: SOLVE [^:]*: (\d+) us"), ("integral", r"eqnbench: integral [^:]*: (\d+) us"),
                             ("PROGBENCH SIN", r"progbench: 100 x SIN in a program (\d+) ms"),
                             ("PROGBENCH LN", r"progbench: 100 x LN  in a program (\d+) ms")):
                v = [int(x) for x in re.findall(pat, log)]
                res[key] = min(v) if v else None
            rows[t][sha] = res
    for t in targets:
        (s0, a), (s1, b) = rows[t].items()
        print(f"\n### {RUNNERS[t][2]}, through its console: {s0} -> {s1}\n")
        print("| resource | before | after | change |\n|---|---|---|---|")
        print(f"| internal heap free after boot, B | {fmt(a['heap'])} | {fmt(b['heap'])} | {delta(a['heap'], b['heap'])} |")
        print(f"| largest internal block after boot, B | {fmt(a['block'])} | {fmt(b['block'])} | {delta(a['block'], b['block'])} |")
        for label, _, _ in STACK:
            print(f"| calc task stack free, {label}, B | {fmt(a[label])} | {fmt(b[label])} | {delta(a[label], b[label])} |")
        print(f"| the worst stack free through the keys, B | {fmt(a['worst'])} | {fmt(b['worst'])} | {delta(a['worst'], b['worst'])} |")
        for key, unit in (("SOLVE", "µs"), ("integral", "µs"), ("PROGBENCH SIN", "ms"), ("PROGBENCH LN", "ms")):
            print(f"| {key}, {unit} (best) | {fmt(a[key])} | {fmt(b[key])} | {delta(a[key], b[key])} |")
    print("\n(n/a where a commit lacks the bench: EQNBENCH from 24308a2, OPBENCH from 0594bae; Rev A head-less from 68683b5)")


# ---- iram-options ----

def iram_options(args):
    sha, wt = worktree(args.commit)
    tgt, tdir = args.target, TARGETS[args.target][0]
    base = build(wt, tgt, bdir="build-iram")
    sdk = open(os.path.join(wt, tdir, "build-iram/sdkconfig")).read()
    used0 = iram_block(base)
    iram0 = iram_code(base)
    r = docker(wt, r"""grep -rhE -A14 "^\s*config [A-Z0-9_]*(PLACE_[A-Z_]*FLASH|IN_IRAM|_IRAM_OPT|ISR_IRAM_SAFE|FUNC_IN_IRAM|ROM_IMPL)" \
        $IDF_PATH/components --include="Kconfig*" """)
    helps, cur = {}, None
    for line in r.stdout.splitlines():
        m = re.match(r"\s*config ([A-Z0-9_]+)", line)
        if m:
            cur = m.group(1)
            helps[cur] = []
        elif cur and line.strip() and not re.match(r"\s*(bool|default|depends|select|help|prompt|---|if |endif)", line.strip()):
            helps[cur].append(line.strip())
    cands = []
    for o in sorted(helps):
        on = re.search(rf"^CONFIG_{o}=y$", sdk, re.M)
        off = re.search(rf"^# CONFIG_{o} is not set$", sdk, re.M)
        into_flash = "FLASH" in o or "ROM_IMPL" in o
        if into_flash and off:
            cands.append((o, f"CONFIG_{o}=y\n"))
        elif not into_flash and on:
            cands.append((o, f"# CONFIG_{o} is not set\n"))
    print(f"\n### {tgt} at {sha}: IRAM code {fmt(iram0)} (the 16 KB block {fmt(used0)}), DIRAM "
          f"{fmt(base['region']['DIRAM']['used'])}\n")
    print("| option | set to | IRAM code | IRAM code freed | DIRAM change | the option's help (its cost) |\n|---|---|---|---|---|---|")
    for o, line in cands:
        try:
            b = build(wt, tgt, bdir="build-iram-opt", extra_defaults=line)
        except SystemExit as e:
            print(f"| {o} | {'y' if '=y' in line else 'n'} | did not build | | | {str(e)[:80]} |")
            continue
        dd = b["region"]["DIRAM"]["used"] - base["region"]["DIRAM"]["used"]
        u = iram_code(b)
        print(f"| {o} | {'y' if '=y' in line else 'n'} | {fmt(u)} | {iram0 - u:,} | {dd:+,} | "
              f"{' '.join(helps.get(o, []))[:300]} |", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("static")
    s.add_argument("base")
    s.add_argument("new")
    s.add_argument("--targets", default="standin,reva")
    s.add_argument("--env", action="append", default=[], metavar="K=V", help="a build option, in both builds")
    b = sp.add_parser("board")
    b.add_argument("base")
    b.add_argument("new")
    b.add_argument("--port", required=True)
    b.add_argument("--no-speed", action="store_true", help="the heap and the stack only")
    b.add_argument("--targets", default="standin,reva", help="standin, reva (Rev A's main head-less) or both")
    i = sp.add_parser("iram-options")
    i.add_argument("--commit", default="HEAD")
    i.add_argument("--target", default="reva", choices=sorted(TARGETS))
    a = ap.parse_args()
    {"static": static, "board": board, "iram-options": iram_options}[a.cmd](a)


if __name__ == "__main__":
    main()
