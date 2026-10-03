# Work unit 016: continuous memory, clearing memory, and interrupting a run

**From:** abacus · **To:** soroban · **Status:** ready (the first unit the real device needs from the
core rather than from the manual's chapters)

## Why now

Rev A is a battery calculator with an ESP32-S3: it sleeps, it loses power, it gets new firmware. The
35s has Continuous Memory: "turning it off does not affect any information you've stored" (p.1-1).
Today the core's whole state lives in `ab_calc` and nothing can save it. And a program run is one
call that returns only at a stop; on the P4 a 10M-line budget run took 3.3 s with the keyboard dead.

## Scope (HP 35s User's Guide)

p.1-1 (Continuous Memory); p.B-3 and the table on p.B-4 (CLEAR ALL and MEMORY CLEAR: what each
resets, and to what); p.13-19 (interrupting a running program: C or R/S, "the program completes its
current instruction before stopping", R/S resumes, and after XEQ, GTO or RTN it cannot be resumed);
p.7-2 and p.7-8 (halting SOLVE: the best estimate is in the unknown, it cannot be resumed); p.8-2
(halting ∫: it cannot be resumed, "no information about the integration is available"); the
INTERRUPTED message in appendix F (read its exact text there).

## Rules (manual first; ours where noted)

1. **A state image (ours).** `ab_state_save(const ab_calc *, uint8_t *buf, size_t cap)` writes the
   whole user-visible state and returns its length (or the length it needs, if `cap` is too small);
   `ab_state_load(ab_calc *, const uint8_t *buf, size_t len)` restores it. Everything the B-4 table
   names, plus whatever else the keys can observe (LAST X, the equation list and its pointer, the
   program pointer and the stop state with what resuming needs, the solve and integration state at
   a stop, the random generator's state, the menus' state if the core holds any). Name anything you
   leave out and why.
2. **Ours, the image outlives the firmware.** Not a copy of the struct: a versioned format with
   field tags or sections, little-endian, independent of the compiler's layout, so a later firmware
   reads an older image and gives anything new its MEMORY CLEAR default. A checksum (say which) over
   the whole image. Programs, equations and variables are stored by content (only the lines,
   equations and registers in use), so the image is small; say its size for an empty calculator and
   for a full one.
3. **Ours, a bad image never half-loads.** A wrong checksum, a truncated image, an unknown future
   version or any field out of range: `ab_state_load` changes nothing, returns an error, and the
   device then does MEMORY CLEAR and shows MEMORY CLEAR (the 35s's own message when memory is lost,
   p.B-3). Loading must be safe on any bytes at all: fuzz it under ASan/UBSan.
4. **Save, load, behave the same.** For any state reachable from the keys, save then load gives a
   calculator that answers every later key exactly as the original would, and save, load, save gives
   the same bytes. A stopped program (STOP, INPUT, VIEW, PSE, an error, a budget stop) resumes after
   a load exactly as it would have.
5. **CLEAR ALL and MEMORY CLEAR, as p.B-4's table says**, column by column: CLEAR ALL keeps the
   modes and the format and clears the lists, the stack, the variables and the indirect variables;
   MEMORY CLEAR also restores every default in the right-hand column (DEG, DEC, FIX 4, /c 4095, xiy,
   seed zero, RPN, flags clear). Contrast, the radix mark and the thousands separator are in the
   table: keep them in the core's state if they are yours, or say they are the device's. If
   `ab_init` already equals MEMORY CLEAR, say so and test that it does.
6. **Running in slices (ours, for the device).** A run can be told to stop after n lines and return
   (`budget` exists; make it the device's slice). A slice stop resumes with no visible trace: running
   a program in slices of any size gives the same final state, bit for bit, as running it whole.
   The budget's own limit (AB_RUN_BUDGET) stays a separate, reported stop.
7. **Interrupting, as p.13-19 says.** A device call (`ab_interrupt` or an op) turns a slice stop into
   the stop C or R/S makes on the 35s: the current instruction finished, the pointer at the next
   line, R/S resumes. XEQ, GTO or RTN from the keyboard then forbid resuming (p.13-19): say what R/S
   does then. Interrupting SOLVE leaves the best estimate in the unknown and shows the interrupted
   message (p.7-8); interrupting ∫ (p.8-2) gives no result; neither resumes. SOLVE and ∫ inside a
   program: say what an interrupt then does to the program.
8. Stack lift and LAST X are untouched by save, load and an interrupt (none is a key the user
   presses on a value).

## Acceptance and report

Red then green, as before, with a vector format for the new calls (your design: perhaps `SAVE`,
`LOAD`, `SLICE:n`, `INTERRUPT` tokens, and `%IMAGE=` for a golden image's checksum). Then:

- every existing key-vector file passing with a save and a load inserted after every key
  (a harness flag, not new files);
- every program in the existing vectors run in slices of 1, 7 and 1,000 lines, final state identical;
- a golden image from this version committed under `tests/`, which every later version must load
  (the first forward-compatibility fixture);
- a load fuzz: random and mutated images never crash and never half-load;
- image sizes (empty, the hidden sets' largest end state, a full program memory) and the time to
  save and load one on the S3.

At review: hidden states built by random key sequences across all units, saved and loaded at random
points, checked against the same sequences run straight through; corrupted images; slicing at random
sizes; and a fresh core loading your golden image.
