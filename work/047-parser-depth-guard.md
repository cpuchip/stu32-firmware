# Work unit 047: the equation parser's depth guard

**From:** soroban (found measuring CAS 005's stack watch, abacus #5106) · **Ruled by:** abacus #5134, #5153 ·
**Status:** built and measured on the S3 (below).

## The defect

**Typing an equation nested about 25 parentheses deep and pressing ENTER resets the S3.**
- **The parse has no depth limit:** eqn_store_typed → stored_text → eqn_write_implied → parse_all → parse_add →
  parse_mul → parse_neg → parse_pow(_r) → parse_post → parse_prim → "(" → parse_add …
- **It overflows the calc task's 32 KB stack** into its neighbours in the internal heap. FreeRTOS then panicked
  in its tick handler (LoadProhibited, EXCVADDR 0x4) at 46 levels, and the board reset at 30.
- **Every parse shares the code path:** storing, evaluating, SOLVE, ∫, the graph, TABLE, CAS's translation checks,
  STU's expression line.

## Measurements (the S3 stand-in, through the keys; the calc task's high-water mark, free of 32,768 B)

**The ENTER site** (typing X^(…X…) d deep in STU, then ENTER):

| d | free |
|---|---|
| boot | 29,188 |
| 10 | 17,300 |
| 15 | 11,540 |
| 20 | 5,780 |
| 30 | the board resets |

That's 1,152 B a level; free ≈ 28,820 − 1,152 d.

**The deepest site:** program Z holds an equation line d deep. It runs as SOLVE's (or ∫'s) function, from a
program 19 XEQ levels deep. At 20 levels the run stops at XEQ OVERFLOW: the function's call takes a level, as 35s
p.F-5 says.

| site | d = 1 | d = 10 | free ≈ |
|---|---|---|---|
| SOLVE, 19 deep | 22,580 | 12,212 | 23,732 − 1,152 d |
| ∫, 19 deep | 23,252 | 12,804 | 24,413 − 1,161 d |
| program entry (storing the line) | 28,468 | 28,468 | (no parse) |

**casim:** at 15 and 20 levels, D/DX (RESULT TOO LONG) left the high-water mark unchanged (11,540; 5,780). Its walk
fits inside stack the parse had already used. Its own 46-level worst can't be reached on the device.

**Each kind of recursion, by a painted stack on the host** (bytes a level; X^( is 1,136 on the host and 1,152 on
the S3):

| pattern | bytes a level | bounded before 047 by |
|---|---|---|
| `(` nest | 1,008 | nothing (192 bytes: ~95 levels) |
| `SIN(` nest | 1,008 | nothing |
| `X^(` nest | 1,136 | nothing |
| `X^X^X…` (STU, right to left) | 128 | the length: ~95 steps |
| `---…X` (unary minus) | 192 | the length: ~190, about 36 KB |
| `X^---X` (exponent minus) | 32 | the length |

## The census: every recursive path in the parser (core/eqn.c)

| | path | handled by |
|---|---|---|
| A | parse_prim `(` → parse_add (a group) | +8 units |
| B | parse_prim `[` → parse_add, each vector element | +8 units |
| C | a function name's `(` → parse_add: its argument, a K2 function's second after `,` (35s) or `:` (33s), RND's `x:n` | +8 units |
| D | parse_neg → parse_neg (unary minus) | a loop: count, parse, negate once a minus; +2 units while the operand is read |
| E | parse_parg → parse_parg (33s/35s exponent minus) | a loop |
| F | parse_parg_r → parse_parg_r (STU exponent minus) | a loop |
| G | parse_pow_r → parse_parg_r → parse_pow_r (STU's a^b^c, right to left) | +1 unit a step |

Everything else already loops: parse_add, parse_mul, 33s/35s parse_pow (left to right), parse_post's `!`. So every
`(` or `[` character opens one level of 8. A function's argument list and a group cost the same, since each is a
parse_add from parse_prim.

**D's loop still has a frame.** parse_neg's frame stays on the stack while its operand is read whenever a minus was
counted (with no minus the compiler may turn the call into a jump). The stack fuzz found it: `-(` costs 1,168 B a
level on the host against `(`'s 1,008. So a unary minus group costs **+2 units** (256 B of the S3's 128 B a unit),
once however many minuses it holds. An exponent's minus (E, F) costs nothing measurable (32 B, inside the +1 of G).

**Outside the parser,** no recursion follows the equation's text (`fn_rmdr`, `ln_two_x`, `minor`, state's
`r8`/`w8` are bounded). Every parse funnels through parse_all: eqn_eval, eqn_write_implied.

## The guard (abacus #5153: (i) + (ii), 153 units, NESTED TOO DEEP)

1. **D, E and F become loops,** with every result unchanged: STU's −X^2, −−X^2 and X^−−2 keep their precedence.
2. **One counter in the parser:** +8 entering A, B or C; +1 entering G; +2 for a unary minus group while its operand
   is read (D); leaving restores it. Past **153** the parse stops with ABN_TOO_DEEP, message **NESTED TOO DEEP**.
   - Once the parse has failed it descends no further, so the counter's peak is the units reached before the first
     error (a K2 function's second argument after a failed first is not counted). tests/eqn_depth.py stops there too.
   - 153 = (23,732 − 4,096) / 128, so the deepest site keeps ≥ 4 KB.
   - That admits 19 levels of `(` or `SIN(` (152), 17 of `X^(` (9 each: 153), 15 of `-(` (10 each: 150), any STU
     power chain (≤ 95), and mixes bounded by their bytes.
   - **A change from the ruling (#5153):** `-(` is 10 units, not 8, so 16 levels of it are refused where the ruling's
     rule would admit 19. The weight is what the stack costs; without it the stack fuzz found 25 admitted equations
     over the byte bound.
   - In 33s and 35s modes powers loop, so only parentheses count.
3. **None of the guides has a message for this:** 35s appendix F p.F-1–F-5, 33s F-1–F-4, 32SII appendix E E-1–E-4,
   read rendered. Their nearest is MEMORY FULL, which would send a user deleting variables.

## Every entry path

- **Typing, and editing a stored equation:** refused at ENTER (the store parses), and the equation stays being typed.
- **Program entry:** the store doesn't parse, so it gets an explicit guarded check; an over-deep line is refused at
  ENTER. Its text is not kept typed: program entry ends the typing before the store, as for MEMORY FULL.
- **casim's output:** stored by the same path, so it's refused with NESTED TOO DEEP. SUBST's typed replacement is
  checked too (NESTED TOO DEEP, still typed; before the review it answered SYNTAX ERROR).
- **A state image from flash:** an older over-deep equation is **quarantined**. It stays as text, and every use
  (evaluate, SOLVE, graph) answers NESTED TOO DEEP. Nothing parses it unbounded, because the guard is in the parser.
  - **CAS** checks before handing the text to casim, whose own walk is a recursion (fresh eyes: without the check,
    SIMP of a 30-group equation succeeded, so casim walked 30 levels the guard never saw).
  - **Viewing it and leaving it unchanged** stores it as it was, without the check (fresh eyes: the check before the
    unchanged-edit branch trapped the editor on it).
  - **A 33-stamped one from before version 11** that the 35s parse finds too deep is not restamped 35, so it answers
    SYNTAX ERROR rather than NESTED TOO DEEP. Kept: the 35s parse's answer is unknown past the bound.
- **STU's expression line:** its evaluation answers NESTED TOO DEEP.

## The reference for the hidden sets (tests/eqn_depth.py)

**The rule:** the maximum, over the parse, of 8 × (the depth of `(`, `[` and argument lists) + the active STU
power-chain steps + 2 × (the unary minus groups whose operand is being read), up to the first syntax error.
Refused when > 153; in 33s and 35s modes powers loop, so the chain steps don't count.

`--scan` follows the stamp the core gives each EQ: token: the mode in force (35s first; MODE33, MODE35, STU, MODEL:n),
or in program entry the line's label stamp (the mode when the label was keyed, or a model key on its LBL line). After a
LOADFILE the image's stamps are unknown, so it scans every model. Over the repo's vector sets and the fuzz sets it
flags exactly 047's six refusals. The Python follows the C parser's grammar
(its names come from core/eqn.c), and a host fuzz checks it against the C counter digit for digit.

## Acceptance

- **Every vector file and hidden set byte-identical** apart from expectations past 153 (abacus scans with
  tests/eqn_depth.py and regenerates those).
- **Host:**
  - the deepest call site on a guarded thread stack at 153 runs, and past 153 is refused (tests/deep_test.c). The
    host's thread is 40 KB, not 32: x86-64's frames and its decimal library need 32 to 36 KB here for what the S3
    does in about 28.6 KB of its 32;
  - a fuzz of random and adversarial equations up to 191 bytes asserts the painted-stack maximum ≤ 153 × 128 + base
    for everything the guard admits;
  - eqn_depth.py equals the C counter on every fuzz case.
- **S3, at the deepest site:**
  - 19 `(`, 19 `SIN(`, 17 `X^(`, a 95-step STU chain and a mix at exactly 153, each with ≥ 4 KB free;
  - 154 typed through the keys shows NESTED TOO DEEP with no reset;
  - the pre-fix reset at 30 levels is the positive control.

## Measured after the guard (the S3 stand-in, through the keys)

**The deepest site** (SOLVE of program Z's line, 19 XEQ levels deep; free of the calc task's 32,768 B):

| line typed | units | result | free | B a level |
|---|---|---|---|---|
| 18 groups around a 9-step chain | 153 | X=1 | 4,132 | |
| 19 `SIN(` | 152 | X=0 | 4,820 | 995 |
| 17 `X^(` | 153 | X=1 | 5,140 | 1,094 |
| 19 `(` | 152 | X=0 | 6,228 | 921 |
| 15 `-(` | 150 | X=0 | 10,196 | 902 |
| a 95-step chain | 95 | X=1 | 12,500 | 118 |

- **Every shape at the limit keeps ≥ 4 KB.** The host fuzz's heaviest admitted STU equation (24,000 cases) is
  48 B over these shapes on the host.
- **On the S3 the minus weight isn't needed:** `-(` costs 902 B a level against `(`'s 921. xtensa's windowed calls
  keep parse_neg's frame with or without a minus, so `(` already pays for it; on x86-64 a minusless call is a jump.
  The +2 stays (host and device must agree, and it is conservative); it costs `-(` four levels. **Ruled (abacus #5177): keep the +2**; one rule on both the host and the S3 beats a
  target-only allowance.
- **Refused through the keys, the board answering after:** 18, 30 and 46 levels of `X^(` at the ENTER site (before
  047: 30 reset the board, `rst:0xc`; 46 panicked), and the 154-unit mix in program entry (no line added). 17 levels
  store, with 10,020 B free.
- **Program entry now parses** (the guard's check): the 153 mix leaves 10,212 B free at its ENTER.

