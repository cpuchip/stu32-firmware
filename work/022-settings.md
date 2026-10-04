# Work unit 022: the device's settings (provisional homes)

**From:** abacus · **To:** soroban · **Status:** ready after 019

## Why

Radix and digit grouping, the auto-off time, the buzzer's volume and the 33s/35s mode need homes and
need to survive a power cut. "Where settings live" is one of Michael's open calls on the roadmap page.
While he sleeps, this unit takes the first option drafted there: MODE holds the 33s/35s switch (019
built it), and RADIX and SETUP hold the rest. If he rules otherwise, only keymap data moves.

## Rules

1. **RADIX (gold 3):**
   - F1 `.` and F2 `,` set the radix mark;
   - F3 `1,000` and F4 `1 000` set digit grouping, and F5 `none` turns it off.

   Source: the 35s DISPLAY menu's radix and grouping options (UG p.1-21 to 1-24; read the pages for
   their exact meaning, including what `,` as radix does to the grouping mark). The screen already
   takes `radix` and `separator` in screen_ui. Show the mark in effect on its soft key.
2. **SETUP (blue 3):**
   - F1 **OFF** cycles the auto-off time: 2, 5, 10 and 30 minutes (ours; the 35s has a fixed 10, p.1-1);
   - F2 **VOL** cycles the five piezo levels (piezo.h), sounding one short tone at the new level;
   - F3 **BEEP** turns the buzzer's uses on or off as a whole.

   What sounds is Michael's call (the roadmap's "what the buzzer sounds for"). Until he answers,
   only the error tone uses it, at the level set. Show each value on its key.
3. **Settings survive.** They go in unit 016's state image as a versioned field. An image from before
   this unit loads with the defaults: radix `.`, no grouping, 10 minutes, the loudest level, BEEP on.
   CLEAR ALL (unit 021) keeps them, as the guide keeps modes and formats (p.1-29). MEMORY CLEAR resets
   them (appendix B).
4. **The core keeps none of this except what it needs to format numbers** (the radix mark and
   grouping, if formatting needs them). Volume, OFF time and BEEP are the device's. Put them where
   app.c and power.c can read them.

## Acceptance

- app_test: each RADIX choice changes a formatted number on the screen model, with `,` radix and
  grouping together tested.
- OFF cycles and wraps.
- VOL cycles the five levels.
- An image saved with non-default settings loads them back.
- A pre-022 image loads the defaults (store_test).
- CLEAR ALL keeps them.
- Red first, then green. Mutants: settings missing from the image, and OFF failing to wrap.
