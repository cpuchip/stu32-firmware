#!/usr/bin/env python3
"""Talks to the stand-in over its console (unit 018).

    talk.py PORT LOG [SCRIPT]

Copies every line the board prints to stdout and LOG. With a SCRIPT, runs its lines in order:

    send TEXT              send TEXT and a newline (the firmware reads K<n> as key n, and more)
    wait TEXT [SECONDS]    wait for a printed line containing TEXT (default 10 s); fail if none
    expect TEXT [SECONDS]  the same, named for a check: a miss is reported as FAIL
    reset                  pulse EN through the board's auto-reset (RTS, with DTR held low)
    sleep SECONDS
    # ...                  a comment

Exits 0 when every wait and expect was met. Without a SCRIPT it relays stdin lines to the board
until EOF. Needs pyserial (present in the ESP-IDF environment). DTR and RTS are held low on open
so opening the port neither resets the chip nor enters its bootloader."""
import sys
import threading
import time

import serial

port, log = sys.argv[1], sys.argv[2]
script = sys.argv[3] if len(sys.argv) > 3 else None
s = serial.Serial()
s.port, s.baudrate, s.timeout = port, 115200, 0.2
s.dtr = s.rts = False
s.open()
out = open(log, "w")
lines, lock = [], threading.Lock()
stop = False


def reader():
    buf = b""
    while not stop:
        buf += s.read(4096)
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


failures = 0
if script:
    pos = [0]
    for n, raw in enumerate(open(script, encoding="utf-8"), 1):
        cmd = raw.strip()
        if not cmd or cmd.startswith("#"):
            continue
        word, _, rest = cmd.partition(" ")
        if word == "send":
            s.write(rest.encode() + b"\n")
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
        elif word == "reset":
            s.dtr = False
            s.rts = True
            time.sleep(0.15)
            s.rts = False
        elif word == "sleep":
            time.sleep(float(rest))
        else:
            sys.exit(f"talk.py: line {n}: unknown command {word!r}")
    time.sleep(0.5)
    print(f"talk.py: {'ALL MET' if failures == 0 else f'{failures} NOT MET'}", flush=True)
else:
    for raw in sys.stdin:
        s.write(raw.rstrip("\n").encode() + b"\n")
    time.sleep(1)
stop = True
out.close()
sys.exit(1 if failures else 0)
