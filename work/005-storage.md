# Work unit 005: storage registers and storage arithmetic

**From:** abacus · **To:** soroban · **Status:** ready after 004 (light brief; you extract examples)

## Scope (HP 35s User's Guide chapter 3, pp.3-1 to 3-10; Appendix B for stack lift and LAST X)

STO and RCL to the 26 lettered variables A–Z; VIEW; storage arithmetic (STO+ STO− STO× STO÷);
recall arithmetic (RCL+ RCL− RCL× RCL÷); exchanging X with a variable (x↔var); the variables I and
J and indirect addressing through them (p.3-9); clearing all variables (CLVARS). Key tokens:
`STO:A`, `RCL:A`, `STO+:A` .. `RCL÷:A` (ASCII `STO/:A`, `RCL/:A` etc. are fine), `XVAR:A`,
`VIEW:A`, `CLVARS`, and the indirect forms `STO:(I)`, `RCL:(J)` etc.

## Rules (manual first; ours where noted)

1. Stack lift and LAST X exactly as Appendix B says for each (B-5 lists which operations are
   neutral; B-6 says recall arithmetic saves x in LAST X, and how the recall sequence differs).
2. RCL lifts the stack like any recalled number, and a typed number completed by RCL survives
   (unit 001 ruling 1).
3. Storage arithmetic with an error (÷0, range) leaves the variable, the stack and LAST X unchanged
   and reports an error.
4. Indirect addressing: follow the manual's numbering of indirect registers (p.3-9 and wherever it
   defines the range); an index out of range is an error. **Ours:** the indirect register count may
   be larger than the 35s's (we have far more memory); start with the 35s's range so its programs'
   numbering holds, and say if the manual's range is ambiguous.
5. Variables persist across power cycles later (a device unit); here they live in the core state
   and `ab_init` clears them.

## Acceptance and report

As before. At review: hidden random sequences of stores, recalls, arithmetic and exchanges against
the reviewer's model.
