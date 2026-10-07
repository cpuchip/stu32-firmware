# Work unit 036: GTO . from the keyboard (GTO . . to PRGM TOP, and GTO . to a line)

**From:** abacus · **To:** soroban · **Status:** queued after 033 (a faithfulness gap found by tutor, fn-03)

## Why

The 33s guide, p.13-6 ("Using GTO from the Keyboard", read from the rendered page):
- "To PRGM TOP: [shift] [GTO] [.] [.]";
- "To a line number: [shift] [GTO] [.] label nnnn (nnnn < 10000). For example, [shift] [GTO] [.] A0005";
- "To a label: [shift] [GTO] label, but only if program entry is not active".

The core already has the operation: AB_GTOPOS with an address or AB_PC_TOP (prog_goto_pos), and the runner's
`GTO.:A005` token reaches it. But the device's GTO prompt (app.c, the KM_ARG_TARGET letter layer) takes no `.`.
At 867ddd5, GOLD GTO . stays at `GTO _`. So neither form can be keyed on the device.

## Rules

1. **At the GTO prompt, `.` opens the address form** (`GTO ._`).
2. **A second `.` goes to PRGM TOP** (AB_GTOPOS AB_PC_TOP). It doesn't run, doesn't record, and works in and out of
   program entry. A stopped program's pointer is moved, so after GTO . . new program lines go in at the top (tutor's
   fn-03 trap).
3. **A label letter then the line's digits goes to that line.** NONEXISTENT if there's no such line.
   - **The digit count:** read the 35s's own page for its form. Our listing shows 3 digits (F001), while the 33s's
     example shows A0005. Say which each mode takes and why.
4. **`GTO .` is never a program line** (it's the keyboard form; a programmed GTO takes a label only). In program
   entry it moves the pointer and records nothing.
5. **C or ← backs out** of the address form, as the other prompts do.
6. **The 35s:** read its "Using GTO from the Keyboard" pages and do what they say. Quote them in your report.

## Acceptance

- **Vectors at the device's key path** (keyrun / app_test, since the runner's token already reaches the core):
  - GTO . . from a stopped program, then a new label keyed in landing at the top;
  - GTO . . in program entry;
  - GTO . to an existing line and to a missing one;
  - backing out.
- **Mutants:** the second `.` ignored; GTO . recorded as a line.
- **No change** to any accepted vector.

## Accepted (abacus, 2026-10-07, 7c96617; red 82c3627, green 21d21f4)

- **abacus's build:** app_test passes 427/427 (the fn-03 trap included), and the hidden sets still pass (TABLE,
  ALG right to left, 028 exact, GRAPH).
- **The digit counts** are accepted: three in 35s and STU (35s p.14-6, "line number < 1000"), four in 33s (p.13-5,
  "nnnn < 10000").

## 036b, ruled (soroban's proposal, read by abacus from the rendered pages)

- **35s p.14-6:**
  - "To a specific line number: GTO label line number (line number < 1000). For example, press GTO . A 0 0 5. For
    example, press GTO A 0 0 5."
  - "If you want to go to the first line of a label, for example A001: GTO A ENTER".
- **35s p.13-10:**
  - "To execute a program from its beginning press XEQ label ENTER. For example, press XEQ A ENTER."
  - "XEQ label Line number, for example XEQ A 0 0 5."
- **So in 35s mode, GTO and XEQ take a letter, then ENTER (line 001) or three digits.** That's the faithful 35s
  (decision 46). Today they act on the letter at once, which is unfaithful.
- **STU follows the 35s** (029 rule 1: STU behaves as 35s until its own calls are ruled). Whether STU should keep
  the one-key XEQ A as a convenience is an STU call for Michael, later. Until then STU is the 35s.
- **33s mode is unchanged.** The 33s's GTO label acts on the letter (p.13-5/13-6).
- **This changes accepted key paths** in 35s mode only. The runner's GTO:/XEQ: tokens reach the core directly and
  don't move. tutor's harness, which presses printed 33s keys in 35s mode too, must add the 35s's ENTER. tutor is
  told.
- **Tests:** the four forms (GTO A ENTER, GTO A nnn, XEQ A ENTER, XEQ A nnn) in 35s and STU; 33s unchanged; ← steps
  back. **Mutants:** 35s acting on the letter alone; 33s waiting for ENTER.
- **Queue:** after 038 and 037.
