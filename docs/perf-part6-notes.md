# Part 6 — the performance campaign (2026-08-28/29)

**The goal, set by the operator:** find everything fixable for performance without
degrading the game, target ~5 ms/frame of gain, keep going until exhausted.

**Where the milliseconds must come from, and why this file exists.** The sibling's
closing accounting (their part 81) found no single large CPU item left at their load and
predicted the same shape here. Part 6's first measurement refuted the premise instead:
**this port is not in the sibling's regime** — see finding 63. Everything below is
ordered by what it established.

---

## 1. What was measured

* **Finding 63** — the first uninstrumented perf profile (`tools/cw_perf_profile.sh`).
  The guest render thread SATURATES (~1.1 cores) while the pump idles 23% at the HUD
  scene; the two biggest symbols in the process are the guest's ring-space spin
  (sub_825B7668 / sub_825B5FB8, 16.7%). GuardFold's 16.5% is almost all on the three
  prehash workers, off the critical path. **A pump-side per-draw saving does not convert
  1:1 here the way it did in Case Zero.**
* **Finding 64** — the ring-latency pair (mid-walk rptr publication + the eager tick).
  Engagement proven both ways, the pre-registered latency bound moved 0.47 -> 0.31
  ms/frame, frame rate NULL at the HUD scene. Kept as defaults (free, closer to
  hardware); re-litigate at the heavy band with `CW_PM4_NO_MIDWALK_RPTR=1` /
  `CW_PM4_NO_EAGER_TICK=1`. Structural fact learned: the ring carries only ~16
  top-level packets a frame — the command mass is inside INDIRECT_BUFFERs.
* **Finding 65** — X_AutoChuck (DebugJump RIGHT x4) reaches 3,100-4,800 draws… and the
  operator then observed **the draws are architecture, not zombies: the AutoChuck
  levels spawn NO crowd on this runtime**, with and without the ignore-humans flag
  (SKIP_Y arm). Chuck's roam also degenerates (wall-walking, a storage-bay teleport),
  so hands-off soaks collect almost nothing; the camera sweep rescues samples but the
  medians stay ~1,000 draws.

## 2. What is built and waiting

* **`CW_PPC_O3`** (runtime/build-o3) — the recompiled image at -O3, the guest-codegen
  experiment finding 63 motivates. The overnight A/B was **invalidated**: runs taken
  with the desktop idle showed 48-58% GPU fence and 146->353 fps swings at fixed draws
  (occluded-window presentation throttling suspected). **A/B arms must run in the same
  display state, back-to-back, attended.** The verdict is OWED at the heavy band.
* **The route pipeline** (Case Zero part 80's, imported): `tools/cw_route_record.sh`
  records the operator's own play with CW_INPUT_TRACE (ms + decoded names + raw axes);
  `tools/cw_transcribe_route.py` turns it into a CW_FAKE_PRESS_SEQ recipe;
  `tools/cw_trace_band.py` reads any A/B by draw band. The debug flags come from
  **this image's own tunables table** (`CW_DEBUG_TUNABLES=chuck_in_god_mode,
  disable_death_sequence,zombies_ignore_all_humans`) — no Y press; the Y-toggle byte
  0x82A74559 IS the table's `zombies_ignore_all_humans`, confirmed from the code and
  the extractor independently.

## 3. The board, priced by the sibling and re-priced here as data arrives

| item | expected | state |
|---|---|---|
| -O3 guest image | unknown — the regime says guest speed is live here | binary built, A/B owed at the heavy band |
| ring pair at the heavy band | up to ~0.5 ms + spin collapse, scales with WAIT hand-offs | re-litigate once the route exists |
| maximal parallel record | ~2-3 ms at 7k draws (sibling ceiling arithmetic) | campaign; needs the order gate FIRST, and the thread-budget trade is the operator's call |
| GPU: scoped clears + resolve copies | ~1.3 ms GPU, costs pump time | price with CW_VK_GPU_PASSES at the heavy band; only converts when fence > 0 |
| texture guard, memoisation items | — | refuted or below floor in the sibling; do not rebuild (their §6ec) |

## 4. The measurement rules this session added

* An A/B whose arms ran in different display states is inadmissible on this machine.
* AutoChuck degenerates: every soak self-gates (GATE_DRAWS/GATE_FRAMES, `.rejected`).
* The heavy CROWD load comes from the operator's recorded route, not from any debug
  level found so far.

## 5. Operator-soak session notes (2026-08-29)

* `force_survivors_idle` (0x82A7459D) is the pacify-humans flag for future soaks —
  NOT armed in the O2/O3 pair, which must differ only by binary. God mode already
  makes human attacks harmless in both arms.
* The route replay drifts: the save-load duration shifts every subsequent input, so
  Chuck misses doors and later presses land in wrong contexts (operator watched it
  pick up a fire extinguisher and scroll the pause menu). Autonomy needs a
  post-load anchor (a WAITWORLD-style barrier on in-game state) before the recorded
  route is replayable. The operator-driven chained soak is the measurement path
  meanwhile.
* The one-pad fix (imports.cpp): synthetic input served every user index, so pads
  0+1 both pressed START and the title could bind the unsigned pad-1 user — Load
  Game refused the save. Synthetic input is one pad now.
