#!/usr/bin/env python3
"""Reads the on-chip test's report from a serial port.

    capture.py PORT LOG [TIMEOUT_SECONDS]

Waits for the app's READY line, sends one byte to start it, then copies every line to stdout
and LOG until DEVICE DONE. Exits 0 on DEVICE PASS, 1 on FAIL or a timeout. Needs pyserial
(present in the ESP-IDF environment). DTR and RTS are held low so opening the port neither
resets the chip nor puts it in its bootloader."""
import sys, time
import serial

port, log = sys.argv[1], sys.argv[2]
limit = float(sys.argv[3]) if len(sys.argv) > 3 else 3600
s = serial.Serial()
s.port, s.baudrate, s.timeout = port, 115200, 0.5
s.dtr = s.rts = False
s.open()
start, started, verdict = time.time(), False, None
buf = b""
with open(log, "w") as out:
    while time.time() - start < limit:
        buf += s.read(4096)
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode("utf-8", "replace").rstrip("\r")
            print(line, flush=True)
            out.write(line + "\n")
            if not started and line.startswith("READY"):
                s.write(b"\n")
                started = True
            if line.startswith("DEVICE PASS"):
                verdict = 0
            elif line.startswith("DEVICE FAIL"):
                verdict = 1
            if line.startswith("DEVICE DONE"):
                sys.exit(1 if verdict is None else verdict)
print(f"capture.py: no DEVICE DONE within {limit:.0f} s", flush=True)
sys.exit(1)
