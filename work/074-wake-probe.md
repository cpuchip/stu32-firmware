# Work unit 074: the wake probe (what each deep sleep armed, in the AB_OFFCLK bench image)

**From:** abacus #8590 (board 0005: on battery, after a long sleep, C didn't wake it; reset on battery did, twice) ·
**Proposed:** soroban #8646 · **Ruled:** abacus #8647 · **Status:** built on the host; the device builds and the
stand-in's run to follow.

## Why

The first hypothesis was that the 100 min refresh wake read C (IO0) wrong while the pad was still EXT1's RTC IO. Then
pwr_release would say STUCK, and the chip would sleep without C's wake. The IO0 probe on the stand-in refuted it
(75d6804: low 0 of 60, before and after rtc_gpio_deinit). Nothing yet says what the failing sleeps armed. The board has
to record it on battery itself: plugging USB in turns OFF into a soft off (068 (2)), and a reset loses the RTC memory.

## What it does

- **firmware/power.c:** `pwr_plan_sleep` is what a deep sleep arms, from the release's answer. C's wake (EXT1) is armed
  only when C is up (OK). The timer is the refresh then, or STUCK's 10 s look again.
  - `pwr_note_sleep` is one sleep entry's note: the wake cause of the boot before it, the release's answer, IO0's level
    when it answered, whether C's wake was armed, and the timer in s. It copies the plan, so the note says what was
    armed.
- **Rev A's sleep_now** arms from the plan. The product image is unchanged in behavior: the same two cases, the same
  timers.
- **The AB_OFFCLK bench image** (never a product image: package.sh refuses its banner) records every sleep entry in
  its RTC ring, not only a timer wake's, each with its note.
  - OFFCLK PRINT's line at each sleep carries the note.
  - OFFCLK prints the figures over the timer wakes only, then each kept sleep's note, oldest first.
  - The production periods are kept: offclk_s 0 means REFRESH_US, 100 min.

## Checks

- **power_test:** the plan for OK and STUCK; the note of a refresh wake, a stuck sleep and C's wake; a plan the note
  copies whatever it holds. 184 → 190 checks.
- **Plants:**
  - the note dropping C's wake goes red on 3 checks;
  - the plan arming C's wake whatever the answer goes red on 2.
- **To follow:** the Rev A and AB_OFFCLK builds (Decision 68), and the head-less AB_OFFCLK image on the stand-in:
  - OFF, timer wakes, then OFFCLK showing the notes;
  - a 20 s OFF then C, where the stand-in can wake.

## The run, for Michael (abacus puts it to him; he picks the board and the time)

1. Flash the packaged AB_OFFCLK Rev A image. On the console send OFFCLK RESET and OFFCLK PRINT, then unplug USB.
2. **The control first, 2 minutes:** OFF on battery, wait 20 s, press C. It must wake (068 (5)'s case).
3. OFF on battery, and leave it alone for at least 3.5 h (two refreshes). Note the time of OFF and of the C press.
4. **Press C:**
   - **If it wakes:** plug USB in and send OFFCLK. Each sleep's note shows what it armed.
   - **If C does nothing:** plug USB in without a reset and keep the console open for up to 100 min. The next wake's
     line has its note:
     - a line every ~13 s is STUCK (with IO0's level);
     - a line at the refresh with C's wake armed means EXT1 is armed but doesn't fire;
     - no line at all means the timer doesn't fire either.
