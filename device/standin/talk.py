#!/usr/bin/env python3
"""Talks to the stand-in over its console (unit 018).

    talk.py PORT LOG [SCRIPT]

Copies every line the board prints to stdout and LOG. With a SCRIPT, runs its lines in order:

    send TEXT              send TEXT and a newline (the firmware reads K<n> as key n, and more)
    wait TEXT [SECONDS]    wait for a printed line containing TEXT (default 10 s); fail if none
    expect TEXT [SECONDS]  the same, named for a check: a miss is reported as FAIL
    expect-min KEY MIN [SECONDS]
                           the next printed KEY=<integer> must be at least MIN: a miss or a smaller
                           value is reported as FAIL (unit 055: stack-free after a wasm trap)
    expect-range KEY MIN MAX [SECONDS]
                           the same, between MIN and MAX inclusive (unit 061)
    expect-grows KEY [SECONDS]
                           the next printed KEY=<integer> must be larger than the last one printed
                           before this line (unit 061: the ULP's edge count climbing)
    expect-none TEXT SECONDS
                           no printed line may contain TEXT for SECONDS: one that does is a FAIL
                           (unit 061: a key bounce, a ghost)
    say TEXT               print TEXT for the person at the board, marked >>> (unit 061)
    confirm TEXT           ask the person at the board TEXT, answered y or n on this program's stdin;
                           n or no answer is reported as FAIL (unit 061: what a log cannot judge)
    reset                  pulse EN through the board's auto-reset (RTS, with DTR held low)
    press0 MS              hold IO0 low for MS ms through the auto-reset's DTR line (RTS high), as
                           pressing BOOT: a key for the light-sleep probe (unit 023)
    sleep SECONDS
    # ...                  a comment

Exits 0 when every wait and expect was met. Without a SCRIPT it relays stdin lines to the board
until EOF. Needs pyserial (present in the ESP-IDF environment, and installed with esptool). DTR and
RTS are held low on open so opening the port neither resets the chip nor enters its bootloader.

If the port goes away (a USB-Serial-JTAG console re-enumerates when the chip resets or sleeps, unit
061), it is opened again by the name given until it comes back, and the log says "talk.py: port open
again", which a script can wait for: on Linux give a by-id path, which follows the device to its new
number. Runs on Windows too (a COM name; the log is UTF-8)."""
import os
import re
import sys
import threading
import time

import serial

port, log = sys.argv[1], sys.argv[2]
script = sys.argv[3] if len(sys.argv) > 3 else None


def open_port():
    p = serial.Serial()
    p.port, p.baudrate, p.timeout = os.path.realpath(port) if os.name == "posix" else port, 115200, 0.2
    p.write_timeout = 2         # a console that stops reading fills the S3's USB FIFO: a failed send, not a hang (061)
    p.dtr = p.rts = False
    p.open()
    return p


sys.stdout.reconfigure(errors="replace")    # (a Windows console's code page may lack a legend's character)
s = open_port()
out = open(log, "w", encoding="utf-8")
lines, lock = [], threading.Lock()
stop = False


def note(text):
    print(text, flush=True)
    out.write(text + "\n")
    out.flush()
    with lock:
        lines.append(text)


def reader():
    global s
    buf = b""
    while not stop:
        try:
            buf += s.read(4096)
        except (serial.SerialException, OSError) as e:
            note(f"talk.py: port lost ({e}); opening {port} again")
            while not stop:
                try:
                    s = open_port()
                    note(f"talk.py: port open again ({s.port})")
                    break
                except (serial.SerialException, OSError):
                    time.sleep(0.1)
            buf = b""
            continue
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode("utf-8", "replace").rstrip("\r")
            print(line, flush=True)
            out.write(line + "\n")
            out.flush()
            with lock:
                lines.append(line)


threading.Thread(target=reader, daemon=True).start()


def wait_for(text, limit, seen_from):
    end = time.time() + limit
    while time.time() < end:
        with lock:
            for i in range(seen_from[0], len(lines)):
                if text in lines[i]:
                    seen_from[0] = i + 1
                    return True
        time.sleep(0.05)
    return False


def wait_value(key, limit, seen_from):
    """the integer in the next printed KEY=<integer>, or None"""
    pat = re.compile(re.escape(key) + r"=(-?\d+)")
    end = time.time() + limit
    while time.time() < end:
        with lock:
            for i in range(seen_from[0], len(lines)):
                m = pat.search(lines[i])
                if m:
                    seen_from[0] = i + 1
                    return int(m.group(1))
        time.sleep(0.05)
    return None


def last_value(key, before):
    """the integer in the last KEY=<integer> printed before line index BEFORE, or None"""
    pat = re.compile(re.escape(key) + r"=(-?\d+)")
    with lock:
        for i in range(before - 1, -1, -1):
            m = pat.search(lines[i])
            if m:
                return int(m.group(1))
    return None


def send(text):
    try:
        s.write(text.encode() + b"\n")
    except (serial.SerialException, OSError) as e:
        note(f"talk.py: send {text!r} failed ({e})")


failures = 0
if script:
    pos = [0]
    for n, raw in enumerate(open(script, encoding="utf-8"), 1):
        cmd = raw.strip()
        if not cmd or cmd.startswith("#"):
            continue
        word, _, rest = cmd.partition(" ")
        if word == "send":
            send(rest)
        elif word in ("wait", "expect"):
            parts = rest.rsplit(" ", 1)
            limit = 10.0
            if len(parts) == 2:
                try:
                    limit = float(parts[1])
                    rest = parts[0]
                except ValueError:
                    pass
            ok = wait_for(rest, limit, pos)
            tag = "PASS" if ok else "FAIL"
            if word == "expect" or not ok:
                print(f"talk.py: {tag} line {n}: {word} {rest!r}", flush=True)
                out.write(f"talk.py: {tag} line {n}: {word} {rest!r}\n")
            failures += not ok
        elif word == "expect-min":
            p = rest.split()
            key, low, limit = p[0], int(p[1]), float(p[2]) if len(p) > 2 else 10.0
            got = wait_value(key, limit, pos)
            ok = got is not None and got >= low
            msg = f"talk.py: {'PASS' if ok else 'FAIL'} line {n}: expect-min {key} {low} (got {got})"
            print(msg, flush=True)
            out.write(msg + "\n")
            failures += not ok
        elif word == "expect-range":
            p = rest.split()
            key, low, high, limit = p[0], int(p[1]), int(p[2]), float(p[3]) if len(p) > 3 else 10.0
            got = wait_value(key, limit, pos)
            ok = got is not None and low <= got <= high
            note(f"talk.py: {'PASS' if ok else 'FAIL'} line {n}: expect-range {key} {low} {high} (got {got})")
            failures += not ok
        elif word == "expect-grows":
            p = rest.split()
            key, limit = p[0], float(p[1]) if len(p) > 1 else 10.0
            was = last_value(key, pos[0])
            got = wait_value(key, limit, pos)
            ok = was is not None and got is not None and got > was
            note(f"talk.py: {'PASS' if ok else 'FAIL'} line {n}: expect-grows {key} (was {was}, got {got})")
            failures += not ok
        elif word == "expect-none":
            text, _, secs = rest.rpartition(" ")
            hit = wait_for(text, float(secs), pos)
            note(f"talk.py: {'FAIL' if hit else 'PASS'} line {n}: expect-none {text!r} for {secs} s")
            failures += hit
        elif word == "say":
            note(f">>> {rest}")
        elif word == "confirm":
            note(f">>> {rest} [y/n]")
            answer = sys.stdin.readline().strip().lower()
            ok = answer in ("y", "yes")
            note(f"talk.py: {'PASS' if ok else 'FAIL'} line {n}: confirm {rest!r} (answer {answer or 'none'!r})")
            failures += not ok
        elif word == "reset":
            s.dtr = False
            s.rts = True
            time.sleep(0.15)
            s.rts = False
        elif word == "press0":               # IO0 low (DTR asserted, RTS not), then released
            s.rts = False
            s.dtr = True
            time.sleep(float(rest) / 1000)
            s.dtr = False
        elif word == "sleep":
            time.sleep(float(rest))
        else:
            sys.exit(f"talk.py: line {n}: unknown command {word!r}")
    time.sleep(0.5)
    print(f"talk.py: {'ALL MET' if failures == 0 else f'{failures} NOT MET'}", flush=True)
else:
    for raw in sys.stdin:
        send(raw.rstrip("\n"))
    time.sleep(1)
stop = True
out.close()
sys.exit(1 if failures else 0)
