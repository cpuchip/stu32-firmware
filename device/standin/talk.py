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
    remember KEY           keep the last KEY=<integer> printed so far, for expect-step
    expect-step KEY N [SECONDS]
                           the next printed KEY=<integer> must be exactly N more than the one remembered
                           (068: boots up by one, so a sleep that bounced awake shows; the firmware prints
                           its own boot line at each boot, so "the last printed" would already be the new one)
    expect-none TEXT SECONDS
                           no printed line may contain TEXT for SECONDS: one that does is a FAIL
                           (unit 061: a key bounce, a ghost)
    say TEXT               print TEXT for the person at the board, marked >>> (unit 061)
    confirm TEXT           ask the person at the board TEXT, answered y or n on this program's stdin;
                           n or no answer is reported as FAIL (unit 061: what a log cannot judge)
    reset [SECONDS]        pulse EN through the board's auto-reset (RTS, with DTR held low); passes when the
                           firmware shows a new boot (new=1 on its boot line, 068) within SECONDS (20)
    expect-boot REASON [SECONDS]  sends X and checks the boot's reason=REASON (POWERON, SW, DEEPSLEEP, ...)
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


def usb_jtag(dev):
    """the S3's USB-Serial-JTAG (Espressif's VID 0x303A), not a USB-UART bridge"""
    try:
        from serial.tools import list_ports
        return any(q.device == dev and q.vid == 0x303A for q in list_ports.comports())
    except Exception:
        return False


def open_port():
    p = serial.Serial()
    p.port, p.baudrate, p.timeout = os.path.realpath(port) if os.name == "posix" else port, 115200, 0.2
    p.write_timeout = 2         # a console that stops reading fills the S3's USB FIFO: a failed send, not a hang (061)
    if sys.platform.startswith("linux") and usb_jtag(p.port):
        # Linux's cdc-acm raises DTR and RTS together at every open, whatever pyserial asks first. Releasing DTR before
        # RTS then resets an S3's USB-Serial-JTAG into download mode (board 0001 on lepton, 068: boot:0x23 at each
        # open); releasing RTS first does not reset it (esp-idf-monitor's open_serial, Apache-2.0, does the same). Only
        # the S3's own port: on the stand-in's CH340 this order pulses IO0 (a C press, seen waking a deep sleep), and
        # its scripts count on the old order's reset at open ("wait REVA READY").
        p.dtr = p.rts = True
        p.open()
        p.rts = False
        p.dtr = False
    else:
        p.dtr = p.rts = False
        p.open()
    return p


sys.stdout.reconfigure(errors="replace")    # (a Windows console's code page may lack a legend's character)
s = open_port()
out = open(log, "w", encoding="utf-8")
lines, lock = [], threading.Lock()
last_send = [0]                                # index in lines of the first line after the last send
stop = False


def note(text, seen=False):
    """print and log TEXT; only a note a script may wait for (seen: the port's loss and return, a failed send) joins
    the lines that expects search. A verdict stays out: expect-none's repeats its own TEXT, so the next expect-none of
    the same TEXT matched it at once and failed (moles.txt at unit 078's gate)"""
    print(text, flush=True)
    out.write(text + "\n")
    out.flush()
    if seen:
        with lock:
            lines.append(text)


def reader():
    global s
    buf = b""
    while not stop:
        try:
            buf += s.read(4096)
        except (serial.SerialException, OSError) as e:
            note(f"talk.py: port lost ({e}); opening {port} again", seen=True)
            while not stop:
                try:
                    s = open_port()
                    note(f"talk.py: port open again ({s.port})", seen=True)
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


def wait_boot(limit, seen_from):
    """the next boot line ("boot: boots=N reason=R new=B ... id=H"), its fields as a dict, or None. One read of the
    whole line: boots=, new= and id= share it, and reading them one wait_value at a time skipped past it (068)"""
    end = time.time() + limit
    while time.time() < end:
        with lock:
            for i in range(seen_from[0], len(lines)):
                if "boot: boots=" in lines[i]:
                    seen_from[0] = i + 1
                    return dict(re.findall(r"(\w+)=(\S+)", lines[i]))
        time.sleep(0.05)
    return None


def set_lines(dtr, rts):
    """RTS then DTR, DTR always written: Windows' usbser.sys passes line changes on only when DTR is set, so an RTS
    change alone never reached the S3 (abacus #7605 (1): talk.py's reset did not reboot board 0005 on Windows);
    esp-idf-monitor writes them the same way"""
    s.rts = rts
    s.dtr = dtr


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
    with lock:
        last_send[0] = len(lines)
    try:
        s.write(text.encode() + b"\n")
    except (serial.SerialException, OSError) as e:
        note(f"talk.py: send {text!r} failed ({e})", seen=True)


failures = 0
remembered = {}
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
            # the reply to the last send against the newest value before it (068 (5b), board 0001: a reset's extra X
            # reply left unread made "was" and "got" two replies from before the sleep, 3 and 3, while the count grew)
            at = [last_send[0]]
            was = last_value(key, at[0])
            got = wait_value(key, limit, at)
            pos[0] = max(pos[0], at[0])
            ok = was is not None and got is not None and got > was
            note(f"talk.py: {'PASS' if ok else 'FAIL'} line {n}: expect-grows {key} (was {was}, got {got})")
            failures += not ok
        elif word == "remember":
            remembered[rest.strip()] = last_value(rest.strip(), pos[0])
            note(f"talk.py: line {n}: remember {rest.strip()}={remembered[rest.strip()]}")
        elif word == "expect-step":
            p = rest.split()
            key, step, limit = p[0], int(p[1]), float(p[2]) if len(p) > 2 else 10.0
            was = remembered.get(key)
            at = [last_send[0]]                 # the reply to the last send, as expect-grows (an unread reply is older)
            got = wait_value(key, limit, at)
            pos[0] = max(pos[0], at[0])
            ok = was is not None and got is not None and got == was + step
            note(f"talk.py: {'PASS' if ok else 'FAIL'} line {n}: expect-step {key} {step} (was {was}, got {got})")
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
            # 068 (abacus #7578): a reset passes when the firmware shows a new boot. X marks the boot it reports as seen,
            # so after the X here a later X (or the boot's own line) printing new=1 can only be a new boot, even when the
            # port reopened after the boot lines (Windows) and even after a chip reset (EN), which starts boots= again
            # Since 068 (5b) the boot line also carries id=, random at each boot, and a changed id is the new boot: a
            # POWERON starts boots= again, and an X reply lost while the port reopens can take new=1 (abacus #7605 (2))
            send("X")
            before = wait_boot(5, pos) or {}
            set_lines(dtr=False, rts=True)
            time.sleep(0.15)
            set_lines(dtr=False, rts=False)
            fresh, after, end = False, {}, time.time() + float(rest or 20)
            while time.time() < end and not fresh:
                time.sleep(2)
                send("X")
                after = wait_boot(3, pos) or {}
                if "id" in before and "id" in after:
                    fresh = after["id"] != before["id"]
                else:
                    fresh = after.get("new") == "1" or (after.get("boots", "").isdigit() and
                                                        before.get("boots", "").isdigit() and
                                                        int(after["boots"]) > int(before["boots"]))
            was, got = before.get("boots"), after.get("boots")
            note(f"talk.py: {'PASS' if fresh else 'FAIL'} line {n}: reset (boots {was} -> {got}, new boot {'seen' if fresh else 'not seen'})")
            failures += not fresh
        elif word == "expect-boot":           # the boot's reason by name (068): POWERON, SW, DEEPSLEEP, BROWNOUT, ...
            p = rest.split()
            reason, limit = p[0], float(p[1]) if len(p) > 1 else 10.0
            send("X")
            ok = wait_for(f"reason={reason} ", limit, pos)
            note(f"talk.py: {'PASS' if ok else 'FAIL'} line {n}: expect-boot {reason}")
            failures += not ok
        elif word == "press0":               # IO0 low (DTR asserted, RTS not), then released
            set_lines(dtr=True, rts=False)
            time.sleep(float(rest) / 1000)
            set_lines(dtr=False, rts=False)
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
