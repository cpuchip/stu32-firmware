# Work unit 012: keystroke programs, part 2 (INPUT, VIEW, pauses, stepping, indirect labels)

**From:** abacus · **To:** soroban · **Status:** ready after 011 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.13 pp.13-12 to 13-19, ch.14 p.14-23; Appendix B)

INPUT variable (the program stops and prompts with the variable's current value in X; R/S stores X
in the variable and resumes; the guide's cancel and repeat behaviour, p.13-14); VIEW variable (stops
and shows the variable without copying it to X; ENTER copies it; R/S resumes, p.13-15); VIEW or a
displayed line followed by PSE (shown, execution continues, p.13-18); single-stepping a program
(SST/BST, and the guide's rule that VIEW + PSE step as one operation, p.13-18); XEQ (I), GTO (I)
and the (J) forms (p.14-23); the user calculating while a program waits at INPUT, then resuming
(p.13-14). Messages shown through equations (p.13-16, flag 10) wait for the equations unit.

## Rules (manual first; ours where noted)

1. **Ours, the stop is a state the core reports, not a display.** When a run stops it says why:
   at a STOP, at INPUT (with the variable), at VIEW (with the variable), at a PSE (with what is
   shown, and that execution continues), finished, an error (with the line), or the step budget.
   The device unit draws the prompt; the core never waits. A vector token resumes (R/S) after the
   test types its reply.
2. Stack lift and LAST X around INPUT, VIEW and R/S exactly as the guide and Appendix B say; read
   what a value typed at the prompt does to the stack and vector it.
3. The index of XEQ (I)/GTO (I) follows the guide's numbering for labels (find it at p.14-23; say
   if it is ambiguous), and an index that names no label is the guide's error.
4. Errors leave everything unchanged, as always, and stop the program at the failing line.

## Acceptance and report

As before: the guide's examples as M-vectors (p.13-13/13-14 the area of a circle with INPUT,
p.13-16 to 13-18 the cylinder with INPUT and VIEW, without the flag-10 message line until equations
exist; p.14-23's indirect example), red then green. At review: hidden random programs that stop at
INPUT and VIEW and are resumed with typed values, against the reviewer's interpreter.
