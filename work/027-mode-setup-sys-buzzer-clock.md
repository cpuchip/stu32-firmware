# Work unit 027: MODE, SETUP and SYS each get one job; the buzzer's uses one by one; a clock

**From:** abacus · **To:** soroban · **Status:** ready after 025 and 026 (abacus decisions 45 and 46, Michael,
2026-10-06)

## Why

Michael's rulings sort the settings menus by job:
- **MODE** is how the calculator behaves: the personality (33s, 35s, later STU and RPL) and the entry logic (RPN,
  later ALG).
- **SETUP** is the device.
- **SYS** is information. It is undefined today (keymap.c:50, menu -1).

Two of his calls ride along:
- **The buzzer** (his note from 10-04, still open): every use on, with "a good way to turn any/all off not just
  vol". Unit 022's BEEP is one switch for all of them.
- **The screen layout:** decided, "Clock".

## Rules

1. **MODE** (blue ENTER):
   - F1 `33s`, F2 `35s` (as now);
   - F3 kept for `STU`;
   - F4 `RPN`;
   - F5 kept for `ALG`.

   Put F3 and F5 in the keymap only when their units land; until then they are blank. Mark F4 `RPN` on, so the
   menu already reads as the place where RPN/ALG lives. Programs are unaffected (decision 45: they keep their
   stamped mode).
2. **SETUP** (blue 3):
   - F1 `OFF` and F2 `VOL` as now;
   - F3 `BEEP▸` opens the buzzer's uses (rule 3);
   - F4 `CLOCK▸` (rule 4). **Open (2026-10-06):** Michael's decided SETUP card lists OFF, VOL and BEEP only. Where
     the clock is set is his call, asked. Build rule 4 with SETUP's F4 as its provisional home; if he places it
     elsewhere, only keymap data moves.
   - F5 and F6 kept for later (Wi-Fi).
3. **The buzzer's uses**, each on or off, all on by default (Michael: "All of these are good!"):
   - `CLICK`: a short tick on every key;
   - `ERROR`: the error tone, as now;
   - `PRGM`: a programmable BEEP (rule 6);
   - `BATT`: one tone when the battery first reads low;
   - `ON`: a short chirp at power-on (a wake from OFF, not a light-sleep wake);
   - `DONE`: a tone when a run that took longer than 2 s ends, whether a program, SOLVE or ∫.

   The menu: `CLICK ERROR PRGM BATT ON ▸`, then `DONE ALL▸`.
   - Each key toggles its use and shows its state.
   - `ALL` turns every use off if any is on, else all on.
   - VOL stays the level; VOL 0 silences everything, as now.

   The settings image grows a uses mask (version bump). An older image loads with all on if its BEEP was on, all
   off if it was off.
4. **The clock:**
   - **The time** comes from the S3's RTC timer (`gettimeofday`). It keeps running through light and deep sleep;
     it is lost only when the cell is disconnected.
   - **Setting it:**
     - the console: `TIME hh:mm[:ss]`;
     - SETUP → `CLOCK▸`: F1 `SET`, a prompt for HH MM in digits; F2 `12/24`;
     - later from the app or Wi-Fi.
   - **The status line** shows it, in 12- or 24-hour form per the setting, default 12 (Michael is in the US). Where
     it goes in the status band is screen.c's call, beside the mode.
   - **Never set:** it shows `--:--`.
   - **The minute's change** is an app_next_wake deadline (unit 023), so the device wakes once a minute while on to
     redraw, and not otherwise. Say what that costs in idle current if you can estimate it.
   - The host keeps the clock behind an injectable "now in wall time", so app_test can drive it.
   - Drift correction is not in this unit (draftsman's power budget, "keeping the clock").
5. **SYS** (blue C):
   - F1 `BATT`: a screen with the gauge's percent, the charge state (CHG_N, PGOOD_N) and, if the gauge gives it, the
     cell's voltage;
   - F2 `VER`: the firmware's version, the core's commit and the build date;
   - ~~F3 `MEM`: free memory, program bytes and the internal heap's free and largest block (unit 023's lesson).~~
     **Amended 2026-10-06, to match Michael's decided SYS card** ("BATT, VER, FLASH (new) free flash mem ? maybe
     different then MEM since this is ESP32/android storage?"): F3 `FLASH` shows the free storage. On the device
     that's the state partition's free space; in the app it's its storage. It also shows the internal heap's free and
     largest block, which is unit 023's lesson and belongs with "how much room is left". The calculator's memory
     catalog (variables, programs, bytes; the 33s's MEM) stays in the MEM menu, a separate thing.

   Each is a view that any key leaves.
6. **BEEP, the programmable instruction.** None of the 33s, 32SII or 35s has one (no buzzer, and no BEEP in any of
   the three manuals: checked). So it's ours: a programmable op that sounds one short tone at the set volume when
   the `PRGM` use is on, and does nothing otherwise. It doesn't affect the stack. It's typed from the keymap where
   you propose (the PRGM-related menu is the natural home). It's in all modes, since it's an addition, not a
   divergence.

## Acceptance

- **app_test:**
  - each MODE, SETUP, BEEP▸, CLOCK▸ and SYS key, with its soft-key state shown;
  - each use toggled;
  - ALL both ways;
  - VOL 0 silencing all;
  - each of the six uses firing exactly when it should and not otherwise, including `DONE` at 2 s and not at
    1.9 s, and `BATT` once, not per tick;
  - the clock's 12 and 24 hour forms;
  - `--:--` until set;
  - the minute's deadline in app_next_wake.
- **store_test:**
  - the uses mask and 12/24 survive a save and load;
  - an older settings image maps BEEP on to all on, and off to all off.
- **The vectors:** BEEP in a program (stack untouched; the device sees one tone request), and BEEP with PRGM off.
- **Mutants, each caught:**
  - a use that ignores its switch;
  - ALL inverted;
  - DONE firing on a short run;
  - the clock's minute deadline missing from app_next_wake.
- **On the 4.3" stand-in:** the menus, the clock set from SETUP and shown, and SYS's screens. Rev A's piezo checks
  are bring-up step 2 (`BEEP` on the console).
