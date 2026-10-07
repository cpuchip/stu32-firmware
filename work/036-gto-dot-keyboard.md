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
