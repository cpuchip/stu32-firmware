# Work unit 023: light sleep between keys

**From:** abacus · **To:** soroban · **Status:** ready (Michael, 2026-10-06: "The sleep firmware sounded good")

## Why

draftsman's power budget (abacus docs/hw/rev-a-power-budget.md, every current sourced) puts device/reva as
written at about 40 mA while on. The CPU never sleeps:
- calc_task wakes every 20 ms;
- the scan task runs every 1 ms whether or not a key is down;
- the clock is 240 MHz with no power management.

That gives about 3 weeks on the 3000 mAh cell at 3 h a day. With automatic light sleep between keys the budget gives
about 0.5 mA idle and 2-5 mA in use: months on the 755060, and still weeks to months on the 1200 mAh 6 mm cell that
a thinner case would want (abacus docs/ideas/2026-10-05-thinner-case.md). Rev A's board supports it as built.

## Rules

1. **The app says when it next needs time:** `uint32_t app_next_wake(const app_state *a, uint32_t now_ms)`, the ms
   until app_tick has anything to do.
   - 0 while running a program, or with a render or save pending.
   - The rest of a PSE pause.
   - The time to the quiet save (APP_QUIET_MS) when dirty.
   - The time to auto-off (the setting, unit 022).
   - Anything else in app.c that acts on time alone, such as the low-battery check through power.c. Find every
     `now_ms` comparison in app.c and power.c; each one is a deadline here.
   - UINT32_MAX when nothing is pending.

   **The contract**, which the tests hold it to: called with any `t < now_ms + app_next_wake(a, now_ms)` and the
   same inputs, app_tick returns APP_IDLE and leaves the state unchanged.
2. **calc_task blocks** on the key queue for `min(app_next_wake, the gauge's next 30 s sample)`, not 20 ms. When
   app_next_wake is 0 it does not block, as it doesn't for `app.running` now.
3. **The scan task sleeps when no key is down.**
   - Idle: drive every column low, set the rows and IO0 (ON/C) as GPIO wake-ups on a low level, and block on a
     semaphore that a row's or IO0's interrupt gives.
   - On wake: scan every millisecond as now, until every key reads up and every debounce integrator is settled
     (keys.h `count[]`). Then go back to idle.
   - A key pressed and released inside one debounce window must still be seen or rejected exactly as today: keep
     kb_scan's logic as it is, and change only when it runs.
4. **Automatic light sleep:**
   - CONFIG_PM_ENABLE and CONFIG_FREERTOS_USE_TICKLESS_IDLE in sdkconfig.defaults;
   - `esp_pm_configure` with light_sleep_enable, max 240 MHz (SOLVE and ∫ keep their speed), and min the lowest
     your measurement supports;
   - FREERTOS_HZ stays 1000 (the scan needs 1 ms ticks while a key is down).
5. **No light sleep on USB power.** While PGOOD_N reads low, hold an `ESP_PM_NO_LIGHT_SLEEP` lock. The
   USB-Serial-JTAG console must stay usable for bring-up and flashing, and on USB the power is free.
   Release it when PGOOD_N goes high.
6. **The display keeps its image.** The ULP-RISC-V's EXTCOMIN toggling (1 Hz) must not stop in light sleep.
   draftsman flagged this as unchecked (power budget, "What light sleep needs"). DISP and BOOST_EN must hold
   their levels through it.
7. **Deep sleep (OFF) is unchanged.** sleep_now and its wake path stay as they are.

## Acceptance

**Host (red first, then green):**
- app_test covers app_next_wake for each deadline:
  - running gives 0;
  - PSE gives the rest of APP_PSE_MS;
  - dirty gives the quiet-save time;
  - the auto-off time, at each OFF setting;
  - nothing pending gives UINT32_MAX.
- A property test drives random keys and time jumps. At random `t` before the deadline, app_tick returns APP_IDLE
  and leaves the state unchanged (compare the state image before and after); at the deadline it acts when it
  should.
- Mutants, each caught:
  - app_next_wake ignoring PSE;
  - app_next_wake ignoring the quiet save;
  - an off-by-one that wakes 1 ms late (a PSE resumed late, or a save missed).
- The scan's idle/active switch as a host-testable function, `kb_idle(const kb_state *)`: true only when every key
  reads up and every integrator has settled. Tested against keys.h's existing debounce vectors, and against a press
  shorter than KB_DEBOUNCE.

**On the 4.3" stand-in (no matrix; IO0 is its BOOT button):**
- The ULP's EXTCOMIN edge counter still counts about 20 edges in 10 s with light sleep on, as the deep-sleep probe
  did (device/ulp_probe).
- IO0 wakes it from light sleep and comes through as the ON/C key.
- An idle current, or at least the relative fall, if the stand-in lets you measure it in isolation; say if it
  doesn't.

**On Rev A, at bring-up** (abacus adds a step 5b to docs/hw/rev-a-bring-up.md):
- idle on the cell well under 1 mA;
- every key wakes it;
- "2 ENTER 6 ×" fast with no dropped keys, as step 6 asks now.

## Not in this unit

- Wi-Fi (the MCP idea): its own power rules come with it. Espressif measured automatic light sleep at DTIM 3 at
  about 1.3 mA connected (power budget).
- Clock drift (the power budget's "keeping the clock" section).
