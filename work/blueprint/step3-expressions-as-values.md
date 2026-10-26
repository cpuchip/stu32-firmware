# Unit 082 (step 3, part 1): expressions as values (blueprint; the saved state first)

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
`!= AB_VECTOR`, 6 of `== AB_COMPLEX` and 21 of `!= AB_REAL` across core/ and firmware/. Each gets the new kinds handled,
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
