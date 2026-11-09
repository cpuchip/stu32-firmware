# Unit 092: the store's wear (blueprint, before any code)

**From:** unit 091's wear test and its (b) (work/091-erase-time.md); abacus #14129 (in scope as 092; blueprint after part
3's, before cas-steps-device), with its five items: where the image starts across the slot's sectors; how load finds the
newest image, and what an older firmware's image looks like to it, which must load; the erases per sector under the three
uses; power loss mid-save; SAVEBENCH before and after.

**Status:** revision 1 (abacus #14138: a blocking finding in lean 1, and the four questions answered; the revision's
section is at the end, and lean 1 below says where it's struck). No code.

## Today (firmware/store.c, measured)

- **Two slots, each half the partition:** the state's abstate is 512 KiB, two slots of 256 KiB, 64 sectors each; the
  apps' abapps is 72 KiB, two slots of 36 KiB, 9 sectors each.
- **A save:** it examines both slots (each header, then each payload's CRC, read back). The target is the slot without
  the newest good image. It erases the sectors the image needs from that slot's start, writes the image, then writes the
  header last. The header commits the save: "ABST", seq, len, the payload's CRC, the header's CRC.
- **A load** takes the good slot with the higher seq.
- **So every save erases its slot's first sector,** whatever the image's size, and the slots alternate. Each slot's first
  sector takes half of all saves (091 (b): up to 21,352 a year at the bound of an hour a day). The other 62 sectors of
  the state's partition are erased only by images big enough to reach them.

## The lean: one ring over the whole partition

1. **Where the image starts.** The partition's sectors are one ring: 128 for the state, 18 for the apps.
   - ~~A save starts at the sector after the newest good image's last sector. When the image wouldn't fit before the
     partition's end, it starts at sector 0 instead (an image never wraps across the end).~~ Struck in revision 1: the
     no-wrap rule wastes the tail and can overlap the newest image (abacus's counterexample). The image wraps instead.
   - It erases only the sectors the image needs there, writes the image, then writes the header last, exactly as now.
     The header's format doesn't change.
   - ~~The new image can never overlap the newest good one: the largest image (IMAGE_CAP, 256 KiB, 64 sectors) plus the
     newest is at most 128 sectors. For the apps, APP_SAVES_IMG_MAX 32,826 B is 9 sectors, and 9 + 9 = 18.~~ False as
     written (revision 1): it holds only when the new image may use every free sector, which the no-wrap rule forbids.
2. **How load finds the newest.**
   - Read the 32-byte header at every sector's start: 128 reads of 32 B for the state.
   - Keep the headers whose own CRC is right. Try them from the highest seq down; the first whose payload CRC matches
     is the newest good image.
   - **An older firmware's image** sits where the two-slot store put it, at sector 0 or sector 64 (the slots' starts),
     with the same header. So the scan finds it like any other, and it loads. The newest by seq wins, as it does today
     between two slots.
   - apps.h v1.2's migration (an old 8 KiB slot inside today's 36 KiB one) is one more image at a sector start, so the
     scan finds it too. ab_store_migrate becomes a no-op, kept for its callers.
3. **The save's cost.** A save needs the newest image's position. The store keeps it in RAM after a load or a save, so
   it scans only when it doesn't know it (the first save after a boot that found no image). Either way, the payload CRCs
   are read only for the candidates, newest first.
4. **Power lost mid-save.** The newest good image is never touched (1), so a cut at any moment leaves it.
   - A cut during the erase: the sectors held older images, now partly gone, whose payload CRCs fail; they're skipped.
   - A cut during the payload's write: no header yet, nothing to find.
   - A cut during the header's write: its CRC fails, skipped.
   - An old header whose payload a newer save has partly erased fails its payload CRC and is skipped.
   - So the load always finds the newest image that finished, as today. store_test's "power cut at every moment"
     (5,452 cases today) is extended over saves that wrap the ring.
5. **What the change costs.** A load reads up to 128 headers, which is milliseconds. A save erases what it erases today
   (the sectors its image needs), and reads less: today's examine reads both slots' payloads back before every save,
   while the ring already knows its newest. Flash for the code is
   small, measured by D68.

## Erases a year of the most-worn sector (an hour a day, 365 days, one OFF a day; 091 (b)'s uses)

Today: every save's first sector, whatever the image's size. The ring: each save erases k sectors out of 128, so each
sector takes k/128 of the saves.

| use | saves a year | today | ring, 1 sector (a typical 1,237 B state) | ring, 3 sectors (the full page, 9,138 B) | ring, 7 sectors (the worst, 27,683 B) |
|---|---|---|---|---|---|
| a pause every 6 min | 4,015 | 2,008 | 31 | 94 | 220 |
| a pause every 2 min | 11,315 | 5,658 | 88 | 265 | 619 |
| the bound | 42,705 | 21,352 | 334 | 1,001 | 2,335 |

At the bound, the datasheet's 100K (091 (a), the GD25Q64C's minimum) is reached in 4.7 years today. With the ring it's
300 years for a typical state, 100 for the full page and 43 for the worst. The apps' ring (18 sectors) takes the apps'
saves, which happen only when an app's state changed (app.c's apps_dirty).

## What it doesn't solve

- **A downgrade.** An older firmware looks only at sectors 0 and 64. After the ring has moved, it finds an old image
  there, or none. Going back a version after this unit would load an older state, or MEMORY CLEAR. Nothing is sold, and
  Rev A's bring-up flashes forward only; the record says it.
- **The bench chip's two worn sectors** (091) stay worn. The ring uses them one save in 128 (or in 18 for the apps), so
  SAVEBENCH's max will still catch them sometimes.

## What the build would test (for abacus's oracle)

- **store_test (host, simulated flash):**
  - the power cut at every byte and every erase, over enough saves to wrap the ring twice, with sizes from 1 to 64
    sectors;
  - after each cut, the load finds the newest finished image;
  - an older firmware's two-slot image (written by today's code into a fresh simulated partition) loading;
  - a mixed partition (old slots plus newer ring images) giving the highest seq;
  - an image whose header is good and whose payload is half erased being skipped;
  - the erase count per sector over 10,000 saves, against the table above (each sector within a stated band of
    saves × k / 128);
  - the apps' 18-sector ring, with its largest image.
- **Fixtures:** a state partition written by today's store (two slots), checked in, loading under the ring.
- **SAVEBENCH before and after,** on the stand-in, for both images: the 20-save distribution of the three states.
  Expected: each save's erase as before (its sectors are fresh, or the two worn ones one save in 128), and the save
  shorter by today's examine (its reads: medians 0, 10 and 33 ms for the three states in 090's SAVEBENCH at
  ca1d330).
- **Mutants,** predicted first: the start not moving; the image wrapping across the end; the load taking the first
  header found, not the newest; the header written before the payload; the overlap check removed.

## Questions for abacus

1. The ring over the whole partition (both slots merged), not a ring within each slot. A ring within each slot would keep
   an older firmware's view of slot 0 and slot 64, but halves the spread.
2. The downgrade cost as stated (nothing sold; Rev A flashes forward).
3. ab_store_migrate made a no-op once the scan finds every sector-aligned image.
4. The apps' store moved to the ring in the same unit (the same code), or left on two slots.

## Revision 1: abacus #14138

**The blocking finding, owned.** Lean 1's guarantee was false. Under "start at sector 0 rather than split across the
end", a newest image at sectors 61-76 and a new 64-sector save: 77 + 64 passes 128, so the save starts at 0 and takes 0-63,
erasing 61-63 of the only good image. A cut there loses it. (Checked: the no-split rule's range overlaps the newest; the
circular range, 77 to 12, doesn't.) The apps' ring breaks the same way (newest 8-15, a 9-sector save at 0-8).

- **The fix: a true circular buffer.** A save starts at the sector after the newest image's last, modulo the
  partition's sectors, and its k sectors run on circularly. The header is at its first sector; the payload continues
  past the partition's end at sector 0.
  - The erase is one range or two; the payload's write is one piece or two; the header is still written last.
  - The load reads a payload circularly for its CRC and into the buffer.
  - The new image ends before the newest's start whenever newest + new ≤ the partition's sectors: 64 + 64 = 128 for the
    state (IMAGE_CAP), 9 + 9 = 18 for the apps (APP_SAVES_IMG_MAX). Now that's true for every position, since the new
    image uses the free sectors contiguously from the newest's end. The store refuses an image past half the partition
    (it couldn't keep the guarantee), as it refuses one past a slot today.
  - An older firmware's image never wraps (it starts at a slot's start and fits in it), so it reads as a ring image
    whose run happens not to wrap.
- **The questions:**
  1. The whole partition: as ruled.
  2. The downgrade cost: accepted as stated; the record will say so.
  3. Migrate: apps v1.2's old second slot is read by the same examine() as every slot (store.c:85,
     `examine(&old, 1)`), so its header is the same "ABST" form, at 0x2000, a sector start. The scan finds it. The build
     adds an apps v1.2 fixture (8 KiB slots, the newest in the old second) loading under the ring, and keeps today's
     migrate test ("migration after the slots grew: the newest image kept") unchanged and green. Only then does migrate
     become a no-op.
  4. The apps in the same unit, with the circular form.
- **Tests added for the finding:**
  - the counterexample as a store_test case (a newest at 61-76, then a 64-sector save, cut at every byte and every erase,
    and the load giving the newest that finished after each cut);
  - the apps' version (newest 8-15, a 9-sector save);
  - random sizes up to the cap over many wraps, with cuts.
  - The "overlap check removed" mutant must go red on the counterexample.

## The hidden set's interface: tests/store_sim (a new host tool)

`build/store_sim FILE` reads lines and prints a FAIL line for each expectation that fails, then a total. Its exit is 0
when every expectation holds. A line is a command and its arguments, separated by blanks; `#` starts a comment.

- `PART n s`: a fresh simulated partition of n sectors of s bytes, every byte 0xFF, and a store over it (the state's
  is `PART 128 4096`, the apps' `PART 18 4096`).
- `OLD slot bytes seq`: an image written the way today's two-slot store writes one: at slot 0 or 1 of a two-slot
  layout over the partition (slot size n×s/2), header and payload. bytes from 1; content as below. This makes the
  older-firmware fixtures.
- `SAVE bytes`: a save through the store of an image of that many bytes. Its content is a function of its own seq (byte
  i is (seq × 31 + i) mod 251), so a load can check it's the right image whole.
- `CUT bytes budget`: the same save, with the power cut after budget flash operations: a byte written or a sector
  erased each counts one. After it, the store is rebooted (its RAM state dropped).
- `CUTALL bytes`: the save repeated from the same starting partition with the cut at every budget from 0 to the
  save's own count. After each cut, a load must give the newest image that finished (the one before, or this one if the
  cut came after its header). It prints the number of cuts and how many held.
- `EXPECT LOAD seq` or `EXPECT LOAD NONE`: a load (after a reboot) gives the image of that seq, its content whole, or
  none.
- `EXPECT ERASES sector lo hi`: that sector's erase count since PART, from lo to hi.
- `EXPECT MAXERASES hi`: no sector's count above hi.
- `REBOOT`: the store's RAM state dropped (the next save scans).
