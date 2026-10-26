# Unit 082 (step 3, part 1): expressions as values (the saved state first)

The blueprint (sections 1 to 9, OK'd by abacus #11161 and #11166) is kept as it was checked, with corrections struck in
place. What was built, and its evidence, follows it under **Built**.

**From:**
- abacus #11101 (point 4) and #11152, on Michael's ruled order of 15 Oct: fractions, the textbook display (080), expressions
  as values, the worksheet; the wheel (081) came between.
- What Michael asked for, from his first look on the phone: an HP 50g-like set. Equations dropped from EQN onto the
  stack, worked symbolically there, names on the stack, stored back. Folders and groups for EQN are new.
- Michael's note on stu-next-when asks for care with forward compatibility and saved states.
- **So this blueprint starts with the image.** No code until abacus checks it. abacus OK'd it as **unit 082** (#11161):
  values, the store, image 21, drop and store-back, and X drawn as 080 draws. 083 is the CAS on the stack, 084 arithmetic
  and EVAL, 085 folders. Its three additions are sections 7, 8 and 9.

## What the state holds today (measured at 95f8778)

- **ab_calc is 111,568 B,** in PSRAM on both boards (`heap_caps_calloc(..., MALLOC_CAP_SPIRAM)`). An ab_val is 64 B: a
  kind, three 16-byte numbers (re, im, p3) and a length. The values live in X, Y, Z, T, LASTx and A-Z (var[27]), and in
  the ALG history's hist_val[16], which is RAM only and not in the image. The equation list is eqn[64][192] (12,672 B),
  programs' equation lines peqn[64][192].
- **The image** ("ABSI" version:u16 0:u16 length:u32 { tag:u16 size:u32 payload } crc:u32) is at version 20, with 21
  sections (T_MODES .. T_CASCPLX). A value is written by wval: kind, len, a mask, and the nonzero parts.
- **The fullest image the calculator can write today is 14,231 B from the core's ab_state_save** (`stackkeys full`: 66
  equations, the longest 191 B, all 26 letters, statistics). The device saves 14,249 B, because dev_image_save appends
  its own 18-byte "ABDS" settings record (radix, grouping, off time, volume, uses, 24-hour), with its own version and
  CRC. Each flash slot is 256 KB (abstate 0x80000, two slots); IMAGE_CAP is 256 KB.
- **What an older firmware does with a newer image:** it refuses it whole. ab_state_load refuses a version above
  AB_STATE_VERSION and a nonzero reserved field (state.c:1227), an unknown tag, and any field out of range. The device
  then does MEMORY CLEAR. **So a downgrade today loses everything:** flashing an older build, or opening the state in an
  older APK. That's the forward-compatibility edge Michael's note is about.

## 1. The values

- **AB_EXPR: an expression.** Its text is STU's, with the × written in (031 rule 7), as an equation's. It's held in an
  expression store in ab_calc: exprs[48][192], 9,216 B, in PSRAM, so internal RAM +0. The value carries the slot's index.
  - 48 slots, because that's every place a value can live: X, Y, Z, T, LASTx, 27 letters, and the 16 ALG history values.
    A stack lift that copies a handle needs no new slot.
  - A slot is freed by mark-and-sweep when a new one is needed: every handle in a live value is marked, the rest are
    free. No reference counts, since the core copies values by assignment everywhere.
  - UNDO's buffers hold images, and images hold texts by content (below), so they keep no handles.
- **AB_NAME: a name,** the letter itself rather than its value: 'A' on the stack, as on the 50g. Its index (0-26) goes
  in len. It needs no store.
- **The equation list itself stays as it is.** Dropping an equation onto the stack copies its text into a new
  expression; storing back copies an expression into the list. The list and the stack never share a slot, so deleting
  an equation can't leave a value pointing at nothing.

## 2. The image, version 21

- **By content, as programs' equation lines are now.** An AB_EXPR value is written as its kind, then its text
  (len:u8 bytes), in place of the number parts. An AB_NAME is its kind and the letter's index. The store's slot numbers
  never reach the image: load allocates slots fresh, in the order the values come.
- **The sections:** T_STACK and T_VARS carry the new kinds. A new T_EQFOLDER carries the folders (section 3). Nothing
  else moves.
- **Size:** at most 48 × 193 B more than today's fullest image, so 14,231 + 9,264 = 23,495 B (the device's save 18 B more),
  far under a 256 KB slot.

## 3. EQN folders and groups

- **Data:** folder names (16 at most, 12 bytes each), and each equation's folder (a byte per equation, parallel to
  eqn_model and eqn_note, as T_EQMODEL and T_EQNOTE are). 0 is the list's top level, so an image without T_EQFOLDER
  loads with every equation at the top level, which is today's list.
- **The wheel (081)** walks the folder that's open. A folder is an entry in its parent's wheel.
- **Michael's to settle before that unit:** how many folders, whether they nest, the name's length, and whether
  "groups" means something other than folders. The image reserves the section, and the sizes above are placeholders.

## 4. Forward compatibility (Michael's note)

Three parts. The first two are recommended now; the third is his call.

- **A. The writer uses the lowest version the content needs.** If nothing in the state uses version 21 (no expression
  or name in a register or letter, no equation in a folder), the writer writes version 20, byte for byte as today's
  writer would.
  - So until the step-3 features are used, a downgrade loses nothing: an older firmware reads the image as it always has.
  - The oracle is strong: the golden images and the `stackkeys full` image must save byte-identically to today's
    (section 6).
- **B. From version 21, the reader skips an unknown section by its size,** instead of refusing the image. The rule is
  written down now: a later version may add a section, but may not change an existing section's meaning.
  - So a version-21 firmware reading a version-22 image keeps everything but what it doesn't know.
  - It can't help firmware already shipped: version 20 and before refuse a future version whatever its content.
- **C. An unknown value kind, in a later image:** loaded as 0, with a notice OLDER FIRMWARE, so the user sees that
  something was dropped; or the whole image refused, as today. **This one is Michael's:** keep the rest of the state at
  the cost of one value, or refuse loudly.

## 5. What step 3 then does (units after this one, not designed here)

1. **The state and the values** (this blueprint): AB_EXPR, AB_NAME, the store, image 21 with A and B. The keys: drop an
   equation from EQN to X, store X back to EQN. The textbook display shows an expression in X as 080's area does.
2. **The CAS on the stack:** derive, integrate, simplify, solve and the rest take X's expression (and a name for the
   variable), and leave the answer in X, as casim's ops do on the list now.
3. **Arithmetic between expressions and numbers** (+ − × ÷ and functions build a new expression), and EVAL with the
   letters' values.
4. **Folders** (section 3, after Michael's answers).

## 6. The oracle for the state (unit 1)

- **Round trip:** every kind in every place (stack, LASTx, letters), saved, loaded, saved again, byte for byte. The
  store refilled at load in the values' order.
- **Older images:** every golden image in tests/ still loads and saves byte-identically as version 20 (A). There are
  goldens for versions 1, 10 and 12 (golden-v1, golden-v10-041, golden-v12-041d, golden-v12-041d-regx), and now version
  20's, written before any 082 code (section 9).
- **A version-21 image:** refused by a build at today's head (it must be: that's today's rule). Read by a version-21
  build with an unknown section added, which is skipped (B).
- **The store:** mark-and-sweep under pressure, all 48 slots live and then one more value, with the oldest dead one
  reused. No live value ever loses its text. This is a fuzz over random key sequences that check every handle's text
  after each key.
- **load_fuzz and store_test,** extended to the new kinds.
- **D68:** internal RAM +0 (the store is in ab_calc, in PSRAM).

## Questions for Michael

- **4C:** an unknown value kind in a newer image. Load the rest, with a notice, or refuse the whole image as today?
- **Folders:** how many, nested or flat, and what "groups" means.
- **The names:** should 'A' on the stack evaluate when an op needs a number (as the 50g's EVAL), or only when EVAL is
  pressed?

## 7. A full store (abacus #11161, addition 1)

- **The status:** ABN_EXPR_FULL, shown as **MEMORY FULL** on X's line, the 35s's own message for a full memory (UG p.F-3,
  already ABN_MEMORY_FULL's text). Like every error, the key changes nothing. No text is dropped or overwritten, and the
  stack, letters and list are as they were.
- **When it can happen:** an op computes its result, then asks for a slot before it commits (transactional), so the
  values it's about to replace are still live while it asks.
  - The case: all 48 places hold distinct expressions, and an op needs a new one before it can let one go. For example,
    a drop from EQN onto a full stack, before T is lost to the lift.
  - So the 49th live expression is refused, not made room for by losing the one leaving. Making room would mean the op
    committed before it knew it could.
- **The image:** a message showing is in the image (STATUS_SINCE), so ABN_EXPR_FULL is new in version 21's table.
- **The oracle:** the store fuzz fills all 48 places with distinct expressions, then asks for the 49th. It must give
  ABN_EXPR_FULL with every text unchanged, byte for byte. Freeing one (CLx on a place) then lets the next succeed.

## 8. An op that needs a number, given an expression or a name (abacus #11161, addition 2)

**Before 084's arithmetic, no op reads an expression or a name as a number.** Measured at today's head:
- The core already refuses complex and vector operands at its dispatch entry. reads_x and reads_y mark the ops that take
  X and Y as numbers, and a complex or vector operand there gives INVALID DATA, changing nothing (calc.c, the guard
  after the entry's bookkeeping).
- **But is_cx compares the kind exactly,** so an AB_EXPR would pass that guard as neither complex nor vector. The real
  path would then read its zeroed number parts as 0. That's the fault this section exists for.

**So 082 adds one guard beside it:** an AB_EXPR or AB_NAME in an operand an op reads as a number gives
**ABN_NOT_NUMBER, shown as "NOT A NUMBER"** (12 cells, X's line at scale 4), changing nothing.

**The op classes, each with what it does given an expression on X, Y or both:**

| class | ops (examples) | given an expression or a name |
|---|---|---|
| arithmetic on X and Y | + − × ÷ y^x x√y % %CHG nCr nPr IDIV RMDR AND… | NOT A NUMBER |
| a function of X | SIN COS TAN LN e^x √x x² 1/x ABS +/− IP FP RND →DEG →HMS… | NOT A NUMBER |
| statistics | Σ+ Σ− x̂ ŷ | NOT A NUMBER |
| stack moves | ENTER x<>y R↓ R↑ LASTx CLx | moves the value (the handle), as for a number |
| store and recall | STO RCL x<>var, VIEW | moves it into or out of a letter |
| store and recall arithmetic | STO+ STO− STO× STO÷ RCL+ … | NOT A NUMBER (either side an expression) |
| tests | x=y? x≠y? x<y? … x=0? … | NOT A NUMBER (084 decides whether x=y? compares texts) |
| indirect | (I) and an index letter holding an expression | NOT A NUMBER |
| the solver and ∫ | SOLVE or ∫ with an expression in a letter they read, or in X as a guess | NOT A NUMBER |
| display settings | FIX SCI ENG ALL, the radix | no effect on it: X shows it as text, drawn as 080 draws |
| base conversions | HEX OCT BIN and their arithmetic | NOT A NUMBER |
| complex and vector entry | i, [ ] with an expression part | NOT A NUMBER |
| EQN's drop and store-back | (new in 082) | drop: an expression from the list's text; store-back: X's expression into the list |

**The audit:** every branch on a value's kind is checked for the new kinds. Measured at today's head: 7 tests of
~~`!= AB_VECTOR`~~ against AB_VECTOR (6 of them `==`, 1 `!=`: corrected at the build, re-measured at b029c45), 6 of
`== AB_COMPLEX` and 21 of `!= AB_REAL` across core/ and firmware/. Each gets the new kinds handled,
or a reason it can't see them. The screen's val_line would show an expression as 0 today; in 082 it shows its text.

**The oracle:**
- an app_test check per class above, with an expression on X, on Y, and on both;
- a fuzz of random op sequences with expressions in random places. After every op, either the op is a move-class op, or
  it returned NOT A NUMBER with the state unchanged (the image before and after compared). No numeric result may come
  from an expression.

## 9. The version-20 golden, written before any 082 code (abacus #11161, addition 3)

- **tests/golden-v20-082.img:** 1,062 B, version 20, saved by the core at b819da0 (today's head, before 082). Its body's
  CRC-32 (the image's trailer) is **455A56E3**. It holds:
  - FIX 4 and RAD;
  - a statistics point;
  - A = 1i2, B = [1,2,3], C = 4;
  - three equations, A^2+B (stamped 33), 2^3^2+X (stamped 35) and 3×X^2-4 (stamped STU, labelled QUOTIENT);
  - CPLX on;
  - X = 7, Y = 8, LASTx = 3.
  The list also holds the two built-in solvers' entries, as every list does. It was built by a scratch program on the
  core's API, then read back: STU, CPLX on, RAD, FIX 4, the stamps 33, 35 and STU, the label, A complex, B a vector of
  three. It loads and saves again byte for byte at b819da0.
- **tests/golden-v20-082-full.img:** 14,231 B, `stackkeys full`'s state (the core's image, cut from its partition image).
  Body CRC **97018F1A**.
- **What 082 checks against them:** each loads, and saves again byte for byte (memcmp with the file), so the writer
  writes version 20 when nothing needs 21 (section 4, A).
- **A note on the existing check:** golden-v1.txt's `%IMAGE=2144DF1C` doesn't pin the image. A CRC-32 over a message
  followed by its own CRC is the constant residue 0x2144DF1C, so every valid image gives it: this golden's whole-file
  CRC is 2144DF1C too. It checks that an image is valid, not which one it is. 082's byte-for-byte check doesn't lean
  on it.

## Built (soroban, 2026-10-26)

Commits on stu32-firmware: 4054ce0 (the store and NOT A NUMBER), fe50c4c (image 21, expr_test), 4e54467 (the screen,
→X and →EQN in the core, the VAR catalog), c3527b0 (load_fuzz, store_test), feab68d (ANS), 61668dc (CAS4's keys, alone),
and this record with VER 0.82.

### The values and the store (sections 1, 7)
- `AB_EXPR` and `AB_NAME` follow AB_VECTOR in ab_kind; an expression's `len` is its slot, a name's its letter (0-26).
- `exprs[48][192]` in ab_calc, after hist_val: so a load (which clears only what is before hist) keeps it, and every slot
  a load doesn't refill is free by the sweep. `ab_expr_new` marks every ab_val in ab_calc (copies included), takes the
  first free slot, else `ABN_EXPR_FULL` with nothing changed. An op asks before it commits (→X asks before the lift).
- **ANS can hold an expression** (a correction to the build's first reader, which refused one there): RPN to ALG sets
  ANS to X. The op sweep found it once it reloaded every state's image. A program's literal can't hold one (no op puts
  one there), and the reader refuses one there.
- Places an expression can be in: X, Y, Z, T, LASTx, the 27 letters, ANS, the message's X, SOLVE's saved stack and the
  evaluation's Y, Z, T (copies of live handles). The history values hold numbers only. 48 slots still bound the
  distinct ones: at most 33 distinct places can hold one at once.

### NOT A NUMBER (section 8): the classes and where each is guarded
| class | where | test |
|---|---|---|
| arithmetic, functions of X, statistics, base arithmetic | the dispatch guard beside the complex/vector one (reads_x, reads_y) | the sweep |
| stack moves, STO, RCL, x<>var, VIEW | move the handle | the sweep (no number read) |
| STO and RCL arithmetic | arith() | the sweep |
| a register that holds numbers only (statistics) | store_val | the sweep |
| tests (x=y? .. x=0?) | prog_test | the sweep |
| indirect: (I), (J), the 33s's (i) | the four index checks | the sweep (I's setup), expr_test (33s (i)) |
| SOLVE: the unknown, the guess; ∫: the limits; f an expression | solve_op, integ_op, fn_value | the sweep, expr_test |
| an equation reading a letter, ANS or (I) | eqn.c's take_val | expr_test (B+1, ⒤+1, B alone) |
| the 33s's CMPLX pairs | cmplx_pair | the sweep |
| the built-in linear solvers | lin_key | expr_test (2x2, 35s mode) |
| GRAPH's window, TABLE's start and step (RPN and ALG), TABLE's start from the variable | their setters | the sweep |
| display settings | no effect: X shows it (below) | screen_test |
| →X, →EQN | new (below) | expr_test, app_test |

**The audit of the 34 kind branches** (re-measured at b029c45; one ring out, the `== AB_REAL` tests too):
- **Changed:** cmplx_pair (901), the four index checks (1012, 1019, 1565, 1572), store_val (1081), prog_test (1508),
  fn_value (2005), solve_op (2698), integ_op (2892), lin_key (3642), the ALG and RPN window and table setters (4427, 4436,
  5437, 5451), TABLE's start (5811), the reader (state.c 572), msg.c's is_zero_val (69: real or complex only), app.c's
  var_set and catalog line (297, 338-339), screen.c's value line (304-305, through val_line).
- **Can't see the new kinds, with the reason:** is_cx and is_vec (132, 171): the guard runs before them; vec_arith (604):
  arith refuses first; graph_f (1911), the graph sample (5886), eqn.c's vector elements (413) and RND's digits (438):
  values from an evaluation, which can't produce one; eng_key (4562): X not real, ENG's view does nothing; SHOW's digits
  (5658): neither real nor complex, nothing shown; msg.c (109-134): message text for errors an expression never reaches
  (NOT A NUMBER comes first); list.c (116-118): a program's literal; the writer's default-value tests (state.c 246, 251):
  an expression is never a default; r->xreal (759): not real, so no ENG view.

### The image (sections 2, 4, 9)
- The writer stamps the lowest version its content needs: 20 unless a value is an expression or a name or MEMORY FULL or
  NOT A NUMBER is showing (STATUS_SINCE 21), then 21. An expression: kind 3, its text's length, mask 0, the text. A name:
  kind 4, the letter, mask 0.
- The reader takes kinds 3 and 4 from version 21, a text's slot the one an equal text met before took (the same value in
  two places loads into one slot). It refuses a kind past 4 (4C, until Michael's card) and skips a section past
  T_CASCPLX by its size from version 21 (a version-20 image with one is refused, as before).
- **The goldens re-save as b029c45 saves them** (a scratch resave program built at both heads, then pinned in
  expr_test by size and body CRC): golden-v1 1,264 B F0E2C760, golden-v10-041 814 B BCC1631F, golden-v12-041d 838 B
  EDEC7390, golden-v12-041d-regx 743 B 6A64DB27, golden-v20-082 1,062 B 455A56E3, golden-v20-082-full 14,231 B 97018F1A;
  the two version-20 goldens byte for byte their files (memcmp).
- **tests/golden-v21-082.img:** 909 B, body CRC 036F56B7 (zlib agrees). X 3×X^2-4, Y the same value, Z the name B, B the
  name i, i the name A, the letters' expressions, SOLVE's saved stack, NOT A NUMBER showing. The core at b029c45 refuses
  it (BAD IMAGE). This build loads it and saves it again byte for byte.
- **The fullest version-21 image measured:** 21,836 B (43 places each a distinct 191-byte expression on
  golden-v20-082-full's state), loading again; the device's save 18 B more. Under section 2's bound of 23,495.
- Not built, waiting on Michael: T_EQFOLDER (folders), 4C's alternative, the names' evaluation.

### The screen (a proposal for Michael's look)
- **X's expression in 2D,** tbook's fit, in the rows under Y's line (112-204): on X's own row when short, growing down
  into the empty rows when not. When it doesn't fit there, X's line shows its text. T, Z and Y stay.
- **T, Z, Y, and a too-tall X show an expression or a name as quoted text** ('3×X^2-4', 'A'), as the 50g writes one, so
  an expression 2 can't read as the number 2. The VAR catalog the same, cut by cells and bytes with an ellipsis.
- **For the phone and the web:** `textbook_on` also marks X's 2D drawing (rows 21-205 from the frame); then all four
  lines keep their text, and `drawn` is X's drawn form.
- Frames 42-47: a short X, a fraction X with Y's expression and Z's name, a wrapped X, a too-tall X (as text), NOT A
  NUMBER, MEMORY FULL. The old 41 frames byte-identical; 22's CRC unchanged (43FC116C).

### →X and →EQN, and CAS4 (abacus #11174; where keys go is Michael's card s3-keys)
- `AB_EQDROP` (→X): the shown STU equation's text as an expression in X, the stack lifted, Equation mode open. Nothing at
  EQN LIST TOP or on a built-in's entry (as ENTER and XEQ there); WHERE on a sum's condition; INVALID DATA on an entry
  stamped 33 or 35 (another mode's grammar); MEMORY FULL with the store full, nothing changed.
- `AB_EQSTORE` (→EQN): X's expression (or a name's letter) inserted after the shown entry, which becomes the shown one,
  stamped STU; X kept. A number in X is NOT A NUMBER (abacus #11174).
- Both nonprogrammable, STU only, the list's keys only (NO EQN outside Equation mode).
- CAS4 after CAS3's ▸, last in the menu list so no index moves: →X, →EQN, its ▸ back to CAS. **After →X the bar
  returns to the list's,** as after the other CAS keys; dropping several takes CAS ▸ ▸ ▸ each time. That and the page's
  place are for the card.

### The oracle's results (make test green at the head)
- **tests/expr_test.c, 276 checks:** the goldens; the round trip of every kind in every place; the reader's rules (an
  unknown section skipped in 21 and refused in 20, out of order refused, an expression in a version-20 image refused, a
  kind past AB_NAME refused shaped as a name, a name past i, a control byte in a text); MEMORY FULL and NOT A NUMBER
  showing alone give version 21; the store at 48 and the 49th; the store fuzz, 200,000 steps (807 refused full, every
  text kept, full exactly when 48 distinct are live); the op sweep, 9,440 runs (every op, args 0, 1, B and (I), an
  expression or a name on X, Y, both, B or I: 1,634 NOT A NUMBER with the stack and letters unchanged, 7,806 through
  with nothing computed from the expression, judged against the same op run with 0 and 7 in its place, every state's
  image loading); the targeted classes; →X and →EQN; the op fuzz, 400 sequences of 25 ops (235 NOT A NUMBER, nothing read
  as a number, every image loading).
- **load_fuzz** seeds golden-v21-082 and sends →X and →EQN: 20,000 cases and 100,000 (seed 7), 0 violations.
  **store_test:** 5,431/5,431 power cuts leave a good image over saves alternating versions 20 and 21; a version-21 image
  with the device's record loads back whole. **screen_test** 9,014 checks, 47/47 frames; **app_test** 801/801.
- **25 mutants, each red** (scratch mut.py, run at the head): the dispatch guard, STO/RCL arithmetic, the program tests,
  an equation's letter, the writer always 21, no dedupe, a store without marking, the unknown section refused, taken in
  20, kinds past 4 taken, ANS refused, MEMORY FULL not raising the version, SOLVE's guess, the 35s's and the 33s's index,
  the window and table setters, fn_value, the complex pairs, the integral's limits, number-only registers, the linear
  solver, the quoted text lines, the catalog, X never in 2D. Four first survived, each a gap in the test, not the code,
  closed before the claim (the v20 section on an image without expressions, a kind-5 shaped as a name, the status-only
  version, SOLVE and (I) needing a function and the I setup).

### What I got wrong on the way
- **ANS:** the first reader refused an expression in ANS on the claim that no op puts one there. RPN to ALG does. The
  sweep went red on it (8 states) once it reloaded images, before the fix.
- The first app catalog cut counted cells but not bytes (28 cells of × are 84 bytes in a 39-byte line); rewritten
  before any test, from its own comment.
- Test-side slips, each caught by a red check: π's lift moved the name I meant for X; the store fuzz never filled the
  store (copies keep the distinct count down; bursts fixed it); an extra EQN toggle left Equation mode; (I) in an
  equation is ⒤, not "(I)"; the 35s's (I) names B by -2; a message showing swallowed the next key.

### The gate (5b48ded)
- **make test** rc 0 at 5b48ded: expr_test 276/276, app_test 801/801, screen_test 9,018/9,018 with 47/47 frames,
  standin_test 349/349, help_test 307 entries, keymap_test 2,880/2,880, load_fuzz 20,000 cases 0 violations, store_test
  5,431/5,431, check-version `0.82, unit 082`. The 25 mutants re-run at this head, each red.
- **The stand-in (device/standin/expr082.txt)**, flashed with 5b48ded on ttyACM1: ver=0.82; after the keys (3×X^2-4 typed
  in STU, CAS ▸ ▸ ▸ →X, EXIT) the state line's X='3×X^2-4', drawn=3X^2-4 and rows 21-205 B8D758BB, the host's (the same
  keys through app.c). SAVE wrote 779 B; after RESTART the version-21 image loaded from flash ("load: 779 bytes ok")
  and the same three held. stack-free at least 28,260. The control failed, as it must.
- **The board found two things the host tests didn't** (the first run, at 3ac0794, matched the frame and the reload but
  failed its console expectations): the console's state line printed X=0 for an expression (it formats X's number
  parts whatever the kind; device/ was outside the kind audit), and its drawn= followed only the shown equation. Fixed
  at fecc3bc: X= is the quoted text, drawn= comes from screen_drawn (compose's rules), and screen_test checks it agrees
  with the page.
- **D68, `scripts/resources.py static b029c45 5b48ded`:** flash +3,136 on the stand-in and +3,232 on Rev A; internal RAM
  +0, IRAM +0, calc_task's frame +0, no internal-RAM symbol changed. At fecc3bc it read internal RAM +8 on the stand-in
  and comp 8,276 -> 8,280 on both (a ninth bool in compose's state); 5b48ded packs the two 2D flags into one byte.
- The claim (#11206, acked #11207, released #11208 at 07:04Z): peak 661.4 MiB.

### abacus's replay (#11230) and its condition
- The replay of fa91521 matched. Plant (1), a reader that names A..Z but not i, went red through load_fuzz's v21 seed.
  **Plant (2), expr_mark without ANS, survived**: nothing made a new expression while ANS alone held one and then read
  ANS's text. The store fuzz's places left out ANS and the copies, and the op fuzz judged number reads and image loads
  only. The code marks ANS; the test didn't show it.
- **Closed:** the store fuzz now runs over every place an expression can be (59: the stack, LASTx, the letters, the
  message's X, SOLVE's and the evaluation's copies, ANS, the history). The op fuzz checks that a slot live before an op and
  after it keeps its text, and its sequences now include an STU equation, seeds in every place, and frequent →X.
- **Plant (2) red twice over:** the store fuzz ("step 43: place 42 lost its text", place 42 being ANS) and, on its own,
  the op fuzz ("fuzz 9 step 1: op 234 arg 12: slot 2's text changed under a live value", →X overwriting ANS's text; 4
  such steps). The same plant without the message's X, susp.var0, ev_t or the history goes red too. The 25 mutants stay
  red; make test rc 0.

### For Michael (through abacus)
- The screen's layout of X's expression under Y, and the quotes on the text lines (frames 42-45).
- CAS4's place and the bar returning after →X (card s3-keys).
- →X on a 33- or 35-stamped entry is INVALID DATA rather than a conversion.
- The word on →EQN with a number: "NOT A NUMBER" is ruled (#11174), but read on that key it says the opposite of what
  happened; "NOT AN EXPRESSION" or INVALID DATA would read true. One line for abacus, not changed here.


### Michael's step-3 answers (abacus #11334)
- **s3-look:** "Looks right (as built)". The screen is accepted as built (frames 42-47).
- **s3-old-firmware:** 4C as section 4 C proposed, "graceful degradation, and I appreciate it". Built below.
- **s3-names:** a name stays symbolic until EVAL (as the 50g: "otherwise as you're trying to build up an equation it'll
  collapse on you to a value"). NOT A NUMBER on a name stays until 084's arithmetic; EVAL is a deliberate key. No change.
- **s3-folders:** 16 folders, one level (unit 085).
- **s3-keys:** prototypes first, (a) EQN LIST's EXIT as a MORE key and (b) a stack submenu, as pictures for Michael.
  INVALID DATA on →EQN with a number goes in with whichever he picks.

### 4C: a newer firmware's image (Michael's s3-old-firmware)
- **The rule (state.h):** an image of a version past this build's (22 on) is read. A section this build doesn't know is
  skipped by its size (B, already built). A value kind it doesn't know is loaded as 0. The kind is written as a later
  build must write one: its kind, len and mask (that kind's own), then a 16-bit byte count and that many bytes.
  **OLDER FIRMWARE** (ABN_OLDER_FW, the message on X's line) shows after any newer image, whether or not anything was
  dropped, since a newer version may also read a field differently. It replaces any message the image had (no op, no
  X), and ab_state_load returns ABN_OK. The state is saved again as 21, the dropped parts gone.
- **Still refused:** a kind past 4 in an image of version 21 or older (no build writes one), and anything else newer: a
  status, an operation code, a field inside a known section. So a later build must add what it needs as a section or a
  value kind, or an older firmware refuses its image (MEMORY CLEAR). That binds every later unit's image design.
- **OLDER FIRMWARE in the image:** STATUS_SINCE 21, so a 21 image may show it. An image saved while it shows is refused
  by builds before this commit (the board's 5b48ded among them): those builds are pre-release.
- **Vector S19 changed:** "a version from the future: refused" is now "read, OLDER FIRMWARE showing" (@A=5
  %STATUS=OLDER_FW). %STATUS is a new expectation: the message showing, by its status's name.
- **The golden, made without this build's writer:** scripts/mk_golden_v22.py rewrites golden-v21-082.img's bytes. The
  version becomes 22. Y (an expression), Z (the name B) and T (a number) become kinds 5, 9 and 200, with 7, 0 and 300
  bytes (a count past 255; len and mask not 0). A section tag 40 of five bytes is added. tests/golden-v22-4c.img:
  1,206 B, body CRC 20009FA1.
- **The oracle (expr_test):** the golden loads, OLDER FIRMWARE showing, with Y, Z and T 0 and X, LASTx and B as before.
  Its image saved again equals, byte for byte, golden-v21-082 loaded, with Y, Z and T set to 0 through the calculator
  and OLDER FIRMWARE showing, then saved (874 B, 9CCA1E94). That is the rest of the state surviving. The resave is
  version 21 and loads, still showing. Refused: the same kinds in a version-21 image, a byte count past the image's
  end, a count one short. Frame 48 is the screen after the load. load_fuzz takes the golden as its ninth seed.
- **Mutants:** 11 in the reader, 10 red: a newer version refused, the payload not skipped, no notice, the kind kept, an
  unknown kind taken in 21, the message's op or X kept, a skip one short, the section's notice gone, a newer image with
  nothing dropped not flagged. M9 (the byte count's own check removed) survives: the image is still refused a few bytes
  later, when the section's end doesn't match. An equivalent mutant; the check stays as a guard.

### A pre-existing bug the sanitized fuzz found: the program pointer past the end
- **Found:** load_fuzz built with ASan and UBSan (by hand; make test has no sanitizer build) gave "negative-size-param"
  in prog_insert. A control build at b2a36e7 (before 4C) fails the same way, so it predates this unit. Two probes
  through the API, with no crafted image, then showed it reachable from keys:
  - **A crash:** a run that a slice stops right after its last line (the slice check comes before the end check) leaves
    pc at prog_len. C between slices stops it there. PRGM then 5 SIN moves memory by a count of -1 line (prog_insert),
    and BKSP does the same in prog_delete. On x86 at -O2 glibc's memmove happened not to fault, and the program came out
    wrong (S31's run gave X=2). Under ASan it aborts. On the device it is undefined.
  - **A lost memory:** GTO a program's last line when it is a test that is false, then SST. The skip leaves pc at
    prog_len + 1. The image is saved with that pc and its own reader refuses it (it allows at most prog_len), so the next
    boot would MEMORY CLEAR. A stop by the slice, C, the budget or flag 5's overflow at that line does the same.
- **Fixed (calc.c):** a run that stops past its last line is clamped to prog_len, "stopped at the end", which the image
  already holds and from which R/S ends the run either way. PRGM, and any program-entry key (for an image loaded in
  program entry), moves a pointer at the end to the last line, so keys record after it and BKSP deletes it.
- **Vectors (work/016):** S27 (SST of a last-line test: the image loads, 3 lines), S28 (after the slice stop and C,
  PRGM 5 SIN: 6 lines, A006), S29 (BKSP: 3 lines, A003), S30 (PRGM shows A004), S31 (the edited program runs: SIN 5
  degrees, Y 2). Each part removed goes red: no clamp fails S27; no entry_pc fails S28 to S31 at -O2 without ASan; at
  PRGM alone fails S30. load_fuzz under ASan and UBSan, 1.8 million cases over six seeds: no violation.
- **D68 (9ba855d to 5e3f85c, 4C and this fix):** flash +288 B on the stand-in (3,638,608 to 3,638,896) and on Rev A
  (3,578,784 to 3,579,072); internal RAM static, IRAM code and calc_task's frame +0 on both; no internal-RAM symbol
  changed. Not run on the board: the core's change is the reader and the run loop, with no new RAM. There is no console
  command to put an arbitrary image in flash, so 4C's real path is the host's (expr_test, frame 48).
- **The shipped build has it (abacus #11415):** S27-S31 at 15a0436 (081 with Casimir 1.3.0) give 0 of 5 vectors, the
  same failures as above. Through the app's own keys, on 15a0436: GOLD 9 then F1 (program entry), GOLD XEQ, A (LBL A),
  BLUE 5 then F1 (x≠0?), GOLD 9 then F1 (out), GOLD 4, A (GTO A), BLUE R/S twice (SST, SST). The second SST leaves
  pc 3 of 2 lines (the screen says TOP). dev_image_save then dev_image_load: REFUSED. At 9a08b6e: pc 2, the load OK.
- **make sanitize (abacus #11415, item 3):** load_fuzz (200,000 cases), expr_test, app_test and every key-vector file
  with a save and load after each key, built with ASan and UBSan; the last step of make test. 37 s on its own (binaries
  built), make test 79 s wall in all from a clean sanitizer build. Its control: with 9ba855d's calc.c it stops at
  prog_insert's negative-size memmove.

### s3-keys built: (a), the list's MORE page (Michael via abacus #11423, #11424)
- **Michael:** "for a stack menu I meant a new stack menu on the more bar for eqn", and "I like having the bare to x and
  to eqn on more". That is (a) as prototyped (work/082-s3-keys), with only →X and →EQN on the page and no DEL. He is
  satisfied that ← deletes the shown entry and EDIT then ← deletes characters, as today.
- **Keys (gen_keymap.py, app.c):**
  - In STU, EQN LIST's sixth key is ▸, opening EQN MORE: →X, →EQN, and ▸ back to the list's own bar. Going back closes
    the menu, so GOLD's REG bank still shows. C leaves the list.
  - In 33s and 35s mode the app shows EQN LIST 35: the same bar with EXIT. Its fifth key keeps the unlabeled CAS item, so
    HELP on it still says STU ONLY (unit 073). help_test caught my first draft, which had dropped it.
  - CAS4 is gone, and CAS3's ▸ goes back to CAS.
- **→EQN with a number in X is INVALID DATA** (ABN_DOMAIN; abacus #11224), not NOT A NUMBER. The list is unchanged.
- **Tests:**
  - app_test: the STU bar's ▸, EQN MORE's keys, →X, →EQN, ▸ back with no menu open, INVALID DATA, C then C, and 33s and
    35s keeping EXIT with no CAS label. 073's EXIT check now presses C in STU. 057's chain goes from CAS3 back to CAS.
  - expr_test's →EQN check expects ABN_DOMAIN.
- **The stand-in's expr082.txt:** the keys are now the list's ▸, →X, C (K5 K0 K42), where they were CAS ▸ ▸ ▸ →X EXIT.
  Replayed through app.c from MEMORY CLEAR, the new keys give X='3×X^2-4', drawn=3X^2-4 and rows 21-205 B8D758BB, the
  same as the old keys at the old head. The old keys on the new tree give 5C857302 (a control).

### UNDO: a key that changed nothing kept its mark (abacus #11428, a bug fixed)
- **Found while measuring ← for Michael:** UNDO keeps one image, marked before every op. A key that changed nothing still
  took it: ← ← ← from a sum's answer deleted the answer and X^K, and the third ← (at TOP) took the mark, so UNDO
  restored nothing and X^K was gone for good. Unit 073 already took the mark back after a refused key (a brief notice).
- **Fixed:** after any key, the mark goes back when the state saves as the mark just taken (app.c undo_keep and
  undo_unchanged, beside 073's refusals, and after MEM's CLEAR). Equality is decided by the new ab_state_same(c, img, n),
  the writer in a compare mode: each byte compared with img's as it would be written, with no buffer. A size or version
  placeholder isn't compared, but its patch is, and so is the checksum. UNDO only ever restores that image, so "the
  image is the same" is exactly "UNDO would do nothing". The device's two undo buffers are unchanged, with no new one.
- **The oracle:**
  - app_test: ← ← ← then UNDO brings back A+B.
  - A sweep: in STU, 35s and 33s, in RPN and in the list (on an entry, at TOP, on the last entry), after a change, each
    face, GOLD and BLUE key that leaves the core's image byte for byte as it was (memcmp, independent of
    ab_state_same) is followed by UNDO, which must give the state from before the change. 1,464 keys, 741 changing
    nothing, 0 failing.
  - At the head's app.c the same tests fail: the ← ← ← case and 42 keys in the sweep (R/S, RTN and SST with nothing
    to run, GOLD ',' and PSE, ← at TOP, ...). 073's own UNDO checks still pass.
  - expr_test: on the states of golden-v1, -v20-082, -v21-082 and -v22-4c, ab_state_same is true for their own image
    and false for every single-byte change of it (header, sizes, version and checksum included) and for a byte short.
  - Mutants: ab_state_same always true (app_test 786/811, expr_test red), and content bytes not compared (801/811,
    red). Every image the writer makes is byte for byte as before (the pinned goldens).
- **Cost:** a second walk of the state after each key. On the host, 115 us beside the save's 112 us for a 14,231 B
  image. Not yet timed on the device (082's gate, with D68).
- **The orphaned WHERE rows** go to Michael as card eqn-delete-rows (abacus #11428). Not built until he answers.

### eqn-delete-rows: a sum's answer deleted with its rows (Michael via abacus #11472)
- **Michael:** "Delete the answer and its rows together, one UNDO".
- **Built (calc.c eqn_delete_entry):** an entry deleted takes the WHERE rows directly after it, in the one key, and so in
  one UNDO step. A WHERE row deleted goes alone. Both ways of deleting an entry do this: ← on it, and its text edited
  to nothing and stored (p.6-8). The cas_rows rollback still deletes one row at a time. The image is unchanged.
- **The oracle (app_test):**
  - A sum's answer with 5 WHERE rows, an entry before it and one after. ← deletes all six, and one UNDO gives the state
    back exactly (by memcmp of the image). ← on the third row deletes it alone; the answer and the other 4 rows keep
    their labels. The answer edited to nothing also deletes the 5 rows.
  - The real path: Σ X^K from 0 to ∞ through the keys, then ← (the answer and ABS(X)<1 both go), then UNDO (exact).
  - At the head's calc.c, 3 of these checks fail (both ← cases and the emptied edit); the row-alone check passes, since
    that behaviour is unchanged. Mutants: the ← path alone reverted fails 2; the edit path alone fails 1; a WHERE row
    taking the rows after it fails the row-alone check.
- **The list has no link from a row to its answer: the rows after an entry are taken as its own.** NEW starts at TOP,
  so it never splits them. A CAS op on the answer does: D/DX X on -1÷(X-1) inserts 1÷(X-1)^2 between the answer and
  ABS(X)<1, and ← on the derivative then takes the sum's row with it (measured).
- **Fixed (abacus #11488): an insert lands after the shown entry's WHERE rows,** never between an answer and its rows
  or among the rows (calc.c eqn_insert). NEW at TOP is unchanged, even when an 081 image left a WHERE row first.
  - Oracle: Σ X^K, D/DX X, then ← on the derivative: ABS(X)<1 stays after -1÷(X-1).
  - The sweep, through the keys, in the real Σ's list (-1÷(X-1), one row) and in a polynomial's (X^2-1, rows X≠1 and
    X≠2, then A+B; FACTR, QUOT, GCD, ∫AB and Σ refuse -1÷(X-1)): D/DX, SIMP, EXPND, COLCT, SUBST, ∫DX, FACTR, ∫AB, Σ,
    QUOT (its two rows), GCD, CANCL, →X then →EQN, a typed equation, NEW; and on a row, a typed equation and →EQN.
    28 inserts, each inserting what it should, every sum's rows still after their answer. At 23f1790's calc.c, 24 fail;
    the 4 that pass are NEW in both lists and the two cases on the real Σ's only row.
  - Σ on an answer that has two rows: its answer after them, its ABS(X)<1 after its answer.
  - Mutants: no TOP guard (NEW over an orphaned first row lands after it) fails 1; an if for the while fails 2 (13 of
    the sweep's 28).
- **Open, measured: an edited row loses WHERE (057, 070) and so splits its sum.** Rows X≠1 and X≠2, X≠1 edited to X≠3:
  ← on the answer then deletes it alone, and X≠2 counts as X≠3's.
