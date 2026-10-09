# Work unit 049: the parser's frames, slimmed

**From:** soroban (047's (b), proposed #5296) · **Ruled by:** abacus #5297 (option (i): keep 153 units, bank the
margin) · **Status:** built and measured.

## Why

**Each nesting level of an equation costs stack** (047): on the S3, 921 B a level of `(` and 1,094 of `X^(`, measured
through the keys. 047 bounded the nesting at 153 units so that the deepest site keeps 4 KB of the calc task's 32. This
unit makes each level cheaper and keeps the bound, so the margin grows. Every result stays byte for byte as it was.

## What the frames held

**parse_prim's frame reserved room for branches most levels never take:** a vector's three values (192 B), RND's
second argument, a two-argument function's second, an indirect read (64 B each), and number reading. gcc gives a
function one frame for all its branches, so every `(` paid for all of them.

**The change:** those branches each go into a function of their own, never inlined: parse_vector, parse_rnd2,
parse_k2, parse_ind, and parse_number. Their values take stack only while that branch runs.

**An operation's application stays inline** (kop, apply0, apply2). Out of line it saved 112 B more a level (576 in
all), but an equation integral ran 2.1% slower, past the unit's 2% bar (below). Inline, both benches ran faster than
before.

## The frames, before and after (-fstack-usage, -O2)

The S3 is xtensa-esp32s3-elf-gcc at the device build's -O2 (CONFIG_COMPILER_OPTIMIZATION_PERF); the host is this
box's gcc. "inlined" means the function has no frame of its own (it is part of its caller's).

| function | S3 before | S3 after | host before | host after |
|---|---|---|---|---|
| parse_prim | 400 | inlined | 432 | inlined |
| parse_post | 32 | 240 | 32 | 112 |
| parse_neg | 112 | 112 | 144 | 144 |
| parse_pow_r | 112 | 112 | 128 | 128 |
| parse_mul | 176 | 176 | 224 | 224 |
| parse_add | 160 | 160 | 192 | 192 |
| parse_all | 160 | 160 | 192 | 192 |
| parse_vector | inlined | 240 | inlined | 272 |
| parse_rnd2 | inlined | 112 | inlined | 144 |
| parse_k2 | inlined | 160 | inlined | 192 |
| parse_ind | inlined | 96 | inlined | 112 |
| parse_number | inlined | 224 | inlined | 224 |
| kop | 96 | 96 | 112 | 112 |
| apply0, apply2 | inlined | inlined | inlined | inlined |
| binop | 96 | 96 | 112 | 112 |
| eqn_eval | 336 | 336 | 352 | 352 |
| depth_parse | 336 | 336 | 336 | 336 |

**A level of `(`** is parse_post (with parse_prim) + parse_neg + parse_mul + parse_add:
- **S3:** 400 + 32 + 112 + 176 + 160 = **880 B before, 240 + 112 + 176 + 160 = 688 after (−22%)**.
- **Host:** 1,024 before, 672 after; measured by the painted stack, 1,008 and 656.

## Measured on the S3 (the stand-in, through the keys)

**047's deepest site** (SOLVE of a program line 19 XEQ levels deep), free of the calc task's 32,768 B, before (ffa443f)
and after:

| line typed | units | before | after | gained |
|---|---|---|---|---|
| 18 groups around a 9-step chain | 153 | 4,132 | 7,588 | 3,456 |
| 19 `SIN(` | 152 | 4,676 | 8,324 | 3,648 |
| 17 `X^(` | 153 | 4,996 | 8,228 | 3,232 |
| 19 `(` | 152 | 6,084 | 9,912 | 3,828 (201 a level; the frames say 192) |
| 15 `-(` | 150 | 10,052 | 13,076 | 3,024 |
| a 95-step chain | 95 | 12,356 | 12,276 | −80 |

- **The worst at 153 units now leaves 7.6 KB,** where 047 left 4.1. The proposal's "roughly 9 KB" assumed the
  all-out-of-line variant (576 B a level), which the speed bar rejected.
- **The power chain doesn't nest through groups,** so it gains nothing. Its leaf now runs parse_post's larger frame
  (parse_prim inlined into it), hence 80 B.
- Every result is the same as before (X in each row).

**Speed** (EQNBENCH, new in the stand-in: SOLVE of X^3-2×X-5 and the integral of SIN(X)^2+X×EXP(-X) from 0 to 1,
each the best of five, three runs; PROGBENCH beside them, which does not parse):

| bench | before | all out of line (rejected) | chosen |
|---|---|---|---|
| SOLVE, µs | 62,384–62,500 | 62,363–62,684 | 61,286–61,564 (−1.8%) |
| integral, µs | 89,575–89,634 | 91,420–91,718 (+2.1%) | 89,127–89,199 (−0.5%) |
| PROGBENCH SIN / LN, ms | 357 / 543 | 355 / 540 | 354 / 538 |

The results are the same in every column (X = 2.0945514815…, 0.5369167609…).

## The oracle

- **Byte-identical:** every vector and fuzz set in make test and make accuracy, unchanged. abacus reruns the 57 hidden
  files and the 048 carry.
- **tests/eqn_stack_fuzz.py, tightened:**
  - **Per unit:** 120 B, down from 132. Measured 116–117 over eight seeds; 125–129 before.
  - **Per level (new):** a level of `(`, `SIN(` and `X^(`, from 10 and 19 levels, at most 720 B. Measured 656, 656
    and 610; 1,008 before.
- **Mutants:**
  - parse_vector re-inlined: a level of `(` is 864 B, red.
  - Every helper re-inlined: 992 B, red.
  - The per-unit bound alone misses the first, because its slack absorbs the 200 B. That is why the per-level check
    exists.
- **The frames above** are the record: a later slip shows as a diff against this table.
