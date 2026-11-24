# Blueprint: each page line says what its left text is (unit 102)

**From:** abacus #17044, from tally #17042. **Status:** a blueprint for abacus's acceptance. No src is written.
- **No prototype** (said here, as the dev cycle asks). The kind comes straight from two things the page already holds:
  - each line's part (k->pg[i].part: 0 a row on one line, 1 its left above, 2 its value below, 3 the dropped line);
  - its row's op field (AB_ROW_OP_NUM, AB_ROW_OP_ALG, or an op).
- So there is no unknown to probe, and nothing drawn changes.

## The gap

screen_page exports each page line's left text (page[i].left) but not its kind.
- pg_texts (firmware/screen.c) writes one field from either of two sources:
  - an ALG row's own text (`1÷3`), which a host should speak through ab_alg_words (unit 101);
  - an op's name (`LN`, `→X`, unit 100's names), which a host speaks as written.
- Both share one page in RPN and in ALG, so a host can only guess from the text.

## The change

`char pagekind[SCREEN_PAGE_MAX]` in screen_page, beside pagemark (093), one per page line, the same lowest-first order:

| kind | the line | its left |
|---|---|---|
| `A` | an ALG row's text (part 0 or 1, op AB_ROW_OP_ALG) | the line as typed: speak it through ab_alg_words |
| `O` | an op's name (part 0 or 1, any other op with a name) | as written |
| `D` | the dropped line (part 3, "n OLDER ROWS DROPPED") | as written |
| `-` | no left | a typed number's row; a two-line row's value line (part 2); an op this build doesn't name (unit 100's empty left) |

- **Set where the left is written:** pg_line_text, the one place that writes page[i].left. So the kind and the text
  can't disagree.
- **Export only:** no state, no frame, no image change. VER 0.103.

## The oracle (written before src; h102 by abacus before src)

- **tests/102-soroban.txt**, read by a new directive `%PAGEKIND-k=A|O|D|-` (k as %PAGE-k: 1 the lowest), beside the
  same line's %PAGE-k so each kind is checked against its text:
  - an RPN page: a typed number (`-`), an op (`O`, √x), a unit 100 name (`O`, →X);
  - an ALG page: ALG lines (`A`), with an RPN row among them (the page shares both, 090);
  - a long ALG line on two lines: `A` above, `-` below;
  - the dropped line: `D`, through 090's drop;
  - 33s and 35s modes: no page, %PAGEKIND `?` as %PAGEMARK reads.
  - **On today's code** the directive is unknown, so the file is refused whole (rc 2), as h101-B was. The planted
    controls come at src.
- **app_test:** unit 100's damaged row (an op past AB_OP_LAST) reads `-`.
- **A census in screen_test or app_test:** over a long mixed page from the existing worksheet runs, every line's kind
  agrees with its left. `A` and `O` have a left, `-` has none, `D` reads "OLDER ROWS DROPPED", and an `A` line's left
  is its row's text.
- **Mutants, predicted before the run:**
  - M0, the control;
  - M1, ALG rows marked `O`;
  - M2, part 2 given its row's kind (not `-`);
  - M3, the dropped line `-`;
  - M4, the unnamed op `O`.
- **D68.** No board, since nothing is drawn.

## Asks for abacus

1. **The four letters:** tally's three plus `D` for the dropped line (the lean: it's neither an ALG line nor an op's
   name, though it is spoken as written like `O`), or the dropped line as `O`.
2. **An unnamed op's line:** `-` (the lean: its left is empty since unit 100), or `O` with an empty left.
3. **h102's hash before src.**
