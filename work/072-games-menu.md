# Work unit 072: the GAMES folder under APPS

**From:** Michael's ruling through abacus #8570 (arcade #8561's shape), #8600 · **Proposed:** soroban #8677 ·
**Ruled:** abacus #8679 · **Status:** built on the host; the stand-in's scripts to follow.

## What it does

- **One registry:** app_registry stays the one list (KEYS, then games.h's GAMES_ENTRIES), so nothing indexed by it
  moves. Beside it, app_registry_folder gives each app's folder: KEYS a tool, and each game its manifest's
  (GAMES_FOLDERS, gen_games.py's numbers: TOOLS 0, GAMES 1).
- **APPS** (blue 0, KM_PAGED_APPS) lists the apps outside the GAMES folder (KEYS, and any TOOLS game later), then a
  GAMES key when any game is listed. Today that's KEYS and GAMES.
- **GAMES** (KM_PAGED_GAMES, appended last in gen_keymap's list so no menu's index moves) lists the GAMES folder's apps
  in the registry's order, paged as APPS and CONST are: five a page, then more, wrapping to the first page.
  - C or back-arrow returns to APPS. In APPS they close it, as before.
  - A game starts as from APPS (app_start), and leaving a game is as before.
- A list given without folders (app_apps, a test's) keeps every app in APPS, with no GAMES key.
- **Unchanged:** games.list and the games, the image (neither the menu nor its page is saved), and the page's look
  ("more": stu-nav's page mark comes later, for every paged menu at once).
- **The wasm gate's build** (STU_WASM, games-wasm-gate.list) uses the same registry and folders, so its three games are
  under GAMES too. games-wasm-gate.list doesn't change.

## The device scripts that reach a game through APPS

Each game's key moves: APPS's GAMES key (K1), then the game's place in GAMES.
- **aim.txt:** K1 K1.
- **babal.txt:** K1 K0, and its expect now names BABAL.
- **land.txt:** K1 K2.
- **snake.txt:** K1 K3.
- **mines.txt:** K1 K4.
- **salvo.txt:** K1, more (K5), K0.
- **wasm.txt:** BABAL K1 K0, BABLW K1 K1, TRAP K1 K2.

Each script's expect names its game.

## Checks

- **app_test:**
  - The menus' legends test builds APPS's expected list from the registry and its folders, independently of the code.
  - A new block: APPS shows KEYS and GAMES; GAMES's first page (five games and more); its second (SALVO, last); more
    wraps; C and back-arrow return to APPS; C in APPS closes it; a game starts from GAMES; a list without folders
    keeps all in APPS. 719 → 728 checks.
- **Mutants, each red on its own checks:**

| mutant | what it breaks | failed |
|---|---|---|
| G1 | the folder filter ignored | 7 |
| G2 | GAMES never pages (CONST's paging too, the shared code) | 6 |
| G3 | C in GAMES closes it | 3 |
| G4 | APPS's GAMES key opens nothing | 6 |
