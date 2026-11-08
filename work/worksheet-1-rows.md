# The worksheet, part 1: the rows (blueprint, before any screen)

**From:** decision 95 step 4 (Michael: "Do the worksheet improvement as you recommend"), abacus #13884, #13953, and the
material in #13972: the design note's step 4, the cards stu-worksheet, stu-worksheet-keep, stu-nav and stu-soft-shift,
and stu-next-when's note. abacus's question: **the row format, with the cause links and the exact value, sized against
the keep budget, before any screen work.** This file answers that question only. The page, the row menus, moving and
the labels' drawing come in part 2, which builds on this part.

**Status:** a draft for abacus. No code. The measurements are on the host at 6e89b45 (the 1.6.0 pin) unless they say
otherwise. The prototype is throwaway: its source and outputs are kept in work/worksheet-evidence/ as the census's
evidence, not as code to build.

## What the rows must carry (from the cards)

- **One page; operations add rows.** An operation never rewrites an old row. The live stack's levels carry X:, Y:, Z:,
  T: at the left (stu-worksheet).
- **Each row keeps its exact value,** not the digits shown (the note).
- **Each row records the op that made it and the 1-3 rows it used** (the cause links). R↓, x↔y and ENTER move values
  without making a row. The links go into the format from the first build, since adding them later changes the saved
  format (the note).
- **The last rows are saved within a byte budget,** and the oldest drop off visibly (stu-worksheet-keep).
- **Forward compatibility** (stu-next-when, Michael): "we'll have to be careful about forward compatibility here".

## What exists today (measured)

- **A value:** `ab_val` is 64 B: kind at 0 (4 B), then 4 B of padding, then re, im and p3 (16 B each, from offset 8),
  then len at 56 (4 B), then 4 B of tail padding. No code compares an `ab_val` by its bytes (`memcmp` in core/ and
  firmware/: none on a value). With a named `uint32_t row` after kind, the host's size stays 64 B (row at offset 4,
  re still at 8).
- **In the image,** a value is kind, len and a mask (3 B), then each non-zero part as 19 B (sign, 16-bit exponent,
  128-bit coefficient): **22 B for a real**, 41 for a complex, up to 60 for a vector. An expression is its 3 B and its
  text, by content (082).
- **ALG's history today:** 16 entries of text (16 × 192 = 3,072 B) and their values (16 × 64 = 1,024 B), in RAM only
  (unit 029 rule 5). The expression store is 48 slots × 192 B (9,216 B), and 16 of them are for history values.
  ab_calc is 120,784 B on the host, in PSRAM on both boards.
- **The saved image:** the abstate partition is 512 KiB, two slots of 256 KiB. The full state (stackkeys full: 66
  equations at their longest, every letter, statistics, RAD) is **14,249 B**, and 082's typical state was 779 B. So the
  slot isn't the constraint.
- **When it's written:** at SAVE and at power-off (APP_SAVE, APP_OFF in standin_main.c), not on every key.
- **What is paid on every key:** UNDO saves the state into its buffer (undo_mark, ab_state_save) and compares
  (ab_state_same). On the stand-in, 082 measured the compare at 2,987 µs for an 8,718 B state and 717 µs for 761 B,
  about 0.35 µs a byte. **Anything in the main image is walked on every key.**

## The prototype: which levels an operation reads (work/worksheet-evidence/)

I didn't know whether the cause links could come from the core's own stack movement, with no table of each op's
arguments. The prototype runs every op (236, from AB_K0 to AB_OP_LAST) in STU's RPN on a known stack. It then runs the
op again with Y, Z or T changed by 100, one at a time. A level is "read" if the change moves a value the op made. Four
bases (X = 3, 1/2, -5/2, 7/3), merged: census-merged.txt.

| what the op makes | levels it reads | ops |
|---|---|---|
| X | none | 34 (digits, π, RCL, CONST, LASTx, CLx, the Σ sums, RAND) |
| X | X | 50 (the one-argument functions, conversions, CHS) |
| X | X, Y | 14 (+ − × ÷, y^x, %, %CHG, nCr, nPr, RMDR, IDIV, the logic ops) |
| X and Y | X, Y | 2 (→POL, →REC) |
| X and Y | X, Y, Z, T | 1 (CMPLX) |
| X, Y, Z, T | none | 2 (CLSTK, CLALL) |

The other 133 make nothing on the stack (menus, modes, ENTER, R↓, x↔y, STO, the CAS keys on a number).

**What it found:**
1. **Comparing values can't tell a new row from an old one.** Typing 3 with X = 3, or ABS(3), leaves the stack
   unchanged byte for byte. The base X = 3 shows K3 and ABS as making nothing, and only the other bases show them. So
   whether an op made a row must come from the op, not from comparing values.
2. **Consumed levels aren't the levels read.** % and %CHG read Y and leave it in place. A rule of "the levels that
   moved away" would drop their Y link.
3. **The census is the table's oracle.** For part 1's build, a table names each op's row and links, and a test runs
   this census (with letters set, which the prototype left at 0, so RCL×'s read of X was missed) and fails on any
   difference.

## The proposal

### 1. A value carries its row

- `ab_val` gains `uint32_t row`: the serial of the row whose value it is, or 0 for none.
- The 33s and 35s never read it. A static assert keeps `ab_val` at 64 B on both boards.
- **Every copy carries it.** ENTER's copy, R↓, x↔y, T's copy down, STO into a letter and RCL from it all keep the
  row, with no line of the stack code changed. So "the live stack values carry X:, Y:, Z:, T:" is each level's row
  field. A letter holding a row's value links to that row, so RCL A's row links to the row STO A stored.
- **The worksheet layer stamps rows.** After each keyboard op in STU, it looks up the op's entry in the table. If the
  op makes values, the layer appends a row for each value made, links it to the rows of the levels the op read (taken
  before the op), and stamps the new value's `row`. Moves make no row.

### 2. A row

In RAM (PSRAM), a byte arena of records appended in order, each:

| field | bytes | |
|---|---|---|
| length | 2 | the record's own length, so a reader skips what it doesn't know |
| serial | 4 | from 1, never reused (a link to a row that was dropped stays a number that no longer resolves) |
| op, arg | 2 + 2 | the op that made it, and its argument (a letter, a digit count) |
| flags | 1 | ALG or RPN, how many links, which of an op's values this is (→POL's two) |
| links | 2 each, 0-3 | back-distances in serials (this row's serial minus the used row's); 0 is "none" |
| text | 1 + n | what was typed: ALG's line, or a typed number's keys; RPN ops have none (the op and the links say it) |
| value | 3 + parts | as the image writes a value today (22 B for a real, 3 + its text for an expression) |

- **The record length goes first** for forward compatibility. A later unit (walk, replay) appends fields after the
  value, and an older build skips them.
- **A row's value is its own.** An expression row holds its text in the record, not in the expression store, so the
  store's 48 slots don't bound the page.

### 3. Sizes, measured on casim's corpus

These rows are built from casim's golden corpus (657 lines answered OK at the calculator's own budgets: arena 8,800 B,
output 192 B), each a typed input and its exact answer. Each row counts 16 B for the fields before the text (15 with
two links, 17 with three), 1 for the text's length, and the value.

| | bytes |
|---|---|
| a number row: an op's result (16 + 1 + a real's 22) | 39; a typed number adds its keys |
| a CAS row, typed in ALG: median | 55 |
| p90 | 230 |
| p99 | 382 |
| max | 402 |
| an RPN CAS row (no text: the op links to X's row) | the answer + 20, so 35 at the median |

| saved budget | number rows | CAS rows, median | CAS rows, p90 |
|---|---|---|---|
| 4 KB | 105 | 74 | 17 |
| **8 KB** | **210** | **148** | **35** |
| 16 KB | 420 | 297 | 71 |
| 32 KB | 840 | 595 | 142 |

### 4. Keep and UNDO

- **The rows are not in UNDO's image.**
  - The state image gains only each level's and each letter's `row`, and the newest row's serial: a few dozen bytes,
    not the page.
  - UNDO restores that state. Rows newer than its serial are hidden, not deleted, so REDO shows them again. The next
    new row overwrites them.
  - The per-key walk grows by those few bytes. With the page inside it, an 8 KB page would add about 2.9 ms to every key
    (0.35 µs a byte, 082's measurement).
- **Saved:** a section after the state in the same slot, written at SAVE and power-off, newest rows first, up to the
  budget.
  - When the page holds more than the budget, the oldest rows are left out.
  - The page's top shows how many rows were dropped (part 2 draws it). That is "the oldest dropping off visibly".
- **RAM:** an arena in PSRAM larger than the saved budget. My lean is 64 KB (1,680 number rows, 1,191 CAS rows at the
  median). When it fills, the oldest rows drop, as at a save.
- **Lean: 8 KB saved.**
  - It keeps 210 number rows, or a lesson's worth of CAS work at the median.
  - It is under the 14,249 B full state.
  - The cost is write time at SAVE and power-off. I haven't measured it, so the build measures it on the stand-in, and
    the budget is a single constant.

### 5. What makes a row, and what doesn't

- **Keyboard ops in STU, by the table.** Typed numbers make a row when their entry ends (ENTER, or an op that takes
  them), with their keys as the text, so `5 ENTER 3 +` gives rows 5, 3 and 8, and 8 links to both.
- **No rows:**
  - in the 33s and 35s modes;
  - inside a running program, a SOLVE or ∫ evaluation, or a table or graph sample. A program's run is one row: XEQ's
    result, linked to the levels it started from.
  - Without this rule, a loop in a program would fill the page.
- **Two values from one op** (→POL, →REC, CMPLX): one row each, adjacent, each linked to the levels read.
- **Clears** (CLx, CLSTK, CLALL): no row. The level loses its row (row 0) and shows as a bare live line.

## Questions, with my leans

1. **The budget:** 8 KB saved, 64 KB in RAM, both constants. The build measures the save's time on the stand-in.
2. **Rows made by a running program:** none, and the run is one row (above).
3. **Clears:** no row; the level is a bare line.
4. **T copy-down:** after a two-argument op, Z and T hold the same row. Both labels belong to one row, which part 2
   must draw. My lean is the lower label, with T drawn as its own bare line under the same value. It isn't settled
   here; it doesn't change the format.
5. **The note's open R↓ and x↔y:** they make no row, as ruled. Their labels move with the values.
6. **Links through letters:** yes, free with the row field (RCL A links to STO A's row).

## What part 1's build would test (for abacus's oracle)

- The op table against the census, run in make test with letters set and four bases.
- `ab_val` at 64 B with the row field (a static assert on both boards).
- Rows and links from key sequences: 5 ENTER 3 + (three rows, two links); % (two links, Y kept); R↓ and x↔y (no row,
  the labels moved); STO A, RCL A (a link through the letter); a program run (one row).
- UNDO and REDO: rows hidden and shown again, never rewritten.
- The saved section: written and read back; the budget's cut at the oldest rows; the dropped count; a record with a
  longer length than this build knows, skipped.
- The image version and its fixture, as 082's: an older build refuses the newer image, as today.
- D68: the arena's PSRAM and the code's flash. The per-key UNDO walk measured on the stand-in, before and after.
