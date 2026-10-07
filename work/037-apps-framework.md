# Work unit 037: APPS, a framework for programs that own the screen (the first: Babal)

**From:** abacus · **To:** soroban · **Status:** after 033's acceptance, ahead of →POL and 036 if you agree (abacus
decision 55, Michael, 2026-10-07: a games seat ports Babal under APPS)

## Why

Michael wants Babal, the HP 48/49/50 game he played for years, on the STU-32 under APPS (blue 0, already reserved in
the layout). A games seat ports it into C (decision 55). It needs a place to run: something that owns the 400 × 240
screen and the keys while it plays, and gives the calculator back exactly as it was. That place is app.c's, so it's
yours. Because tally's Android app and primer's page drive the same app.c, every app runs there too.

## Rules

1. **The app interface,** a C struct per app, linked in at build time (there are no loadable apps):
   - its name, as APPS lists it;
   - `start(ctx)`;
   - `key(ctx, key, down)`, with key-down and key-up events (030 added key-up);
   - `tick(ctx, ms)`, with the elapsed milliseconds;
   - `draw(ctx, frame)`, the whole ls027 frame;
   - `stop(ctx)`;
   - `save` and `load` of the app's own small state (see rule 5).

   Propose the exact signatures. The games seat writes against them, so freeze them early.
2. **APPS (blue 0) is a menu of the registered apps,** one per soft key, paged if more than five, as CONST is.
   - Choosing one starts it.
   - Ship one tiny built-in app, KEYS (shows each key's name and up/down), as the framework's own test subject and
     a diagnostic.
3. **While an app runs:**
   - **The core is untouched:** no ab_* call is made, and the calculator's state image is byte-identical before
     and after.
   - **Keys go to the app, except the exit key.**
   - **The app's draw replaces screen_render.**
   - **Auto-off is suspended,** because a game is played without keys for long moments. It resumes on exit.
4. **The exit:** C (the bottom-left key, the HP's ON position) always ends the app, which can't claim it.
   - The calculator returns to its screen as it was.
   - Propose otherwise if the layout argues for another key. Michael's open question is the exit key, so say what
     you chose and I'll put it to him.
5. **Each app has its own small save** (64 bytes, say, enough for a high score and settings).
   - It's stored apart from the calculator's image, so a calculator MEMORY CLEAR doesn't wipe a high score, and an
     app can never corrupt the image.
   - It goes through the same store layer, with its own key and checksum.
   - Propose the size and where it lives.
6. **The frame rate:** the app sets its own tick rate (up to what the LS027 and the S3 can take). Measure the
   achievable full-frame rate on Rev A's path (the SPI send of a 12 KB frame) and report it. The games seat needs it
   to plan.
7. **Light sleep (023)** is not entered while an app's tick is active.

## Acceptance

- **app_test:**
  - starting KEYS from APPS;
  - keys and key-ups routed;
  - C exits;
  - the calculator image byte-identical across a session (MARK / SAMEMARK);
  - the app save surviving save/load and a calculator MEMORY CLEAR;
  - two apps paged in the menu.
- **Mutants:**
  - a key leaking to the core during an app;
  - C claimable by the app;
  - the app save inside the calculator image;
  - auto-off firing during an app.
- **The S3:** the full-frame rate, and KEYS on the stand-in.
- **No change** to any accepted vector. The calculator never sees an app.
