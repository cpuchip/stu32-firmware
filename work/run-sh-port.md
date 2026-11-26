# device/standin/run.sh talks from a new container after a flash (abacus #17527)

**Why:** at four units in a row (087, 096, 100, 103), `run.sh PORT SCRIPT` with FLASH=1 lost the console straight after
the flash. talk.py read the first boot's lines, then every send failed with `[Errno 5] Input/output error`, and the
script was rerun with FLASH=0, which always passed first time.
- **The cause, from the logs** (work/103-evidence/board/standin-glyph103-flash-run.out): esptool's reset re-enumerates
  the board's USB serial during that first boot. talk.py reopens the by-id link, but it ran in the flash's container,
  which holds only the device node it started with.
- **Not the USB:** the FLASH=0 rerun connected first time each time, and scripts/resources.py's board path (flash,
  sleep 3 s, then talk from a fresh run) never lost it.

## The change (FLASH=1 only)

- The build and flash run in one container, and talk.py in a new one.
- **Between them, on the host:** 3 s, as resources.py waits, then up to 10 s (20 × 0.5 s) for the by-id link to name a
  character device. Not opened here, because an open of a cdc-acm port raises DTR and RTS.
  - If it doesn't come back: `run.sh: PORT did not come back after the flash`, exit 1.
- **Both containers** get the link's resolved device. FLASH=0 is one container, as before.

## Shown

- **The evidence abacus named:** the next flash run passes in one go.
- **`device/standin/run.sh` with FLASH=1 on device/standin/glyph103.txt** (evidence: run-sh-port-evidence/,
  00:05:53Z to 00:07:30Z, the stand-in claimed and released):
  - the flash verified (the app's 2,427,008 B);
  - no port lost;
  - 5 PASS, both frames the host's (4AFAD607, AC98D863), ver=0.104;
  - the one NOT MET is the script's control, which must fail.
- The four before it each needed a FLASH=0 rerun.
- The containers peaked at 209.1 MiB, through soroban's shim.
