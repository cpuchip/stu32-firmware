# Unit 087's evidence (abacus #13762)

The record is in work/087-libbid-sections.md. Base 86b3fbf, new 0a7de57, all on lepton on 2026-11-08, with the
stand-in board (1a86 56E1012198).

## The files

- **transcripts/{base,new}/**: talk.py's transcripts as captured (build/standin.log, build/reva-headless.log).
  - `standin-smoke.log` and `standin-accept.log` are device/standin/run.sh with device/standin/smoke.txt (built
    and flashed), then accept.txt with FLASH=0.
  - `reva-smoke.log` and `reva-accept.log` are the same two scripts through device/reva/headless.sh: Rev A's main,
    head-less, on the stand-in's S3.
  - Run order: Rev A base, Rev A new, then the stand-in new, then the stand-in base (each smoke loads what the run
    before saved).
- **transcripts/base-try1/standin-smoke.log**: the failed base stand-in attempt. It flashed on ttyACM1, the port
  re-enumerated to ttyACM3, and talk.py lost it. It is kept only because the record cites it.
- **make-test/**:
  - `base.txt` and `new.txt`: `make test` as captured;
  - `mask.sh`: the mask;
  - `base.masked.txt` and `new.masked.txt`: what the mask gives, identical to each other. `mask.sh base.txt`
    reproduces base.masked.txt byte for byte, and the same for new.
- **map-controls.sh** and **map-controls.txt**: libbid's per-function sections in each link map, and the library's
  flag stamp at each commit.
- **esptool-wrote.txt**: esptool's own "Wrote" lines for each flash (the app is at 0x10000).
- **resources-board.txt**: `scripts/resources.py board 86b3fbf 0a7de57`'s output: heap, stack and speed.

## The transcripts' mask, base against new

Set these aside, then everything else should be identical:
1. **ESP-IDF's boot log:** lines that start `I (n) `, `W (n) ` or `E (n) `. The image's segment sizes, compile time,
   ELF hash and timestamps move with any build.
2. **VER, the core commit and the boot id:** `ver=0.86 core=86b3fbf id=…` against `ver=0.87 core=0a7de57 id=…`.
3. **The heap:** `heap internal N (largest block M)`, `heap=N`, and the stand-in's `calc stack 32768, then N`.
   The heap is +4,608 on both targets, and Rev A's largest block +4,096.
4. **`Saved PC:`:** a code address at the software reset, which moves with the layout.
5. **The save's milliseconds:** `save: 893 bytes, ok, N ms`.
6. **Each smoke run's loaded state:** `load: N bytes ok`, and the `state:` and `eqn:` lines that follow. That is
   whatever the run before had saved.
7. **accept's timed counter:** the X in the `state:` lines after `run: started`, at SLICE and STOP, and again after
   the restart. The program counts during `sleep 3` and is stopped by R/S, so its X is a timing.
8. **Serial noise at a reset:** stray bytes before `ESP-ROM:`.
9. **Rev A's `ulp-edges=` count:** the ULP coprocessor's edge counter. Head-less, no Rev A pin is driven, so it is
   a free-running count read at a moment.
10. **Where talk.py's own `PASS` line falls among the device's lines:** the script's verdict and the device's output
    are two writers to one transcript, so their order can swap by a line. This happens once, in the stand-in
    accept: `PASS line 50` comes before the `boot:`/`flash:` lines at the base and after them at the new build.

After items 1 to 10, what still differs is the one Rev A stack reading named below. The script that applied this
mask is `mask-transcripts.py`.

Stack-free readings are *not* masked. On the stand-in they are identical at every line. On Rev A, two readings
differ, both the first `state:` after a fresh boot:
- base 28,644 against new 28,788, in reva-smoke (line 86) and reva-accept's second boot (line 179);
- the same reading after accept's third boot is 28,788 at both (line 387).

So the base image gives both values for the same kind of reading. That makes it a timing (an interrupt landing at
the low-water point), not a path the new image left out. resources-board.txt's masked rows are +0 on every path.
