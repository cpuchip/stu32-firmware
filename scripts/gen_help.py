#!/usr/bin/env python3
"""gen_help.py (unit 075): HELP's text, help/help.txt, into firmware/help_text.c.

    python3 scripts/gen_help.py            write firmware/help_text.c
    python3 scripts/gen_help.py --check    exit 1 if firmware/help_text.c is not what help/help.txt gives

help/help.txt: '#' starts a comment line. A line 'MODE: key key ...' names entries whose text must differ by mode
(the code branches on the mode there; tests/help_test.c checks each has a variant). An entry is a header line, then its
text lines:

    [AB_SIN]            an op (KM_OP, KM_PROMPT, KM_CONFIRM) with any argument
    [AB_TEST:3]         the op with that argument (tried before the plain op); the argument is C (a number or a name)
    [menu:CAS]          a menu's key
    [dev:UNDO]          a device action (KM_DEV_UNDO)
    [catkey:C]          a key MEM's catalog takes before its own action: C, CLEAR, BACK (the back-arrow), ENTER
    [app] [const]       a paged menu's soft key: an app (APPS, GAMES), a constant (CONST)
    [soft]              a soft key with nothing under it
    [undefined]         a legend with no meaning yet
    [more]              a paged menu's sixth key (CONST, APPS, GAMES)
    [graphpad]          a digit while a graph is open (it moves the cursor, GOLD ten pixels)
    [cmplxpair]         an operator while 33s mode's CMPLX prefix waits (the pair operation)

A text line may start with a mode prefix, 33: 35: STU: or a list (33,35:). In a mode where any line carries its
prefix, only those lines show; elsewhere the plain lines show. {} in a line is the pressed legend. The C names
(AB_..., KM_DEV_...) are emitted as they are written, so an entry for an op or a device action that no longer exists
fails the build; tests/help_test.c checks the rest: every reachable action has text, every entry is reachable, every
line fits, and the MODE entries vary.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "help", "help.txt")
OUT = os.path.join(ROOT, "firmware", "help_text.c")
PREFIX = re.compile(r"^((?:33|35|STU)(?:,(?:33|35|STU))*):\s?(.*)$")


def fail(msg):
    sys.exit(f"gen_help: {msg}")


def cstr(s):
    out = []
    for ch in s:
        if ch in '\\"':
            out.append("\\" + ch)
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\x1f":
            out.append("\\037")              # a menu's name ends here (help.h)
        elif ord(ch) < 0x20:
            fail(f"a control character in {s!r}")
        else:
            out.append(ch)
    b = "".join(out).encode("utf-8")
    text, i = [], 0
    while i < len(b):                       # non-ASCII as octal escapes: no hex escape eats a following digit
        c = b[i]
        text.append(chr(c) if c < 0x80 else "\\%03o" % c)
        i += 1
    return '"' + "".join(text) + '"'


def parse(lines):
    entries, modes, cur = [], [], None
    for n, raw in enumerate(lines, 1):
        line = raw.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line.startswith("MODE:"):
            modes += line[5:].split()
            continue
        m = re.match(r"^\[([^\]]+)\]\s*$", line)
        if m:
            cur = {"key": m.group(1).strip(), "lines": [], "at": n}
            entries.append(cur)
            continue
        if cur is None:
            fail(f"line {n}: text before any entry")
        p = PREFIX.match(line)
        if p and not p.group(2).strip():
            fail(f"line {n}: a mode prefix with no text")
        cur["lines"].append(line)
    for e in entries:
        if not e["lines"]:
            fail(f"line {e['at']}: [{e['key']}] has no text")
    keys = [e["key"] for e in entries]
    dup = {k for k in keys if keys.count(k) > 1}
    if dup:
        fail(f"entries twice: {sorted(dup)}")
    for k in modes:
        if k not in keys:
            fail(f"MODE names [{k}], which has no entry")
    return entries, modes


def c_entry(e):
    k = e["key"]
    text = cstr("\n".join(e["lines"]))
    if k.startswith("AB_"):
        op, _, arg = k.partition(":")
        return f"{{HELP_OP, {op}, {arg if arg else '-1'}, {text}}}"
    if k.startswith("menu:"):
        return f"{{HELP_MENU, 0, -1, {cstr(k[5:] + chr(0x1f) + chr(10).join(e['lines']))}}}"
    if k.startswith("dev:"):
        return f"{{HELP_DEV, KM_DEV_{k[4:]}, -1, {text}}}"
    if k.startswith("catkey:"):
        cats = {"C": "HELP_CAT_C", "CLEAR": "HELP_CAT_CLEAR", "BACK": "HELP_CAT_BACK", "ENTER": "HELP_CAT_ENTER"}
        if k[7:] not in cats:
            fail(f"[{k}]: not a catalog key (C, CLEAR, BACK, ENTER)")
        return f"{{HELP_CATKEY, {cats[k[7:]]}, -1, {text}}}"
    special = {"app": "HELP_APP", "const": "HELP_CONST", "soft": "HELP_SOFT", "undefined": "HELP_UNDEF", "more": "HELP_MORE", "graphpad": "HELP_GRAPHPAD", "cmplxpair": "HELP_CMPLXPAIR"}
    if k in special:
        return f"{{{special[k]}, 0, -1, {text}}}"
    fail(f"[{k}]: not a key this file knows")


def generate():
    with open(SRC, encoding="utf-8") as f:
        entries, modes = parse(f.readlines())
    idx = {e["key"]: i for i, e in enumerate(entries)}
    out = ["/* Generated by scripts/gen_help.py from help/help.txt (unit 075): edit that file, not this one. */", "#include <stddef.h>",
           '#include "help.h"', '#include "calc.h"', '#include "keymap.h"', "",
           "const help_entry help_table[] = {"]
    out += [f"    {c_entry(e)}," for e in entries]
    out += ["};", f"const int help_table_n = {len(entries)};", "",
            "/* the entries whose text must differ by mode (help.txt's MODE lines) */",
            "const int help_mode_list[] = {" + ", ".join(str(idx[k]) for k in modes) + (", " if modes else "") + "-1};",
            ""]
    return "\n".join(out)


def main():
    text = generate()
    if "--check" in sys.argv[1:]:
        old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if old != text:
            fail("firmware/help_text.c is not what help/help.txt gives: run scripts/gen_help.py")
        print("gen_help: firmware/help_text.c matches help/help.txt")
        return
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"gen_help: {OUT}")


if __name__ == "__main__":
    main()
