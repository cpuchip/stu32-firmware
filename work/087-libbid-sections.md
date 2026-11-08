# Blueprint: the decimal library built in sections (the S3's unused tables and code left out)

**From:** soroban · **To:** abacus · **Asked:** abacus #13646 (Michael: "Go for all 4") · **Unit 087.** **Base:** 86b3fbf.

**Why.** On the S3, libbid is built without `-ffunction-sections -fdata-sections`. So a whole object is linked
whenever one function in it is called. Measured in a scratch copy of 5b9450a (#13630):
- bid_binarydecimal.o brings in 1,175,494 B of conversion tables that nothing calls;
- libbid's code is 182,366 B once the uncalled functions are dropped, against 363,918;
- Rev A's image is 2,241,792 B, against 3,603,712.

**The change** (one line, plus VER):
- `scripts/fetch-intel-dfp.sh`: the esp32s3 build's `CFLAGS_OPT` gains `-ffunction-sections -fdata-sections`.
  - The script's flag stamp already forces a clean library build when the flags change.
  - Every S3 path goes through it: device/reva/build.sh, headless.sh, device/standin/run.sh, prof.sh and
    resources.py.
  - ESP-IDF already links with `--gc-sections`.
- `firmware/version.h`: "0.87".
- **Not changed:**
  - The host library (`TARGET=""`) keeps its flags, so the tests run the same library code.
  - The esp32c6 recipe: no device builds it. The same line would apply if one ever does.
  - The core, Casimir and the games.

**The oracle: nothing a student sees changes.**
1. **`make test`:** byte for byte against 86b3fbf's output, with the host library unchanged. This is a check that
   nothing else moved, not evidence for the change: the host tests never link the S3 library.
2. **D68,** `scripts/resources.py static 86b3fbf NEW` (stand-in and Rev A):
   - flash falls about 1.36 MB;
   - DIRAM .data falls by libbid's dropped writable tables;
   - IRAM code and .bss stay +0;
   - every other library stays +0.
3. **On the board** (the stand-in, 1a86 56E1012198):
   - **Answers:** smoke.txt and accept.txt give ALL MET with the same answers at the base and the new build, and
     each script's own control row is NOT MET as it was written to be.
   - **Board run,** `resources.py board 86b3fbf NEW`: heap after boot, the stack through the keys, and the speed
     benches.
   - **Flash time:** esptool's own "Wrote … in N s" for the app, at the base and the new build.
   - **Rev A boot:** Rev A boots once, as its head-less image on the stand-in board, as D68 does. The two
     Espressif USB-JTAG ports on lepton aren't mine, and I won't touch them unless you name one as a Rev A board.
4. **`make clang-check`.**
5. **The flags took effect:**
   - **Positive control:** the new map has per-function sections from libbid (`.text.__bid128_add`).
   - **Negative control:** the base map has none.
   - Without this check, a stale libbid.a would pass every check above while measuring the old library.

**Predictions** (from the scratch build; written before the run):
- **Rev A image:** 2,241,792 B ± 16 (VER and the commit string are the same lengths).
- **libbid on Rev A:**
  - code 363,918 -> 182,366;
  - rodata 2,349,568 -> 1,174,074;
  - .data 37,176 -> 32,512 (−4,664).
- **The stand-in:** about the same cut. Not measured in scratch.
- **Heap after boot:** +4,664 ± 16 on both targets, since the dropped .data returns to the heap.
- **Stack:** +0 (the same code and frames).
- **Speed:** the code is laid out differently in flash, so cache behaviour may shift. I predict every bench stays
  within ±5% and every answer is identical. A bench outside ±5% gets reported, not explained away.
- **Flash time:** shorter. I won't guess how much; esptool compresses before writing, and the cut tables may
  compress well.
- **Host `make test`:** identical output.

**Risks.**
- **Code that moves:** the flags change where code lands, not what it does.
- **A table dropped that is still needed:** only an unreferenced section can be dropped, and anything reached
  through a reference is kept. The device answer scripts are the end-to-end check.
- **A stale library:** check 5 catches it.
