# Part 7 kickoff — the live hand-off

**Written at the end of part 6 (2026-08-28/29). THIS IS THE LIVE ONE.**
`part6-kickoff.md` is superseded; its backlog items that survive are folded into §3.

Read this, then `docs/xenia-capture-analysis.md` (findings 63-67 are part 6) and
`docs/perf-part6-notes.md` (the campaign notes, including every dead end priced).

---

## 1. WHAT PART 6 WAS

The operator's brief: *find everything fixable for performance without degrading the
game, ~5 ms/frame of gain, keep going until exhausted.* Part 6 delivered one measured
item, refuted two plausible ones, and — the bulk of the part — built the thing every
future perf claim runs on: **an autonomous heavy-band measurement pipeline.**

### Banked: the ring-latency package — −0.19 ms/frame at the heavy band

Eager tick + mid-walk rptr publication + the fast-retry backoff (naps after an
unproductive walk start at 5 us and double to the 100 us floor). Measured at
6,500-7,000 draws over 3 accepted soaks per arm: +1.7%, every heavy band in its
favour, mechanism confirmed (pump sleep 0.47 → 0.15 ms/frame). Defaults ON; control
arms `CW_PM4_NO_EAGER_TICK`, `CW_PM4_NO_MIDWALK_RPTR`, `CW_PM4_NO_FAST_HELD`.

### Refuted / closed

* **CW_PPC_O3** (guest image at -O3): null at the heavy band. The build option stays
  (runtime/build-o3) for re-litigation; default build stays -O2.
* **AutoChuck levels**: spawn ZERO zombies on this runtime — their draws were
  architecture. Not a load source.
* **The guest-side theory of the frame**: the light-scene guest-thread saturation
  (finding 63) is ring-space spin; at the crowd the pump owns the frame outright
  (wall 11.1 ms, walk 10.97).

### Built (the pipeline — findings 65-66)

`cw_route_record.sh` (one operator recording, pad-bias calibrated) →
`cw_transcribe_route.py` (compound entries, 40 ms lerp knots) → `config/cw_soak_route.seq`
(WAITWORLD-anchored) → `cw_crowdroute.sh` (gated, per-frame trace, BIN_SRC for binary
arms) → `cw_soak_until.sh` (K accepted runs) → `cw_trace_band.py` (banded verdicts).
Plus `cw_crowd_perf.sh` (perf at the heavy band) and `cw_perf_profile.sh` (light scene).
Every run under 3 minutes (operator's cap). 6/6 acceptance in the evening session.

**The route's saves**: slot 2 area is the operator's crowd save (Holding Pens); the
soak route loads slot 1 (Shipping Office bathroom spawn) and walks to a 6,800-draw
vantage. If the SAVE FILE changes, the route breaks — treat `assets/save/DR2E000.DSF`
as part of the measurement configuration.

### Fixed on the way (correctness, not perf)

* **Synthetic input is ONE pad now** — it used to press START on every user index at
  once, binding the unsigned pad-1 user; Load Game refused the save (the whole class
  of "not signed in" replay failures).
* The settings log named `cz_settings.txt`; the store writes `cw_settings.txt`.

---

## 2. THE DECISION PART 7 OPENS WITH — the operator's, by the sibling's own governance

**The maximal parallel-record campaign** is the one large CPU item left anywhere
(sibling §6eb §3c; local numbers below). Its technical prerequisite is MET: the order
gate is proven alive both ways (serial 20,063 frames / 0 failed; POISON=100 fails by
name). What remains is the trade the sibling explicitly reserved for the operator:
**its workers are the guard pool's 3 threads** (each ~23% busy on prehash at the crowd).

Local arithmetic at the heavy band (6,800 draws, wall 11.1 ms, all CPU):

| slice of the pump's 11 ms | size | what could take it |
|---|---|---|
| our draw-path code | ~8.9 ms | the campaign moves the record/decode portion (~3.4 ms at 0.5 us/draw) to workers — **ceiling ~2.3 ms** with 3 |
| the driver's calls | ~2.1 ms | secondaries ceiling ~1.4 ms — the sibling killed this variant at their load; marginal here |
| libc (copies) | ~1.4 ms | part of the above |

GPU runs 7.3 ms concurrent with fence 0 — **after ~3.8 ms of CPU savings the GPU
becomes the wall**, which is when the GPU items (scoped clears ~0.6, resolve copies
~0.7) start converting.

**So the honest 5 ms path is: campaign (~2.3) + walk micro-items (~0.5-1) + GPU pair
(~1.3) + banked (~0.2) — and it is a multi-session campaign, not an evening.**

## 3. THE BACKLOG OTHERWISE (carried from part 6)

1. Decals (Case Zero's 00m, still theirs).
2. The game-side FOV (their `camera_fov.cpp` recipe transfers, addresses do not).
3. `find_dropped_branches.py` — the owed W0 gate, still never run.
4. Eight unconsumed B4 capture frames.
5. The F3/DebugEnter boot-logo crash (parked by the operator).
6. A repeat shader-dump run over covered ground returning zero new shaders (finding 62's
   completeness test) — the bank is at 480.

## 4. MEASUREMENT RULES THAT NOW EXIST (learned expensively)

* An A/B whose arms ran in different display states is inadmissible (occluded-window
  presentation throttling: 48-58% fence where attended runs read 3-13%).
* Route replays are gated and retried, never trusted individually; only accepted runs
  enter a comparison, and only through `cw_trace_band.py`.
* The soak route is pump-side ground truth only; the operator's crowd save is the
  oracle for whole-frame and guest-side claims.
* One replay chain at a time: two of this part's launches were blocked by the previous
  chain's own game process (pgrep -x on the TRUNCATED name `cw_runtime_crow`).
