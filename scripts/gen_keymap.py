#!/usr/bin/env python3
"""gen_keymap: the layout (layout/stu32-v0.json) to firmware/keymap.c (unit 018).

    python3 scripts/gen_keymap.py            # rewrite keymap.c
    python3 scripts/gen_keymap.py --check    # exit 1 if keymap.c is not what the layout gives

Every legend must have a meaning below, or the generator stops: a new legend in the layout fails
here before it can reach a key. A legend the layout marks as a menu (gold_menu, blue_menu) is a
menu whatever its text. Meanings come from the core's ops (core/calc.h) and the guide's names for
them (UG Appendix G); where the layout's legend is not the guide's name it is noted. Standard
library only.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LAYOUT = os.path.join(HERE, "..", "layout", "stu32-v0.json")
OUT = os.path.join(HERE, "..", "firmware", "keymap.c")

# legend -> (kind, op, arg, prompt)
OP = lambda op, arg=0: ("KM_OP", op, arg, 0)
VAR = lambda op: ("KM_PROMPT", op, 0, "KM_ARG_VAR")
MEANING = {
    # digits and entry (UG 1-15, 1-19)
    **{str(d): OP(f"AB_K{d}") for d in range(10)},
    ".": OP("AB_KPOINT"), "E": OP("AB_KE"), "+/−": OP("AB_KCHS"), "←": OP("AB_KBKSP"),
    "ENTER": OP("AB_ENTER"), "C": OP("AB_CANCEL"),
    ",": OP("AB_KCOMMA"),                       # G-2: separates a function's arguments; a vector's too
    # the stack (ch.2)
    "R↓": OP("AB_RDN"), "R↑": OP("AB_RUP"), "x↔y": OP("AB_XY"), "LASTx": OP("AB_LASTX"),
    # arithmetic and functions (ch.1, 4)
    "+": OP("AB_ADD"), "−": OP("AB_SUB"), "×": OP("AB_MUL"), "÷": OP("AB_DIV"),
    "√x": OP("AB_SQRT"), "x²": OP("AB_SQ"), "ABS": OP("AB_ABS"),
    "eˣ": OP("AB_EXP"), "10ˣ": OP("AB_TENX"), "LN": OP("AB_LN"), "LOG": OP("AB_LOG"),
    "yˣ": OP("AB_POW"), "ˣ√y": OP("AB_XROOT"), "1/x": OP("AB_INV"),
    "%": OP("AB_PCT"), "%CHG": OP("AB_PCTCHG"), "π": OP("AB_PI"),
    "SIN": OP("AB_SIN"), "COS": OP("AB_COS"), "TAN": OP("AB_TAN"),
    "ASIN": OP("AB_ASIN"), "ACOS": OP("AB_ACOS"), "ATAN": OP("AB_ATAN"),
    "→RAD": OP("AB_TORAD"), "→DEG": OP("AB_TODEG"),
    "INT÷": OP("AB_IDIV"), "Rmdr": OP("AB_RMDR"),   # G-12 names it RMDR; the layout prints Rmdr (4-2's menu)
    "n!": OP("AB_FACT"),                        # G-15 names it !; the layout prints n!
    "RAND": OP("AB_RAND"), "SEED": OP("AB_SEED"),        # G-11 RANDOM; the layout prints RAND
    "Σ+": OP("AB_SIGMAPLUS"), "Σ−": OP("AB_SIGMAMINUS"),
    # storage and programs (ch.3, 13, 14): the argument is typed next
    "STO": VAR("AB_STO"), "RCL": VAR("AB_RCL"), "VIEW": VAR("AB_VIEW"),
    "ISG": VAR("AB_ISG"), "DSE": VAR("AB_DSE"),
    "LBL": ("KM_PROMPT", "AB_LBL", 0, "KM_ARG_LABEL"),
    "XEQ": ("KM_PROMPT", "AB_XEQ", 0, "KM_ARG_TARGET"), "GTO": ("KM_PROMPT", "AB_GTO", 0, "KM_ARG_TARGET"),
    "RTN": OP("AB_RTN"), "R/S": OP("AB_RS"), "PSE": OP("AB_PSE"), "SST": OP("AB_SST"),
    # the device's own
    "OFF": ("KM_DEVICE", "0", "KM_DEV_OFF", 0),
    "UNDO": ("KM_DEVICE", "0", "KM_DEV_UNDO", 0),
    "→FRAC": OP("AB_FDISP"),                     # the Fraction-display toggle, ch.5 (abacus #3822)
}

def o(legend, op, arg=0):
    return (legend, "KM_OP", op, arg, 0)


def p(legend, op, prompt):
    return (legend, "KM_PROMPT", op, 0, prompt)


def t(legend, rel):
    return (legend, "KM_OP", "AB_TEST", rel, 0)


NONE = ("", "KM_NONE", "0", 0, 0)
GPAGE = ("\u25b8", "KM_DEVICE", "0", "KM_DEV_GPAGE", 0)    # unit 040: the graph's next page

# The menus (abacus #3877; provisional until Michael rules the menus): a menu's name and its six
# soft keys, each (legend, kind, op, arg, prompt). PRGM, DISP, the angle modes and CLEAR are
# defined; a menu key whose menu is not here is a menu not defined yet (arg -1). Data only, like
# LETTERS.
MENUS = [
    ("PRGM", [("PRGM", "KM_OP", "AB_PRGM", 0, 0), ("LBL", "KM_PROMPT", "AB_LBL", 0, "KM_ARG_LABEL"),
              ("INPUT", "KM_PROMPT", "AB_INPUT", 0, "KM_ARG_VAR"), ("SST", "KM_OP", "AB_SST", 0, 0),
              ("BST", "KM_OP", "AB_BST", 0, 0), ("CLPGM", "KM_OP", "AB_CLPGM", 0, 0)]),
    # Michael, #3901: FIX, SCI, ENG then a digit setting (UG p.1-21), ALL; xiy and rθa provisional
    ("DISP", [("FIX", "KM_PROMPT", "AB_SETFIX", 0, "KM_ARG_DIGITS"),
              ("SCI", "KM_PROMPT", "AB_SETSCI", 0, "KM_ARG_DIGITS"),
              ("ENG", "KM_PROMPT", "AB_SETENG", 0, "KM_ARG_DIGITS"), ("ALL", "KM_OP", "AB_SETALL", 0, 0),
              ("xiy", "KM_OP", "AB_XIY", 0, 0), ("r\u03b8a", "KM_OP", "AB_RTA", 0, 0)]),
    # abacus decision 33: the angle modes (UG G-5, G-11, G-8); soft keys 4-6 empty
    ("\u2221MODE", [("DEG", "KM_OP", "AB_DEG", 0, 0), ("RAD", "KM_OP", "AB_RAD", 0, 0),
                    ("GRAD", "KM_OP", "AB_GRAD", 0, 0), ("", "KM_NONE", "0", 0, 0),
                    ("", "KM_NONE", "0", 0, 0), ("", "KM_NONE", "0", 0, 0)]),
    # unit 019's draft order (CLSTK second: the forum's complaint), with unit 021's two items: ALL
    # asks first (UG p.1-5, CLR ALL? Y N), CLVARx takes a three-digit address (UG p.1-5)
    # abacus #3958: the 35s's option names, the title supplying CL (UG p.1-5), STK kept second
    ("CLEAR", [("x", "KM_OP", "AB_CLX", 0, 0), ("STK", "KM_OP", "AB_CLSTK", 0, 0),
               ("VARS", "KM_OP", "AB_CLVARS", 0, 0), ("\u03a3", "KM_OP", "AB_CLSIGMA", 0, 0),
               ("ALL", "KM_CONFIRM", "AB_CLALL", 0, 0), ("VARx", "KM_PROMPT", "AB_CLVARX", 0, "KM_ARG_ADDR")]),
    # unit 019 (abacus's draft v1, provisional): built exactly as drafted, F1-F6 in order
    ("HYP", [o("SINH", "AB_SINH"), o("COSH", "AB_COSH"), o("TANH", "AB_TANH"), o("ASINH", "AB_ASINH"),
             o("ACOSH", "AB_ACOSH"), o("ATANH", "AB_ATANH")]),
    ("STAT", [o("x\u0304", "AB_XBAR"), o("y\u0304", "AB_YBAR"), o("sx", "AB_SX"), o("sy", "AB_SY"),
              o("\u03c3x", "AB_PSX"), o("\u03c3y", "AB_PSY")]),
    ("L.R.", [o("x\u0302", "AB_XHAT"), o("y\u0302", "AB_YHAT"), o("r", "AB_CORR"), o("m", "AB_SLOPE"),
              o("b", "AB_YINT"), o("x\u0304w", "AB_XWBAR")]),
    ("SUMS", [o("n", "AB_NSUM"), o("\u03a3x", "AB_SUMX"), o("\u03a3y", "AB_SUMY"), o("\u03a3x\u00b2", "AB_SUMX2"),
              o("\u03a3y\u00b2", "AB_SUMY2"), o("\u03a3xy", "AB_SUMXY")]),
    ("PROB", [o("Cn,r", "AB_NCR"), o("Pn,r", "AB_NPR"), o("n!", "AB_FACT"), o("RAND", "AB_RAND"),
              o("SEED", "AB_SEED"), NONE]),
    # SF, CF, FS? take the flag from the digit prompt: 10 and 11 by .0 and .1 (UG p.14-9)
    ("FLAGS", [p("SF", "AB_SF", "KM_ARG_DIGITS"), p("CF", "AB_CF", "KM_ARG_DIGITS"),
               p("FS?", "AB_FS", "KM_ARG_DIGITS"), o("BEEP", "AB_BEEP"), NONE, NONE]),   # unit 027: BEEP, ours
    # the tests: AB_TEST with its relation (calc.h's AB_TEST_* order, + 6 against 0; UG p.14-7)
    ("x?y", [t("x\u2260y?", 0), t("x\u2264y?", 1), t("x<y?", 2), t("x>y?", 3), t("x\u2265y?", 4), t("x=y?", 5)]),
    ("x?0", [t("x\u22600?", 6), t("x\u22640?", 7), t("x<0?", 8), t("x>0?", 9), t("x\u22650?", 10), t("x=0?", 11)]),
    ("TEST", [t("x=y?", 5), t("x\u2260y?", 0), t("x=0?", 11), t("x\u22600?", 6), p("FS?", "AB_FS", "KM_ARG_DIGITS"),
              t("x<y?", 2)]),
    ("BASE", [o("DEC", "AB_DEC"), o("HEX", "AB_HEX"), o("OCT", "AB_OCT"), o("BIN", "AB_BIN"),
              ("LOGIC", "KM_MENU", "0", "LOGIC", 0), ("SUFX", "KM_MENU", "0", "SUFFIX", 0)]),   # boxed as submenus
    ("LOGIC", [o("AND", "AB_AND"), o("OR", "AB_OR"), o("XOR", "AB_XOR"), o("NOT", "AB_NOT"), o("NAND", "AB_NAND"),
               o("NOR", "AB_NOR")]),
    ("SUFFIX", [o("d", "AB_SUF", 10), o("h", "AB_SUF", 16), o("o", "AB_SUF", 8), o("b", "AB_SUF", 2), NONE, NONE]),
    ("CMPLX", [o("i", "AB_KI"), o("\u03b8", "AB_KTHETA"), o("ARG", "AB_ARG"), o("[", "AB_KVEC"), NONE, NONE]),
    # the three formats are one core op each, both flags at once (rule 2; UG p.5-6)
    ("FRAC", [o("FDISP", "AB_FDISP"), o("/c", "AB_SETC"), o("RCL/c", "AB_RCLC"), o("PREC", "AB_FRACFMT", 0),
              o("FCTR", "AB_FRACFMT", 1), o("FIXED", "AB_FRACFMT", 2)]),
    # abacus #3958: EXP as drafted, LN(1+x) shortened to fit five cells (ours)
    ("EXP", [o("2\u02e3", "AB_TWOX"), o("LOG\u2082", "AB_LOG2"), o("e\u02e3\u22121", "AB_EXPM1"), o("LN1+x", "AB_LN1P"),
             NONE, NONE]),
    ("CONV", [o("\u2192kg", "AB_TOKG"), o("\u2192lb", "AB_TOLB"), o("\u2192\u00b0C", "AB_TOC"),
              o("\u2192\u00b0F", "AB_TOF"), o("\u2192cm", "AB_TOCM"), o("\u2192in", "AB_TOIN")]),
    ("UNITS", [o("\u2192l", "AB_TOL"), o("\u2192gal", "AB_TOGAL"), o("\u2192km", "AB_TOKM"),
               o("\u2192mile", "AB_TOMILE"), NONE, NONE]),
    ("ANGLE", [o("\u2192POL", "AB_TOPOL"), o("\u2192REC", "AB_TOREC"), o("\u2192HMS", "AB_HMS"),
               o("HMS\u2192", "AB_HMSINV"), o("\u2192DEG", "AB_TODEG"), o("\u2192RAD", "AB_TORAD")]),
    ("VARS", [p("x\u2194", "AB_XVAR", "KM_ARG_VAR"), p("VIEW", "AB_VIEW", "KM_ARG_VAR"), o("CLR", "AB_CLVARS"),
              NONE, NONE, NONE]),
    # unit 030: SHOW at F5, held (abacus #4292: no shift position is free on the layout)
    ("STACK", [o("CLSTK", "AB_CLSTK"), o("LASTx", "AB_LASTX"), o("R\u2191", "AB_RUP"), o("x\u2194y", "AB_XY"),
               o("SHOW", "AB_SHOW"), NONE]),
    # unit 027: how the calculator behaves; unit 029: STU (a 35s) at F3, its entry RPN or ALG at F4 and
    # F5 (ALG shown only in STU: the app hides it); the model and the entry in effect marked
    ("MODE", [o("33s", "AB_MODEL", 33), o("35s", "AB_MODEL", 35), o("STU", "AB_MODEL", "AB_MODEL_STU"),
              o("RPN", "AB_ENTRYM", 0), o("ALG", "AB_ENTRYM", 1), NONE]),
    # the parts of a number under the printed legend POW (Michael is asked to rename it PARTS)
    ("POW", [o("IP", "AB_IP"), o("FP", "AB_FP"), o("INTG", "AB_INTG"), o("RND", "AB_RND"), o("SGN", "AB_SGN"),
             o("ABS", "AB_ABS")]),
    # unit 020 (provisional): in Equation mode the soft keys are the editor's bar, chosen by the app
    # from the core's state: the list's (NEW, ours: EQN LIST TOP) and typing's; REG > is one choice
    # of a register (UG p.B-7), then typing's bar again. On no key, like LOGIC and SUFFIX.
    ("EQN LIST", [o("\u25b2", "AB_BST"), o("\u25bc", "AB_SST"), o("EDIT", "AB_KRIGHT"), o("NEW", "AB_EQTOP"),
                  ("CAS", "KM_MENU", "0", "CAS", 0), o("EXIT", "AB_EQN")]),    # CAS: STU's only (CAS 004)
    # unit 032 (abacus #4350, #4353 answers): TABLE on blue 8: the variable prompted, START and STEP from X,
    # GO; while a table is open the app's bar scrolls it
    ("TABLE", [p("VAR", "AB_TVAR", "KM_ARG_VAR"), o("START", "AB_TSTART"), o("STEP", "AB_TSTEP"), o("GO", "AB_TABLE"),
               NONE, NONE]),
    ("TABLE BAR", [o("\u25b2", "AB_TUP"), o("\u25bc", "AB_TDOWN"), NONE, NONE, NONE, NONE]),
    # unit 033: GRAPH on blue 7: the window from X, AUTO, GO; while a graph is open its bar traces it
    ("GRAPH", [o("XMIN", "AB_XMIN"), o("XMAX", "AB_XMAX"), o("YMIN", "AB_YMIN"), o("YMAX", "AB_YMAX"),
               o("AUTO", "AB_GAUTO"), o("GO", "AB_GRAPH")]),
    # unit 040: the open graph's soft keys, three pages after the 50g's plot menus (guide p.12-4 to 12-7, 12-47):
    # ▸ (F6) moves to the next; a page's GOLD or BLUE bank shows while that shift is armed. The first keeps 033's
    # trace keys where they were; the digits move the cursor (app.c).
    ("GRAPH BAR", [o("\u25c0", "AB_GLEFT"), o("\u25b6", "AB_GRIGHT"), o("TRACE", "AB_GTRACE"),
                   ("(X,Y)", "KM_DEVICE", "0", "KM_DEV_GXY", 0), o("MARK", "AB_GMARK"), GPAGE]),
    ("GRAPH BAR GOLD", [("ZFACT", "KM_DEVICE", "0", "KM_DEV_GFACT", 0), NONE, NONE, NONE, NONE, GPAGE]),
    ("GRAPH ZOOM", [o("ZIN", "AB_GZOOM", "AB_Z_IN"), o("ZOUT", "AB_GZOOM", "AB_Z_OUT"), o("BOXZ", "AB_GZOOM", "AB_Z_BOX"),
                    o("CNTR", "AB_GZOOM", "AB_Z_CNTR"), o("ZAUTO", "AB_GZOOM", "AB_Z_AUTO"), GPAGE]),
    ("GRAPH ZOOM GOLD", [o("HZIN", "AB_GZOOM", "AB_Z_HIN"), o("HZOUT", "AB_GZOOM", "AB_Z_HOUT"),
                         o("VZIN", "AB_GZOOM", "AB_Z_VIN"), o("VZOUT", "AB_GZOOM", "AB_Z_VOUT"),
                         o("ZLAST", "AB_GZOOM", "AB_Z_LAST"), GPAGE]),
    ("GRAPH ZOOM BLUE", [o("ZDFLT", "AB_GZOOM", "AB_Z_DFLT"), o("ZDECI", "AB_GZOOM", "AB_Z_DECI"),
                         o("ZINTG", "AB_GZOOM", "AB_Z_INTG"), o("ZSQR", "AB_GZOOM", "AB_Z_SQR"),
                         o("ZTRIG", "AB_GZOOM", "AB_Z_TRIG"), GPAGE]),
    ("GRAPH FCN", [o("ROOT", "AB_GFCN", "AB_F_ROOT"), o("SLOPE", "AB_GFCN", "AB_F_SLOPE"), o("EXTR", "AB_GFCN", "AB_F_EXTR"),
                   o("AREA", "AB_GFCN", "AB_F_AREA"), o("F(X)", "AB_GFCN", "AB_F_FX"), GPAGE]),
    ("GRAPH FCN GOLD", [o("TANL", "AB_GFCN", "AB_F_TANL"), o("ISECT", "AB_GFCN", "AB_F_ROOT"), NONE, NONE, NONE, GPAGE]),
    # unit 035 (review): a linear solution's X= Y= (Z=): up and down step among them (35s p.7-7)
    ("LIN VIEW", [o("\u25b2", "AB_BST"), o("\u25bc", "AB_SST"), NONE, NONE, NONE, NONE]),
    # unit 029 proposal 2: STU's expression line, chosen by the app as Equation mode's bars are: the
    # cursor, parentheses, the history up and down (the key for = is ENTER's)
    ("ALG LINE", [o("\u25c0", "AB_KLEFT"), o("\u25b6", "AB_KRIGHT"), o("( )", "AB_KPAREN"), NONE,
                  o("\u25b2", "AB_HUP"), o("\u25bc", "AB_HDOWN")]),
    ("EQN TYPE", [o("\u25c0", "AB_KLEFT"), o("\u25b6", "AB_KRIGHT"), o("( )", "AB_KPAREN"), o("=", "AB_KEQ"),
                  o("SPC", "AB_KSPACE"), ("REG", "KM_MENU", "0", "REG", 0)]),
    ("REG", [o("REGX", "AB_KREG", 0), o("REGY", "AB_KREG", 1), o("REGZ", "AB_KREG", 2), o("REGT", "AB_KREG", 3),
             NONE, NONE]),
    # unit 022 (provisional homes): the device's settings (settings.h). RADIX: the mark and the
    # grouping (UG p.1-24; 1 000 ours); SETUP: OFF cycles 2/5/10/30 minutes, VOL the five levels,
    # BEEP on or off (the app shows each value). A KM_DEVICE action's op is its value.
    ("RADIX", [("."   , "KM_DEVICE", "'.'", "KM_DEV_RADIX", 0), (",", "KM_DEVICE", "','", "KM_DEV_RADIX", 0),
               ("1,000", "KM_DEVICE", "1", "KM_DEV_GROUP", 0), ("1 000", "KM_DEVICE", "' '", "KM_DEV_GROUP", 0),
               ("none", "KM_DEVICE", "0", "KM_DEV_GROUP", 0), NONE]),
    ("SETUP", [("OFF", "KM_DEVICE", "0", "KM_DEV_OFFTIME", 0), ("VOL", "KM_DEVICE", "0", "KM_DEV_VOL", 0),
               ("BEEP", "KM_MENU", "0", "BEEP", 0), ("CLOCK", "KM_MENU", "0", "CLOCK", 0), NONE, NONE]),
    # unit 027 rule 3: the buzzer's uses, each toggled (the op its DEV_USE_* bit), two pages; ALL turns
    # every use off if any is on, else all on. On no key: SETUP's BEEP opens it.
    ("BEEP", [("CLICK", "KM_DEVICE", "1", "KM_DEV_USE", 0), ("ERROR", "KM_DEVICE", "2", "KM_DEV_USE", 0),
              ("PRGM", "KM_DEVICE", "4", "KM_DEV_USE", 0), ("BATT", "KM_DEVICE", "8", "KM_DEV_USE", 0),
              ("ON", "KM_DEVICE", "16", "KM_DEV_USE", 0), ("\u25b8", "KM_MENU", "0", "BEEP2", 0)]),
    ("BEEP2", [("DONE", "KM_DEVICE", "32", "KM_DEV_USE", 0), ("ALL", "KM_DEVICE", "0", "KM_DEV_USEALL", 0),
               ("APP", "KM_DEVICE", "64", "KM_DEV_USE", 0), NONE, NONE,     # unit 037: an app's tones
               ("\u25b8", "KM_MENU", "0", "BEEP", 0)]),
    # unit 027 rule 4 (SETUP's F4 its provisional home): SET prompts HH MM; 12/24 toggles the form
    ("CLOCK", [("SET", "KM_DEVICE", "0", "KM_DEV_CLOCKSET", 0), ("12/24", "KM_DEVICE", "0", "KM_DEV_H24", 0),
               NONE, NONE, NONE, NONE]),
    # unit 027 rule 5 (as amended: FLASH, Michael's SYS card): views any key leaves (the op APP_VIEW_*)
    ("SYS", [("BATT", "KM_DEVICE", "1", "KM_DEV_VIEW", 0), ("VER", "KM_DEVICE", "2", "KM_DEV_VIEW", 0),
             ("FLASH", "KM_DEVICE", "3", "KM_DEV_VIEW", 0), NONE, NONE, NONE]),
    # rule 3: the one paged menu; the app fills soft keys 1-5 from the core's constants
    ("CONST", [NONE, NONE, NONE, NONE, NONE, ("more", "KM_NONE", "0", 0, 0)], "AB_CONST"),
    # unit 037: APPS on blue 0, paged as CONST: the app fills soft keys 1-5 with the registered apps
    ("APPS", [NONE, NONE, NONE, NONE, NONE, ("more", "KM_NONE", "0", 0, 0)], "KM_PAGED_APPS"),
    # CAS 004 (casim's work/004 rule 4, proposed): Casimir on the equation shown, from EQN LIST's fifth key, in STU
    # only (the app shows no CAS in 33s or 35s mode); D/DX, COLCT and SUBST prompt for the variable as SOLVE does
    ("CAS", [p("D/DX", "AB_CASD", "KM_ARG_VAR"), o("SIMP", "AB_CASSIMP"), o("EXPND", "AB_CASEXP"),
             p("COLCT", "AB_CASCOL", "KM_ARG_VAR"), p("SUBST", "AB_CASSUB", "KM_ARG_VAR"), NONE]),
]
# Menu legends that act as plain ops until their menu is ruled (abacus #3867): the EQN menu's
# first item will take this over.
MENU_AS_OP = {"EQN": OP("AB_EQN"),
              # unit 019 rule 4: SOLVE and integrate are direct, the 35s's way: the variable, then
              # the op. The layout still boxes them as menus; Michael's roadmap answer decides the
              # legend.
              "SOLVE": ("KM_PROMPT", "AB_SOLVE", 0, "KM_ARG_VAR"),
              "\u222b": ("KM_PROMPT", "AB_INTEG", 0, "KM_ARG_VAR")}

# The letter layer. The letters are the layout's own (each lettered key's "letter" field in
# stu32-v0.json, printed on the keycaps, decision 25, abacus #3887); LETTERS is the check: the
# generator stops if the two disagree, so the caps, the keymap and the app read one assignment.
# The roles are not printed, so they live here. Keys with neither type nothing.
LETTERS = {
    6: "A", 7: "B", 8: "C", 9: "D", 10: "E", 11: "F", 12: "G", 13: "H", 14: "I", 15: "J", 16: "K", 17: "L",
    18: "M", 19: "N", 20: "O", 21: "P", 22: "Q", 24: "R", 25: "S", 26: "T", 27: "U", 31: "V", 32: "W",
    33: "X", 37: "Y", 38: "Z",
}
ROLES = {
    39: "(I)", 43: "(J)", 42: "ESC",
    46: "+", 40: "-", 34: "*", 28: "/",         # STO + ... as on the 35s (UG p.3-6, 3-7)
}


def layout_letters():
    layout = json.load(open(LAYOUT, encoding="utf-8"))
    out = {}
    for r, row in enumerate(layout["rows"]):
        for col, key in enumerate(row["keys"]):
            if "letter" in key:
                out[r * 6 + col] = key["letter"]
    if out != LETTERS:
        diff = sorted(set(out.items()) ^ set(LETTERS.items()))
        sys.exit(f"gen_keymap: the layout's letters and LETTERS disagree: {diff}")
    return {**out, **ROLES}
ARITH = {"+": "AB_ADD", "-": "AB_SUB", "*": "AB_MUL", "/": "AB_DIV"}


def letter_cell(what):
    if len(what) == 1 and "A" <= what <= "Z":
        return f"{{KL_LETTER, {ord(what) - ord('A')}}}"
    if what in ARITH:
        return f"{{KL_ARITH, {ARITH[what]}}}"
    return {"(I)": "{KL_IND_I, 0}", "(J)": "{KL_IND_J, 0}", "ESC": "{KL_ESC, 0}"}[what]


def check_letters(acts, layer):
    letters = [w for w in layer.values() if len(w) == 1 and "A" <= w <= "Z"]
    if sorted(letters) != [chr(ord("A") + i) for i in range(26)]:
        sys.exit("gen_keymap: the letter layer must give A..Z once each")
    for k in layer:
        if k not in acts or acts[k][0][0] in ("KM_NONE", "KM_SOFT"):
            sys.exit(f"gen_keymap: the letter layer names key {k}, which is not a lettered key")


def cstr(s):
    """a C string literal; after a hex escape, a hex digit starts a new literal ("\\x92" "DEG")"""
    out, esc = "", False
    for c in s:
        if 32 <= ord(c) < 127 and c not in '"\\':
            if esc and c in "0123456789abcdefABCDEF":
                out += '" "'
            out += c
            esc = False
        else:
            out += "".join(f"\\x{b:02x}" for b in c.encode())
            esc = True
    return '"' + out + '"'


def actions():
    layout = json.load(open(LAYOUT, encoding="utf-8"))
    out = {}
    for r, row in enumerate(layout["rows"]):
        for col, key in enumerate(row["keys"]):
            k = r * 6 + col
            acts = []
            for shift in ("face", "gold", "blue"):
                legend = key.get(shift, "")
                if shift == "face" and row["name"] == "soft":
                    acts.append(("KM_SOFT", "0", str(col), "0", ""))
                elif shift == "face" and not legend and key.get("cap") in ("gold", "blue"):
                    acts.append(("KM_SHIFT", "0", "1" if key["cap"] == "gold" else "2", "0", ""))
                elif not legend:
                    acts.append(("KM_NONE", "0", "0", "0", ""))
                elif legend in MENU_AS_OP:
                    kind, op, arg, prompt = MENU_AS_OP[legend]
                    acts.append((kind, op, str(arg), str(prompt), legend))
                elif key.get(shift + "_menu"):
                    index = [m[0] for m in MENUS].index(legend) if legend in [m[0] for m in MENUS] else -1
                    acts.append(("KM_MENU", "0", str(index), "0", legend))
                elif legend in MEANING:
                    kind, op, arg, prompt = MEANING[legend]
                    acts.append((kind, op, str(arg), str(prompt), legend))
                else:
                    sys.exit(f"gen_keymap: row {r} key {col}: no meaning for the {shift} legend {legend!r}")
            out[k] = acts
    return out


def menu_arg(kind, arg):
    """a soft key that opens a menu names it (BASE's LOGIC and SUFFIX): its index"""
    if kind == "KM_MENU":
        return str([m[0] for m in MENUS].index(arg))
    return str(arg)


def emit():
    lines = ["/* keymap: generated by scripts/gen_keymap.py from layout/stu32-v0.json (the layout: edit the",
             " * layout or the generator's table, not this file). See keymap.h. */",
             '#include "keymap.h"', "", '#include "calc.h"', "",
             "static const km_action KEYMAP[KB_KEYS][3] = {"]
    acts_by_key = actions()
    none = [("KM_NONE", "0", "0", "0", "")] * 3
    for k in range(48):                         # every slot, a key or not: no NULL legend
        acts = acts_by_key.get(k, none)
        cells = ", ".join(f"{{{kind}, {op}, {arg}, {prompt}, {cstr(legend)}}}" for kind, op, arg, prompt, legend in acts)
        lines.append(f"    [{k}] = {{{cells}}},")
    layer = layout_letters()
    check_letters(acts_by_key, layer)
    lines += ["};", "", "static const km_menu MENUS[] = {"]
    for name, items, *paged in MENUS:
        if len(items) != 6:
            sys.exit(f"gen_keymap: menu {name} needs six soft keys")
        cells = ", ".join(f"{{{kind}, {op}, {menu_arg(kind, arg)}, {prompt}, {cstr(legend)}}}"
                          for legend, kind, op, arg, prompt in items)
        lines.append(f"    {{{cstr(name)}, {{{cells}}}, {paged[0] if paged else 0}}},")
    lines += ["};", "",
              "const km_menu *km_menu_at(int index)",
              "{",
              "    return index >= 0 && index < (int)(sizeof MENUS / sizeof MENUS[0]) ? &MENUS[index] : (const km_menu *)0;",
              "}", "", "static const km_letter LETTER[KB_KEYS] = {"]
    for k in sorted(layer):
        lines.append(f"    [{k}] = {letter_cell(layer[k])},")
    lines += ["};", "",
              "const km_letter *km_letter_of(int key)",
              "{",
              "    static const km_letter none = {KL_NONE, 0};",
              "    return key < 0 || key >= KB_KEYS ? &none : &LETTER[key];",
              "}", "",
              "int km_arith_op(int op, int arith)",
              "{",
              "    int i = arith == AB_ADD ? 0 : arith == AB_SUB ? 1 : arith == AB_MUL ? 2 : arith == AB_DIV ? 3 : -1;",
              "    if (i < 0) return -1;",
              "    if (op == AB_STO) return AB_STOADD + i;     /* STOADD..STODIV and RCLADD..RCLDIV are in order */",
              "    if (op == AB_RCL) return AB_RCLADD + i;",
              "    return -1;",
              "}", "",
              "int km_prompt_arg(int prompt, const km_letter *l)",
              "{",
              "    switch (prompt) {",
              "    case KM_ARG_VAR:",
              "        return l->kind == KL_LETTER ? AB_VAR_A + l->value : l->kind == KL_IND_I ? AB_VAR_IND_I",
              "               : l->kind == KL_IND_J ? AB_VAR_IND_J : -1;",
              "    case KM_ARG_LABEL:",
              "        return l->kind == KL_LETTER ? l->value : -1;",
              "    case KM_ARG_TARGET:                         /* a label's first line (UG p.13-10), or (I) / (J) */",
              "        return l->kind == KL_LETTER ? l->value * 1000 : l->kind == KL_IND_I ? AB_TGT_IND_I",
              "               : l->kind == KL_IND_J ? AB_TGT_IND_J : -1;",
              "    default:",
              "        return -1;",
              "    }",
              "}", "",
              "const km_action *km_lookup(int key, int shift)",
              "{",
              "    static const km_action none = {KM_NONE, 0, 0, 0, \"\"};",
              "    if (key < 0 || key >= KB_KEYS || shift < 0 || shift > 2) return &none;",
              "    return &KEYMAP[key][shift];",
              "}", ""]
    return "\n".join(lines)


if __name__ == "__main__":
    text = emit()
    if "--check" in sys.argv:
        sys.exit(0 if open(OUT, encoding="utf-8").read() == text else 1)
    open(OUT, "w", encoding="utf-8").write(text)
