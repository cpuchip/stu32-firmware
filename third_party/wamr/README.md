# WAMR, pinned (STU_WASM)

WebAssembly Micro Runtime, github.com/wasm-micro-runtime/wasm-micro-runtime, under Apache-2.0 WITH
LLVM-exception (its LICENSE). It runs .wasm games as .aot built by our pinned wamrc (decision 67: Michael,
"WAMR, and send the fix upstream too"); its costs are counted under decision 68. Only with `STU_WASM`
on; off, nothing of it is built.

`scripts/fetch-wamr.sh` exports `PIN`'s commit from a local clone and applies `patches/` in order, checking
each patch's sha256 and the resulting tree against `PIN`. The patches are modifications of WAMR, under its
licence:

1. `0001` maps PSRAM AOT code on the instruction bus (upstream PR #5146; issue #5145).
2. `0002` fixes the Xtensa L32R relocation lost to strict aliasing (upstream PR #5148; issue #5147).
3. `0003` makes `os_self_thread()` return the FreeRTOS task handle, so apps on the calc task can call
   AOT code (local; soroban's ruling #5413). Its audit and the chip proof are in stu32-arcade
   `probes/wasm/wamr-local-os-self-thread.patch`.
4. `0004` makes every architecture's target symbol map const, so it sits in flash, not internal DRAM (local,
   decision 68: with it and `WAMR_BUILD_QUICK_AOT_ENTRY=0` the option costs the stand-in 324 B of static
   D/IRAM, not 2,100, and the stand-in boots). Audit: stu32-arcade `probes/wasm/wamr-local-const-symbol-map.patch`.
5. `0005` lets a project without libc-wasi leave lwip out (local, decision 68): the device builds set
   `WAMR_NO_LWIP`, so the option brings no VFS or console drivers, and Rev A with the option on and no game
   is main's size. Audit: stu32-arcade `probes/wasm/wamr-local-no-lwip.patch`.

Repin: move `commit`, drop each patch upstream has merged, regenerate the rest on the new commit, and
record their sha256s and the new `git write-tree` in `PIN`; then measure the decision 68 table again.
