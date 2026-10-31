# Unit 083: Equation mode's own screen, →X leaves the list, EVAL in 33s and 35s (blueprint)

**From:** abacus #11974, on Michael's eqn-screen ruling (31 Oct, 19:56Z): option 1, with one note. EVAL is abacus's
answer to that note, which Michael may veto. **Blueprint first, no code;** abacus reviews it before the build.

The numbers below come from a throwaway prototype (719ee04, a local commit on 9df97a8, never pushed). It has the three
changes and nothing else: no test edits, no docs. It was measured against 9df97a8 with the same tools as the 1.4.0
repin: a census of every vector's final state, then each other command of `make test` run on its own, then D68. The
unit's code is written fresh from this file, and the prototype is deleted.

**The number.** 083, so VER says 0.83 and a build tells which screen it has. 082's record names the next units 083 (the
CAS on the stack), 084 (arithmetic and EVAL) and 085 (folders). Each moves up by one, and 082's accepted text stays as it
is. The other choice is 082b: it keeps 083 for the CAS, but VER stays 0.82 (check-version reads the digits only).

## 1. Equation mode gets a clean screen in every mode

**Today:** STU's list is clean (080's textbook area). 33s and 35s keep T, Z and Y in view above the list's line, and so
does every mode while an equation is typed, STU included (abacus 054d2ab, stu-eqn-typing-crop.png).

**The rule:** while Equation mode is open and the list or the editor is on X's line, T, Z and Y are not drawn. Their
values and their letters are both hidden. Nothing in the state changes, only compose and the two places that draw what
it builds (screen_render, and screen_lines, which the console and the vectors' text read).
- **Kept on Y's line:** what belongs to the equation, not the stack. That is FACTOR's MAY FACTOR MORE label (048) and the
  CAS step labels, ∫X FROM, ∫X TO, ÷ BY and GCD WITH (056, 057). These are set as k->prompt today, and that stays.
- **Unchanged:** program entry's equation line (PRGM shows the program), ALG (no Equation mode there), TABLE and GRAPH
  (each draws its own view first), SHOW and SYS's view (each returns before the stack lines), and the prompts after
  ENTER, XEQ or EVAL. Those prompts leave Equation mode (eqn_evaluate), so the stack shows with A? on Y, as today.
- **The status band keeps EQN** (screen.c:438). It stays the one place the mode is named.
- **Cost:** one more `:1` bit beside textbook and xexpr in compose's struct, so comp stays 8,276 B. §5 has the
  measured numbers: RAM +0.

## 2. STU's →X leaves Equation mode

**Today** (082, calc.c:3968): "Equation mode stays open, so several can be dropped." C leaves the list and a second C
clears X. Typing after →X types into the list, so `EQDROP 3 ENTER` stores an equation "3" (082's "Not yet").

**The rule:** after a successful drop, eqn_drop clears eqn_mode, as eqn_evaluate does after XEQ. The learner sees the
expression land in X (drawn in 2D under Y, 082 §9), and the bar is the stack's own again.
- **Nothing changes on a refusal:** TOP, a built-in solver, WHERE on a condition, a 33/35-stamped entry, or MEMORY FULL
  all return before the line that leaves.
- **Several drops are now one EQN each:** `EQN ▸ →X`, then GOLD EQN, which reopens the list at the same entry
  (AB_EQN keeps eqn_cur), then ▸ →X again.
- **→EQN is unchanged.** It still keeps X and the list, so a store-back can be followed by another. I see no reason to
  touch it. X is not in view while the list is open, but it never was: X's line holds the list.
- **Measured in the prototype through the app's keys:** STU EQN ▸ →X gives eqn=0, the menu back to the stack's, X
  'A×B' drawn as AB, and T, Z and Y shown again. One C then makes X 0. EQN ▸ →X 3 lifts 'A×B' into Y with X 3 being
  typed.

## 3. EQN LIST 35's fifth key is EVAL

**Today:** in 33s and 35s, the list's bar is ▲ ▼ EDIT NEW (CAS, unlabeled) EXIT. That key opens CAS, and HELP on it
says STU ONLY (gen_keymap.py:267-268, unit 073).

**The rule:** it becomes EVAL, `o("EVAL", "AB_XEQ", "AB_TGT_EQN")`. That is XEQ on the equation shown, through the same
eqn_key path the XEQ key takes in Equation mode (calc.c, case AB_XEQ). It asks for each variable, puts the value in X,
and leaves Equation mode. On TOP it is INVALID DATA, and on a built-in solver it does nothing (p.7-6), as XEQ is today.
While an equation is typed, the bar is EQN TYPE's, so EVAL isn't on it.
- **HELP on it:** help.txt can give it its own page, `[AB_XEQ:AB_TGT_EQN]` (gen_help tries the op with its argument
  first; the XEQ key's own arg is 0, so XEQ's page is untouched). Proposed text: "EVAL: the equation shown, as XEQ:
  asks each variable, the value in X; leaves the list."
- **CAS's page loses its "33,35: STU ONLY" line, and menu:CAS leaves help.txt's MODE line.** No state in 33s or 35s
  reaches CAS once this key is EVAL, and help_test refuses mode text that no state shows (measured: entry 213).
- **A naming note, for abacus to weigh:** the next units' arithmetic-and-EVAL unit (082's 084) plans an EVAL for an
  expression in X in STU. This EVAL evaluates the shown equation. The two meanings are near each other, but they are
  different keys in different modes.
- **Measured in the prototype**, 33s and 35s, A×B shown with 7 8 9 on the stack: EVAL gives A? then B?, and 2 R/S 5 R/S
  leaves X=10 and eqn=0. That is the same as XEQ.

## 4. What moves (measured, the prototype against 9df97a8)

**The vector census.** I ran the 90 `./build/vectors` commands of the test target with AB_IMGCRC=1 in both trees, which
gives the final image's CRC of every vector. 3,256 vector runs were compared: every file's plain run, and the
store-and-reload runs that the recipe names one by one. The KEYV loops (slices, save-and-load) rerun the same files,
and the census doesn't repeat them.
**Only 082-soroban.txt moved: X1, X2, X3, X4, X5, X7 and X8**, in both the plain run and the AB_SAVELOAD run. Their
images differ because eqn_mode is off. No other vector in the corpus presses EQDROP, and the screen change touches no
state.

Of these, five fail and two only move their CRC:

| vector | today | at 083 |
|---|---|---|
| X1 `EQ:X^2+1 EQDROP` | passes | passes (the CRC moved: Equation mode closed) |
| X2 `… EQDROP EQ:A÷B EQDROP` | passes | passes (EQ: reopens the list) |
| X3, X4, X5 `… EQDROP C` | C leaves the list | C clears X: the C comes out, the expectations stay |
| X7 `… EQDROP C 3` | | the same: `EQDROP 3`, the same expectations |
| X8 `… EQDROP EQSTORE` | | →EQN outside the list is NO EQN: becomes `EQDROP EQN EQSTORE` |

The header lines 5-6 ("None of these expects where →X leaves the list") are rewritten.

**New vectors** (082-soroban.txt). X10, X11 and the rewritten X7 and X8 were run in both trees: each fails at
9df97a8 and passes in the prototype, in the plain run and the AB_SAVELOAD run. At 9df97a8, X11 gives %EQN=2: the "3" is
stored as an equation.
- **X10, the one abacus asked for:** `STU EQ:X^2+1 EQDROP C`, expecting `X=0 %XEXPR=none`. →X, then C clears X, and
  only because the learner chose to. Today the same keys leave X as the expression (C only leaves the list).
- **X11:** `STU EQ:X^2+1 EQDROP 3 ENTER`, expecting `X=3 Y=3 %YEXPR=none %EQN=1`. No equation "3" is stored, which closes
  082's "Not yet".
- **A direct expectation, %EQNMODE (0 or 1), proposed:** no expectation reads Equation mode today, so X10 proves it
  only indirectly. X1 would add %EQNMODE=0. This is a runner change, red then green as %XEXPR was.

**Elsewhere in make test** (each command run on its own in the prototype; everything not listed is green):
- **screen_test:** frames 14-equation (33s list), 15-equation-typing (33s typing) and 33-tb-typing-stu (STU typing)
  differ. All 9,020 checks pass, so no check pinned the stack in Equation mode. The three are re-rendered, and new
  checks are added: in STU, 33s and 35s, list and typing, screen_lines gives T, Z and Y empty with no letters, and a CAS
  label (∫X FROM) still shows on Y while typing.
- **standin_test:** 05-equation-bar's CRC (typing, with RCL's prompt) moves. It is recorded new.
- **app_test, 13 checks:**
  - 082's EQN MORE walk, 4 checks: "→X: … Equation mode kept, back to the list's bar", and the three after it that
    assumed the list was still open (→EQN, ▸ back, →EQN with a number).
  - The 082 bar checks: 33s and 35s keep EXIT, no CAS (2).
  - "▲ ▼ EDIT NEW - EXIT" (1).
  - CAS 004, 33s and 35s blank (2).
  - 073, CAS says STU ONLY in 33s and 35s (2).
  - 080's "EQN LIST TOP in 33s mode as before: the stack and its line" (1).
  - eqn-delete-rows' insert sweep (1): its →X →EQN step now needs EQN between the two keys. Four of its 28 inserts
    fail.

  Each is rewritten to the new rule, not deleted, and the 073 pair becomes "HELP on EVAL shows its page".
- **expr_test §7 (→X and →EQN):** its "Equation mode and the list as they were" check flips, and the →X →EQN sequence
  after it ran on into a NULL strcmp (a crash in the test, not the firmware). It is rewritten with EQN between the two.
- **help_test:** entry 213 (§3).
- **The UNDO sweep (app_test, 082)** still passes: 1,464 keys, 0 failures. 729 keys change nothing, against 741 today,
  because the EVAL key now acts.

**Not moved, to re-check at the build:** the op fuzz (expr_test, line 877) and load_fuzz press EQDROP, and both pass
in the prototype. But →X now leaves the list, so about half the fuzz's →X presses meet NO EQN. abacus's plant (2) from
082 (expr_mark without ANS) went red only once the sequences made expressions often. **The gate re-runs that plant and
its four siblings at 083's head, and they must still go red.** If they don't, the fuzz gets an EQN before each →X.

**Outside this repo:** tutor's G2 (#11886) moves with §2. Any render of the EQN screen that tally or primer keep moves with
§1. Which ones they keep is theirs to say; this repo's are the three frames and the stand-in CRC above.

## 5. Bytes (D68, resources.py static 9df97a8 719ee04)

| resource | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 3,657,600 → 3,657,744, **+144** | 3,597,104 → 3,597,232, **+128** |
| internal RAM static (DIRAM) | 211,847, +0 | 219,798, +0 |
| IRAM code | 59,022, +0 | 78,086, +0 |
| calc_task's frame, B | 144, +0 | 128, +0 |

Internal-RAM symbols: none changed on either board (164,823 and 137,741). The unit's code adds comments and tests
beside the same lines, so its D68 is measured again at its head; anything above the prototype's figures by more than a
few bytes gets explained in the gate.

## 6. Docs in the unit's commit
- calc.c's eqn_drop comment ("Equation mode stays open…").
- help.txt's →X line ("…in X; leaves the list").
- 082's "Not yet" bullet on where →X leaves the list closes with a pointer here; nothing else in 082's record changes.
- 082-soroban.txt's header.
