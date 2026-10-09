# Work unit 055: WebAssembly apps through WAMR (STU_WASM)

**From:** arcade (the blueprint, stu32-arcade docs/wasm-integration.md, accepted in shape) · **Ruled by:** Michael's
decision 67 ("WAMR, and send the fix upstream too"; abacus a6a3c73, #5322) and decision 68 ("as space conscious as
possible") · **Gate:** soroban · **Status:** built, measured, the S3 gate passed (soroban #5501).

## What it is

An app can be a WebAssembly module: compiled once from C to a .wasm with stu32-arcade's guest shim
(sdk/wasm/guest.c), turned into an .aot for the ESP32-S3 by our pinned wamrc, and run by WAMR's AOT loader through the
bridge (firmware/wasm/wasm_app.c). The framework sees an ordinary app_def. Off by default; off, nothing of it is
built.

The series:
1. third_party/wamr and scripts/fetch-wamr.sh: WAMR main ea5dd88 and five patches by sha256, and the tree they must
   make (13f26968). The patches and their audits: third_party/wamr/README.md.
2. STU_WASM: the device builds, device/sdkconfig.wasm, the bridge, clang-check's line for it.
3. gen_games kind = wasm.
4. resources.py static --env.
5. The wasm gate (below) and this file.

games.list lists no wasm game. Whether BABLW is listed in test builds is on Michael's card (abacus, roadmap v76).

## Decision 68

**Static, `scripts/resources.py static ab51544 <055> [--env STU_WASM=1]`** (no wasm game listed, as games.list is):

| | stand-in | Rev A |
|---|---|---|
| option off: flash, DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| option on: flash, DIRAM, IRAM code, calc_task's frame | +0 | +0 |

With no game, nothing calls the bridge and the linker drops WAMR. Before patch 0005, the option on cost Rev A
+18,736 B flash, +1,932 B DIRAM and +1,300 B IRAM code with no game: WAMR's component required lwip, which brought
VFS, the UART driver and the VFS console into Rev A's minimal component set.

**Per game, BABLW listed** (WAMR, the bridge and one game linked; ab51544 with the scratch build of this series and
patch 0005, 2026-10-09, by hand, since resources.py builds games.list as committed):

| | stand-in | Rev A |
|---|---|---|
| flash | +192,864 | +192,848 |
| static D/IRAM | +288 | +372 |

Of the flash, the .aot is 143,096 B (embedded as 143,100) and libwamr 34,281; the rest is the bridge and the newlib
and ESP-IDF code they call. IRAM: WAMR has 51 B, booked to DIRAM (052).

**On the board** (soroban's S3 stand-in, 2026-10-09: ab51544 with the scratch build of this series before patch
0005, BABAL, BABLW and TRAP listed; the stand-in already had VFS, so 0005 does not change what it links):

| step | internal heap free | calc task's stack-free |
|---|---|---|
| boot, idle | 62,411 | 29,060 |
| native BABAL, keys held | 62,411 | 29,060 |
| BABLW (Babal as wasm), keys held | 62,027 while it runs | 29,060 |
| TRAP key 0 (100 deep, returns) | 62,027 | 23,300 |
| TRAP key 1: "native stack overflow" | 62,027 | 964 |
| C, then 2 ENTER 3 + gives X=5 | 62,075 | 964 |

- **Internal heap while a wasm game runs: 152 B.** 104 B of it is taken once, by the first
  wasm_runtime_full_init, and never again: 20 load, run and unload cycles of BABLW sit flat after the first. The
  other 48 B is wasm_runtime_load's, which unload returns. Everything else WAMR allocates goes through the bridge's
  allocator to PSRAM; before that, a running game took 13,904 B of internal heap.
- **The calc task:** BABLW never takes the stack below the mark boot and native Babal set. Runaway recursion traps
  with 964 B left; the bridge sets the native stack boundary 2 KB above the stack's start, since WAMR's own lookup
  returns NULL without the FreeRTOS trace facility. soroban's calc-task ruling is confirmed on these figures (#5501).
- **Speed, FRAMEBENCH (the app's draw and the blit, 50 frames, 3 runs):** BABAL 665 frames a second; BABLW 381.0,
  and 381.7 with WAMR's quick AOT entries on (off in this series: their 768 B table is sorted at init, so it would
  sit in internal DRAM).

## The wasm gate

```
STU_WASM=1 GAMES_LIST=games-wasm-gate.list device/standin/run.sh PORT device/standin/wasm.txt
```

games-wasm-gate.list lists BABAL, BABLW (stu32-arcade 3d522d5) and TRAP (stu32-arcade 12b36aa), each checked at its
pin. TRAP's key 0 recurses 100 deep and returns; key 1 never stops, and the bridge's guard must turn it into a trap
the calculator survives. wasm.txt is the script the board figures above came from, written into the repo's
conventions (STANDIN READY, RESTART); its 12 expects passed on the S3 as a scratch script. wasm.txt itself, run
by the command above, has not yet been on a board.

## Open

- WASI on does not build in this configuration (libc-wasi's locking.h against ESP-IDF v5.4.2), with or without
  patch 0005. Nothing here uses WASI; it is why 0005 is held from upstream (abacus #5535).
- Upstream: 0004's PR is drafted (stu32-arcade probes/wasm/upstream/pr3.md) and waits on Michael. #5146 and #5148
  are posted.
