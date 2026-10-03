# Work unit 017: the 33s/35s mode, and entry as the real machines do it

**From:** abacus · **To:** soroban · **Status:** ready after 016 (it adds a field to 016's image)

## Why

Michael ran our provisional rulings on his own HP 35s and HP 33s (abacus
`docs/evidence/2026-10-03-35s-field-results.md`). Where both machines agree and we differ, we were
wrong and follow them. Where the two machines differ from each other, Michael ruled (abacus
decisions §22): *"We should record both (have them as operational modes) and I lean hp 33s here as
the default."* His observations are the source for this unit's vectors; cite them as "Michael's
33s" and "Michael's 35s" with the date.

## Rules

1. **A mode setting (ours).** Two values, 33s and 35s; the 33s is the default after `ab_init` and
   MEMORY CLEAR, unchanged by CLEAR ALL (as the other modes in p.B-4's table). It is part of 016's
   state image: an image written before this unit loads with the default (016 rule 2, now tested
   for real). How the keyboard reaches it is a menu choice for the device; give the core an op.
   Only the behaviours named in rules 3 and 4 depend on it; any later divergence Michael finds gets
   added here, not as a new switch.
2. **Backspace after CHS keeps the sign (both machines; unit 001 ruling 2 is withdrawn).** Michael:
   `1 ENTER 5 +/- ←` "does not show 0 it just shows - after the one backspace. So the final X is
   -3_" after `3`, with Y = 1, on both machines. Backspace removes the last digit typed; when only
   the sign is left, the entry shows `-` and continues, and the next digit starts the negative
   number. What a second backspace on a lone `-` does was not observed: rule-propose it (ending
   entry with X = 0 and lift disabled, as before, is the natural reading) and it goes on Michael's
   list.
3. **E with no mantissa (mode).** 33s: E starts the number as if 1 had been typed, so `E 3 ENTER`
   is 1000 (Michael's 33s: "X: 1000"). 35s: the E is accepted into the entry line, and ending the
   entry is SYNTAX ERROR with the stack unchanged; the typed text stays in the entry for editing,
   the insertion point at its start, so digits typed next go before the E (Michael's 35s: "Syntax
   error. Pressing enter again drops you back to x:_E3, cursor at beginning"). Unit 001 ruling 3
   (E ignored) is withdrawn in both modes.
4. **The first key after an error message (mode).** 33s: the key clears the message and then acts
   as it would have: `1 ENTER 0 ÷ 5` leaves X = 5, Y = 0, Z = 1 ("5 clears the error and places it
   into X. So its stack is 0, 1, 0, 5_"; this is also unit 001 ruling 1's side effect, now
   confirmed). 35s: the key only clears the message: X = 0, Y = 1, Z = 0 ("5 clears the divide by
   zero error, but doesnt place the 5"). Any key, any error the keyboard caused (an error stop in a
   program too: say if you read the guide otherwise). The core keeps whether a message is showing,
   since it decides what the key does; the device draws it.
5. **Pending, not yet ruled:** FIX 0 shows a trailing radix mark on the 35s (`-3.`; ours `-3`).
   Michael is checking his 33s; if they agree it becomes a rule with no mode, otherwise it joins
   rule 1's mode. Do not change the formatter until it is ruled.

## Acceptance and report

Red then green, as before: Michael's sequences as vectors in both modes, the mode across save and
load, an image from before this unit loading with the 33s default, and every earlier vector file
still passing in the default mode (say which earlier vectors change and why: unit 001's V42-V44
should be the only ones). At review: the hidden sets regenerated with the reviewer's model updated
to these rules, in both modes.
