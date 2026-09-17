# Part 13 kickoff — the live hand-off

**Written at the end of part 13 (2026-09-17). THIS IS THE LIVE ONE.**
`part12-kickoff.md` is superseded (its owed items that are still owed are repeated
below). Part 13 was one conversation answering two operator asks: *"Did some update
to case zero can you grab the ones that also applies to you. Also implement that F4
open the in game debug menu like case zero"* — with one mid-part scope call: *"For
the co-op stuff don't implement the new thing in 1.1.1 of case zero it's only for
case zero issues since it didn't release with co-op."*

Read this, then `docs/imported-fixes.md` §14 (the record), then the sibling's
`docs/lighting-plan-part120.md` if the exposure question comes up — their night
measurements are the ones that exist.

---

## 1. WHERE THE PORT IS

* **Branch: `xlive-integration`**, v1.1.0 published from it (`49f8383`), master at
  `bf076e7`. Part 13's commits are on top, **unreleased**.
* **v1.1.0 is live** (2026-09-13, four artifacts). The player reports from its thread
  are in `docs/release-notes-v1.1.0.md`'s known-issues list.
* **New since v1.1.0, all DEFAULT unless said** (§14): tiny colour resolves written
  back to guest memory (the auto-exposure's input — `CW_VK_NO_RESOLVE_WRITEBACK=1`),
  tile-replay shader-binding restore (`CW_PM4_NO_REPLAY_RESTORE=1`), the EDRAM-space
  draw placed at its tile (`CW_PM4_NO_TILE_OFFSET=1`), alpha-to-mask as
  alpha-to-coverage (`CW_VK_NO_A2C=1`), the PP leaderboard flush at 2 s
  (`CW_LEADERBOARD_FLUSH_S=360`), F8 = twenty consecutive frames, launcher/panel
  labels in the subtitle language; OFF: the display gamma ramp
  (`CW_VK_GAMMA_RAMP=1`). Plus **the F4 debug menu** under `CW_DEBUG_MENU=1`.

## 2. WHAT PART 13 MEASURED, AND WHAT IT DID NOT

* **Engagement** on the DebugJump route (`tools/cw_hud_capture.sh`): resolve
  write-back 86-92k records a run, absent on the control arm; tile-replay restores
  1.00/frame; alpha-to-coverage 125k draws; the leaderboard timer applied at the
  constructor. The tile-offset fix read 0.00/frame — the DebugJump levels spawn no
  near actors, so its trigger never occurs on the route. **Unexercised here.**
* **The exposure PICTURE is not measured here.** Mean luma of the F9 captures,
  default vs control, 57-58 vs 54-57 at the safehouse by day: inside one scene's
  sample noise. What IS established is that the consumer exists here identically
  (`sub_825A8F78`, a unique 1.000 match of the sibling's `sub_825D65A8`, the same
  `lwz`/`lha` and the same sentinel on zero). The sibling's effect was at the night
  floor. **The gate is the operator's sitting in a dark room** — before/after with
  `CW_VK_NO_RESOLVE_WRITEBACK=1` as the control, same spot, same hour.
* **Eleven guest addresses re-derived**, one of them a genuine per-title
  difference (the PP board is index 0 here, 1 there — gotcha 328).
* **The F4 menu**: 158 nodes / 45 labelled on boot; two synthetic edges -> two
  toggles; the object INTACT through a level load with the destructor hook at 0
  calls (gotcha 329). **Navigation and activation are UNTESTED** — the overlay is
  driven by SDL keys, which no headless run can press. First sitting: F4, Down to
  ORIGINAL ENGINE DEBUG ITEMS, Enter, toggle 'Draw Text' or 'Enable Performance
  Chartz' and see whether the title's own overlay appears; then a custom bool
  (CHUCK GOD MODE) and its log line.
* **Validation gate**: the same single VUID (`topology-08773`, 8 lines) on the new and the pre-import binary, both run now — nothing new. Capture STDOUT (the layer prints there) and use `timeout -s KILL` (details in §14).
* **Not measured**: Windows (czwin compile of the merge), the old-base build, the
  leaderboard write itself (needs a save on a signed-in profile).

## 3. WHAT IS OWED, IN ORDER

1. **The operator's sitting** on this build: (a) a dark interior before/after the
   write-back — the one visible claim of this import; (b) the F4 menu's
   navigation; (c) a near zombie against the right half of the screen, the
   sibling's issue-#3 shape, to see whether the tile-offset counter moves.
2. **czwin**: pull, build, `--smoke`, a boot. `ui_strings.cpp` and the renderer
   merge came across untested on clang-cl.
3. **A leaderboard check**: signed in through the launcher, save, open the board
   within ~10 s (`CW_LEADERBOARD_TRACE=1` prints the chain).
4. Then the v1.1.1 artifacts, if the operator wants them — the sibling's v1.1.1
   notes are the template; ours would carry §14's list.
5. Still owed from part 12: a co-op session past an hour (the sign-in grace), a
   real Deck, the branch -> master merge.

## 4. GOTCHAS WRITTEN IN PART 13

328 (a 1.000 shape match proves the code, not the enum — diff the immediates) and
329 (a destructor hook that never fires is not a teardown that never happens — check
the object). Both in `docs/gotchas.md`.

## 5. MEASUREMENT RULES (unchanged from part 12's §5)

Same-binary arms; the DebugJump route for engagement; the operator for pictures;
`grep` every import for the sibling's constants and strings (the rename sed is now in
§14 so it is not reconstructed again).
