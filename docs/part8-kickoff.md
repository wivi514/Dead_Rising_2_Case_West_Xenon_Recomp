# Part 8 kickoff — the live hand-off

**Written at the end of part 7 (2026-08-30). THIS IS THE LIVE ONE.**
`part7-kickoff.md` is superseded; its campaign ran to completion.

Read this, then `docs/xenia-capture-analysis.md` (findings 68-71 are part 7) and
`docs/perf-part7-notes.md` (the campaign log, every stage and both refutations priced).

---

## 1. WHAT PART 7 WAS: the parallel-record campaign, run to its honest end

One overnight session (2026-08-29 evening → 2026-08-30 morning), staged exactly as the
operator agreed, every stage gated by the order gate + poisons and a 3-accepted-runs
A/B on the soak pipeline. ~450 M draws of correctness proof across the night's gates.

### Banked: THE STACK IS THE DEFAULT — +0.35 ms/frame at the crowd

Per-range **secondary command buffers** + **range-batched deferred replay**
(capture decodes and uploads; `RecordDrawCore` replays a range's tickets
back-to-back). All eight draw bands positive vs the old defaults, +3.3% at the
decisive 6,500-7,000 band, confirmed in play by the operator twice (~103 M draws in
their own final session, 0 dropped) — and RE-CONFIRMED on the flipped binary itself
(+0.38 ms / +3.5% decisive band, +2.0% weighted, 3+3 accepted runs). Controls:
`CW_VK_NO_SECONDARIES=1` / `CW_VK_NO_DEFER_RECORD=1` (each announces; the
pre-part-7 recorder).

### Proven and parked: the worker flip is CORRECT AND NULL (findings 70-71)

`CW_VK_PREC_EXEC=1` records ranges on the shared guard-pool workers — 110 M draws /
0 order failures / 0 failed draws at the crowd — and buys nothing: the pump steals 65%
of ranges (jobs arrive too late in each pass), and the deferred core is only
~0.3 µs/draw once capture-side resolve moved the rest. Raising the worker share to 52%
(range 64) changes nothing — the share was never the bottleneck. **The campaign's
lesson (finding 70): each stage of making recording safe to distribute also made it
cheaper to keep serial.** The sibling's refutation, reached from the other side, at
proof scale.

### The structure it leaves behind (this is the export)

* `runtime/gpu/parallel_record.{h,cpp}` — the range partition on the shared pool,
  order-gate reconstruction, id/count modes, poisons. **Case Zero never built any of
  this; the export flows TOWARD them for the first time** — module plus call sites.
* The **DrawTicket seam**: DoDraw decodes/uploads at capture; the minimal record core
  (state cache + binds + draw) takes a `RecordCtx` and can run anywhere. Any future
  recording work lifts this seam.
* Stage 1's own A/B failed and forced a redesign (finding 68) — the pre-registration
  discipline caught a 0.4 ms regression before it shipped.

### Also closed in part 7

* **DECALS ARE FIXED** (operator's statement at part close; mechanism not established
  — do not claim one). `docs/imported-fixes.md` updated.
* The harness stamps every soak run's **binary sha256** (a mid-chain rebuild nearly
  relabelled an A/B arm — the rule is NO BUILDS WHILE A CHAIN MEASURES).

## 2. WHERE PART 8 STARTS (updated at the 2026-08-30 stop)

**Two censuses are owed their local verdicts** (Case Zero measured both and refuted
both AT THEIR CROWD — same engine, but a copied conclusion is not a measurement):

1. `CW_VK_VCULL_CENSUS=1` at the crowd — is the frustum-cull pool real here? (Theirs:
   0.1% off-screen; the engine culls before submitting. Caveat there: only ~36% of
   crowd draws classifiable.) One diagnostic soak; the run was interrupted mid-chain
   at the stop.
2. `CW_VK_REUSE_CENSUS=1` at the crowd — the four-cell reuse census, WRITTEN AND
   BUILT this session, NEVER RUN. (Theirs: crowd 1.2-2.1% reusable; 93-94% of draws
   differ only in ALU constants. Our light bucket is the positive control — must read
   high like their menus' 69-77%.)

Then findings 72+ with the sibling cross-reference confirmed or diverged. Their ask
back: our 2a/2b restructure is worth exporting to them once they re-price it.

## 2b. THE OPERATOR'S PARKED LIST

**The operator has a list of MINOR VISUAL ISSUES they observed and deliberately did
not describe** — they said so explicitly, to prevent them being chased before the
important work. **Do not hunt them. Ask what part 8's brief is; they will name the
issues when they matter.**

## 3. THE BACKLOG OTHERWISE (carried from part 7)

1. The game-side FOV (their `camera_fov.cpp` recipe transfers, addresses do not).
2. `find_dropped_branches.py` — the owed W0 gate, still never run.
3. Eight unconsumed B4 capture frames.
4. The F3/DebugEnter boot-logo crash (parked by the operator).
5. The shader-bank completeness test (a repeat dump run over covered ground returning
   zero new shaders — the bank is at 480).
6. The parallel-record module's export to Case Zero, when the operator wants it —
   staged as a module drop plus call sites by design.

## 4. MEASUREMENT RULES (accumulated; the part-6 set plus part 7's)

* Arms in one display state, back-to-back; only accepted runs; verdicts only through
  `cw_trace_band.py`; one replay chain at a time.
* **NO BUILDS while a soak chain runs** — the harness copies `build/cw_runtime` per
  replay; the log's `sha=` field is the audit.
* A surgical script asserts every slice's ORDER and every pattern's PRESENCE before
  writing (two first-occurrence `.index` bugs in one night, both caught pre-build).
* `CW_VK_GPU_PASSES` and the walk-time record-core diagnostics need the control arms
  (`CW_VK_NO_SECONDARIES` / `CW_VK_NO_DEFER_RECORD`) now that the stack is default.
* `CW_VK_PROFILE` is refused under `CW_VK_PREC_EXEC` (phase sinks are not
  thread-safe).
