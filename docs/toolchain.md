# Toolchain rulings

## ESP-IDF for on-chip tests (ruled 2026-10-03)

Asked by soroban through workspace-basecamp (charter: any third-party framework needs Michael's
ruling). Michael's answer, as basecamp relayed it (chillacks #3724): "yes, that's totally
acceptable, and fine to do on future boxes we commission."

The shape that answer approved, a standing rule for any box we commission:

- Espressif's official image, **`espressif/idf:v5.4.2`**, pinned to that tag (the tag lives in
  `tests/device/run.sh`; change it only by a commit that says why).
- Flashing and capture through `docker --device` for the board's serial port only, with the
  port's group passed in; nothing installed on the host; the agent user stays out of `dialout`.
- No board-vendor BSP code copied; the test app touches only the serial console.
- ESP-IDF is Apache-2.0. It builds the test app in `tests/device/`; the core in `core/` stays
  portable C11 with no platform calls.

## clang for the gate (ruled 2026-10-09)

Asked by soroban through workspace-basecamp (#5434); basecamp's answer (#5436): yes, as the espressif/idf shape already
ruled (#3724), Michael's decision 59 leaving gate quality to the owner. Its conditions, kept here:

- **The image:** the official `ubuntu:24.04`, pinned by digest
  (`sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3`), plus `clang-18` from Ubuntu's own archive
  (tools/clang/Dockerfile). Built here as `abacus-clang:18`, image `sha256:afac9b154693ca25481cdffb3052b0ab9292e5bfb7eed9222546abce2eb9dd88`,
  `Ubuntu clang version 18.1.3 (1ubuntu1)`.
- **Compile only:** `make clang-check` (in all-checks) compiles every C file of the core, the firmware, the tests and
  Casimir with `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wsign-compare`. Nothing is linked or shipped from it.
  gcc stays the ship compiler. Nothing is installed on the host.
- **Why:** primer's wasm build is clang, and found two bugs gcc passes: draw_test's unsequenced arguments (abacus
  #5289), and casim 9cc5d45's `-Wsign-compare` at casim.c:4263 (#5431). The latter failed this check before casim
  cc62dfd.
- **A planted control** (tools/clang/control.c, casim's own line) must fail, or the check reports that its flags don't
  bite.
