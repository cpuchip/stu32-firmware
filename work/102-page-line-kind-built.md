# Unit 102 built: each page line says what its left text is

**From:** the blueprint work/page-line-kind.md at 660a5d2 (abacus #17044, from tally #17042), with abacus's rulings
in #17048 and h102 frozen in #17055: sha256 03d2a7f6…b892, 11 vectors. Its %PAGE-k halves pass 8/8 at 660a5d2; its
kinds wait for src, with a fault abacus plants at src as their control.
- **The build:**
  - 439ef9c: the source and its tests.
  - 55becb3: the mutants' predictions and their runner, before any run.
  - This commit: the mutant run, D68, this record.
- **VER 0.103.**

## For tally and primer

screen_page gains `char pagekind[SCREEN_PAGE_MAX]`, lowest first as page[], beside pagemark:

| kind | the left |
|---|---|
| `A` | an ALG row's text: speak it through ab_alg_words (unit 101) |
| `O` | an op's name: as written |
| `D` | the dropped line, "n OLDER ROWS DROPPED": as written, worded as a host likes (ruling 1) |
| `-` | none: a typed number, a two-line row's value line, an op this build doesn't name (ruling 2) |

## What changed

- **firmware/screen.c:**
  - pg_texts returns the left's kind with its text: `A` for an ALG row with text, `O` for an op with a name, `-`
    otherwise.
  - pg_line_text gives `D` to the dropped line and `-` to a two-line row's value line. It is the one writer of
    page[i].left.
  - screen_lines stores the kind beside each line.
- **firmware/screen.h:** the field and its comment.
- **Export only:** no state, image or frame change.
  - The field adds SCREEN_PAGE_MAX (8) bytes to a screen_page.
  - On the devices the only screen_page is the bench's (device/dev_bench.h), in PSRAM.

## Tests

- **tests/102-soroban.txt, 9 vectors, 36 expectations,** read by `%PAGEKIND-k` (new in tests/vectors.c, beside
  %PAGEMARK-k), each kind beside its line's %PAGE-k:
  - P01: an RPN page (`-` `O` `-`);
  - P02: unit 100's →X (`O`);
  - P03: an ALG page (`A`);
  - P04: one page with both (`A` `O` `-`);
  - P05: an ALG line on two lines (`A` above, `-` its value);
  - P06, P07: the dropped line (`D`), alone and above the rows after it;
  - K01, K02: 35s and 33s, no page (`?`).
  - Each line was read on a draft build in a scratch worktree before src. The directive is new, so the file can't run
    on 660a5d2.
  - It runs plain and in the sanitizer run, with no save/load pass (the page is RAM).
- **app_test:**
  - Unit 100's damaged row reads `-`, as do the typed numbers around it.
  - **The kind census:** a page built key by key, RPN and ALG in turns of 40 keys on one page, 600 keys. After every
    key, every page line's kind agrees with its text: `A` and `O` have a left, `-` has none, `D` reads "OLDER ROWS
    DROPPED". At least one `A`, `O` and `-` must be seen.
    - 2,991 page lines (A 266, O 633, − 2,092, D 0), 0 wrong.
    - Its keys make no dropped line and, as M2 showed, no two-line row. Those are 102-soroban's P05-P07.

## Results

- **make test** (439ef9c's tree): rc 0.
  - 22,314/22,314 vectors and 61,509/61,509 expectations, summing every `n/m vectors` and `n/m expectations` in the
    output. That's 1d27260's 22,296 and 61,437 plus 102's 9 and 36, twice (plain and san).
  - app_test 919/919; screen_test 67/67 frames; the replay's 51 scripts.
- **Mutants** (work/102-evidence/mutants-run1.txt; host, 2 min 11 s, largest process 214.8 MiB): 5 of 5 verdicts as
  predicted, one list inexact.
  - **M0** (the control): SURVIVED.
  - **M1** (ALG rows `O`): RED on 102 P03, P04, P05, as predicted.
    - I predicted app_test green. It went red too, because the census requires at least one `A` line and saw none.
      My own requirement, overlooked in the prediction.
  - **M2** (a value line keeps its row's kind): RED on P05 only. Left unpredicted for the census, which stayed green:
    its keys make no two-line row.
  - **M3** (the dropped line `-`): RED on P06, P07 only, as predicted.
  - **M4** (an unnamed op `O`): RED on app_test's damaged-row kind check only (1 short), as predicted.
  - The runner built the restored tree again at its end.
- **D68** (`scripts/resources.py static 660a5d2 439ef9c`, work/102-evidence/d68.txt):
  - flash +96 (stand-in) and +32 (Rev A);
  - DIRAM, IRAM and calc_task's frame +0;
  - libmain.a +108 and +32.
  - The containers peaked at 763.6 MiB (36 samples, soroban-* through the shim, 4 GiB cap, --rm).
- **The board:** not run. Nothing is drawn differently (67/67, the replay's 51 scripts).

## For abacus

- **h102:** to run at this record's head.
- **The vector file:** tests/102-soroban.txt.

## After acceptance (abacus #17066, #17067)

- **Accepted** at b62fa9e (abacus dc1e772). h102 11/11 (40/40). abacus's plants:
  - ALG rows marked O takes it to 7/11, every A kind red;
  - the dropped line marked - takes it to 10/11, H08 alone.
- **h102 adopted** as tests/h102-abacus.txt, typed from #17067: sha256
  03d2a7f6922dbfcce8e448258ea9e6dec9fe5cff798a97e6fb9224bb1b28b892, 1,974 bytes, 15 lines, ending in one newline. It
  runs in make test and the sanitizer run, 11/11 (40/40).
- **The flip:** H08's D changed to O fails it, 10/11, exit code 1 (read in its own command).
- **make test:** rc 0, 22,336/22,336 vectors and 61,589/61,589 expectations.
