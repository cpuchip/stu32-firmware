# STU's indirect addressing (blueprint)

Asked by abacus (#14679, then #15019 after decision 101). Decision 100 chose "The simpler STU model": letters by name,
unnamed registers 0 and up, i as the index, and a clear NO REGISTER message. RPL goes to a future mode of its own
(decision 101), so this stays the simple model on today's keys. STU stays at 4 levels, so nothing here touches the
stack. No src. Everything is measured at 6a2f7a4 unless it says otherwise.

## Michael's question, answered plainly

> "I assume 0 and 27+ are unnamed registers, where as 1-26 are the named one?" (his 10-08 note, as abacus quotes it
> in #14679)

**In STU, no number names a letter.** Every number in i names an unnamed register: 0, 1, 2 and up to 800. So 1 is
register 1, not A. The letters A to Z have no numbers; you reach them by their own keys (STO A, RCL B).

Where his reading comes from: it is close to the 33s. The two machines number differently, and STU today follows the
35s.

| | 33s mode (unit 025, 33s p.13-21) | 35s mode, and STU today (35s UG p.14-21, 14-22) | STU, proposed |
|---|---|---|---|
| the index | i | I or J (ordinary letters) | i |
| A to Z | 1 to 26 (sign ignored) | −1 to −26 | by name only |
| i itself | 27 | — | by name only |
| statistics registers | 28 to 33 | −27 to −32 | by the SUMS menu, not by number |
| unnamed registers | none | 0 to 800 | 0 to 800 |
| anything else | INVALID (i) | INVALID (I) | NO REGISTER |

## 1. The model

- **The index is i:** the 27th variable unit 025 added for the 33s, separate from the letter I. It's stored,
  recalled and cleared as the letters are. In STU, I and J become ordinary letters with no special role.
- **(i) names unnamed register n,** where n is i's integer part (a fraction is truncated, as now: unit 005, ours).
  - The registers run from 0 to 800 (AB_IND_MAX, the 35s's range), and are defined from 0 up to the highest nonzero one,
    as now (35s UG p.14-24). Storing a nonzero value above the top extends the range; storing 0 in the top register
    releases it.
- **NO REGISTER** is the one message for an index that names no register:
  - i negative;
  - i above 800;
  - i complex, or a vector;
  - reading (RCL, x↔, VIEW, ISG and DSE, RCL arithmetic) a register above the defined range, as the 35s errs there
    (35s UG p.14-24).
  - An expression in i stays NOT A NUMBER (082's guard), as now.
- **Where (i) works in STU:**
  - STO and RCL, and their arithmetic forms;
  - x↔;
  - INPUT and VIEW (the prompt shows "(5)", as screen.c:517 names an unnamed register now);
  - ISG and DSE.
- **Where it doesn't:**
  - **SOLVE (i) and ∫ (i).** The 35s can't solve or integrate for an unnamed variable (35s UG p.14-23), and in STU
    (i) names only unnamed registers, so the prompt doesn't offer them.
  - **XEQ (i), GTO (i) and FN= (i).** A label is a letter, and STU's numbers don't name letters. This one is a real
    loss: a 35s program can branch by number (−1 to −26 as labels A to Z), and STU's couldn't.
    - **(a), my lean:** not offered in STU. The model stays one sentence: numbers name registers, names name
      letters and labels.
    - **(b):** i from 1 to 26 names label A to Z, for labels only. That's unambiguous, since labels have no
      unnamed counterpart, but it puts numbered letters back in the one place.
    - It's Michael's choice. A computed branch is advanced programming, and STU is the learner's mode.
  - **The statistics registers by number.** The SUMS menu still recalls them. Storing into one directly becomes
    impossible in STU (the 33s and 35s reach them through the index). Only a program that edits a Σ register in place would notice.
- **Room for later, without a promise:** 082's AB_NAME (a letter as a value, 'A') would let i hold a name, and (i)
  could then address that letter: letters by name, through the index. No key makes a name today (tests/082-soroban.txt
  says so in its header), so a name in i stays NOT A NUMBER until a unit gives names a key. That is the RPL mode's
  ground (§5), not STU's.

## 2. The keys

- **STU takes the 33s's keys from unit 025,** unchanged: in a variable prompt, the key that gives (I) in 35s mode
  gives **i**, and the key that gives (J) gives **(i)**. The prompt shows `STO (i)`.
  - The code already does this for the 33s (app.c:965: `!ab_rec35(a->c)`). The change is the test: the model in force
    is 33s or STU (`ab_rec_model`), not "not 35s".
  - The same holds in program entry, by the edited label's stamp (unit 041 C), as now.
- **In a target prompt** (XEQ, GTO, FN=), the indirect keys give nothing in STU under (a), or (i) under (b).
- **In an equation,** STU reaches (i) by the indirect glyph ⒤ alone. A lone i in an STU-stamped equation isn't the
  variable i (eqn.c:643 reads it so only in the 33s stamp), and ⒥ has no meaning in STU: NO REGISTER.
- **33s and 35s modes: unchanged.** The 35s keeps I, J, the negative letters and 0 to 800; the 33s keeps i and 1 to 33.

## 3. What already exists that this changes

- **The resolver** (calc.c:1017 `resolve`, and `ind_target` at 1596): it branches on `ab_mode35`, which is true in
  STU (it's "35 or STU" for a label: calc.c:5758). STU gets its own branch: i, 0 to 800, NO REGISTER. The 33s and
  35s branches stay as they are.
- **The message:** a new status, ABN_NO_REGISTER, shown as "NO REGISTER" (msg.c). STATUS_SINCE gives it the new image
  version, so an older build reads an error recorded with it as the existing rule for an unknown status.
- **The vectors that use indirect in STU: two, both mine** (grep over tests/*.txt, abacus's sets included):
  - **029 Z05**, `MODE33 … LBL:A STU 5 STO:I 7 STO:(I) … XEQ:A | @#5=7`. Its STU-stamped label sets the letter I,
    and under the STU model its (I) reads i (0), so it would write register 0. I'd amend it in place to `STO:i`,
    with a note pointing here.
  - **096 C09**, `STU 30 STO:I 4 STO:(I) CLX RCL:(I) | X=4`. It passes either way, since it writes and reads one
    register, but it would quietly test register 0. The same amendment.
- **Programs and equations already saved** with STU-stamped (I) or (J) lines. They were written under the 35s model,
  and the same op code reads differently now.
  - **The rule, as unit 025's for the 33s:** an image from before this unit loads with every STU-stamped label that
    holds an indirect line restamped **35**, because 35 is how those lines behaved. The listing shows it (`LBL A ·35`).
  - Restamping is safe on what I've checked: STU-only ops test the calculator's mode (`c->stu`, e.g. calc.c's ERF
    guard), not the label's. The build lists every use of a label's STU stamp (13 lines match `ab_rec_model` or
    `== AB_MODEL_STU` in core and firmware) before trusting that.
  - **Telling old from new:** the writer stamps the lowest version its content needs (082), so the version alone can't
    say an image predates this unit. As 096 did for T_ROUNDED, an image is written at the new version whenever any
    label or equation is stamped STU, or the mode is STU. An older-version image with an STU label is then known to
    be pre-unit.
  - **STU-stamped equations with ⒤ or ⒥** can't be restamped, since a stamp also sets their syntax. They follow
    STU's model from then on. None is in the suite. Whether a lesson holds one is tutor's walk diff to find.
- **For tutor:** STU lessons that use (I), (J), ISG or DSE through an index, or a numbered letter. abacus's route
  for depth (tutor's walk diff against a prototype) fits here too, once the build exists. I can't read the lessons.

## 4. Cost

- **RAM:** +0. i and the 801 registers already exist (calc.h:328, `ind[AB_IND_MAX + 1]`).
- **Flash:** a branch in two functions, a message and a key test; D68 measures it at the build.
- **The image:** one new version, for the marker above and the new status; no new section.

## 5. What an RPL mode would add (a later option, as stu-depth §8)

Michael's 10-08 note: "I want full RPL support so we may want to do something a little different? how can we fit
that into our keyboard?" (quoted by abacus in #15019). With RPL ruled to a mode of its own (decision 101), STU's model
shouldn't bend toward it. What that mode's storage would be, from the 50g's guide:

- **Named variables of any name,** not 26 letters: a quoted name and STO (50g UG p.2-47, creating variables). That
  needs a way to type names: the alpha layer, and somewhere to show them.
- **Directories** holding those variables (p.2-35 to 2-43).
- **No numbered registers.** Indexing is done on objects: a list or an array and GET and PUT with an index (p.10-6).
  A loop over storage is a loop over an array's elements, not over register numbers.
- **Local variables** inside a program (p.21-2).
- **The keyboard question** is that mode's own: names and directories need alpha entry and a variable menu. STU's
  keys don't change for it. The one bridge already in place is 082's AB_NAME, a letter as a value on the stack, which
  is the 50g's quoted name in miniature.

## 6. The test, for the build

1. **Vectors** (the unit's own):
   - i at 0, 5, 800, 801, −1, 5.7 (truncated), a complex number;
   - reading above the defined range;
   - each op in §1's lists;
   - INPUT and VIEW's prompt text;
   - SOLVE (i) and ∫ (i) not offered;
   - the keys (the (I) key gives i, the (J) key gives (i), and `STO (i)` on the prompt);
   - 33s and 35s unchanged beside each STU case;
   - an STU equation with ⒤ and with ⒥;
   - Z05 and C09 amended.
2. **The image:**
   - a pre-unit image with an STU label holding (I) loads stamped 35 and still writes the register it wrote before;
   - a new STU image is at the new version;
   - a 35s image with no STU label is unchanged, byte for byte.
3. **Mutants, predicted first:**
   - STU falling through to the 35s branch;
   - a negative i accepted as a letter;
   - the range check at 799 or 801;
   - the restamp skipped;
   - the version marker skipped;
   - the key test left at "not 35s".
4. **D68** at the build. No frame changes beyond the message and the prompt text, so screen_test's new frames are the
   message and `STO (i)` only.

## For abacus

- **To judge:**
  - the model in §1;
  - NO REGISTER's cases;
  - (a) or (b) for XEQ, GTO and FN= (i), which looks like Michael's;
  - the statistics registers by number dropped in STU;
  - the restamp rule and the version marker for saved programs;
  - the Z05 and C09 amendments.
- **To put back to Michael:** the plain answer at the top. His 1 to 26 are the 33s's numbers; in STU they're
  registers 1 to 26, and the letters go by name.
