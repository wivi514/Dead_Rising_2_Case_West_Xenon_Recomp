# Part 7 — the maximal parallel-record campaign (2026-08-29 — )

**The brief, from `docs/part7-kickoff.md` §2:** the campaign is GO (operator,
2026-08-29). Staging agreed in advance: (1) a worker-pool skeleton that records ranges
but still executes serially, proven under the order gate with zero behaviour change;
(2) execution flipped to secondary command buffers behind an arm,
`CW_VK_NO_PARALLEL_RECORD=1` as the control. Ceiling arithmetic at the heavy band
(6,800 draws, wall 11.1 ms): the record/decode portion ~3.4 ms, 3-worker ceiling
**~2.3 ms**. Workers SHARE the guard pool's three threads — the operator's call — and
that trade is measured, not assumed.

This file is the campaign log. Findings that graduate go to
`docs/xenia-capture-analysis.md`; this is where the working numbers and dead ends live.

---

## 1. Stage 1 — the range skeleton (2026-08-29, committed `8ebe410`)

**What it is.** `runtime/gpu/parallel_record.{h,cpp}` (namespace `prec`), a module plus
named call sites — deliberately, because this item's export flows TOWARD Case Zero for
the first time (verified in their tree: they never built it), and their own
three-way-merge lesson says the import must be a module drop, not a hand-merge.

The pump partitions each frame's draws into **contiguous ranges** (broken at every
rendering-instance boundary — a secondary can inherit exactly one — and at a size cap,
`CW_VK_PREC_RANGE`, default 128). Every closed range is dispatched to the **shared
guard-pool workers**, whose stage-1 job is only to FNV the range's draw identities.
At the frame boundary the pump steals back any unclaimed job (so the drain is bounded
by one in-flight hash, not by a worker's guard chew), waits out the remainder, and the
**order gate rebuilds its "submitted" sequence from the ranges concatenated in creation
order** — the exact reconstruction a parallel submitter performs from its secondaries.
Execution itself does not move: every draw still records into the primary, in walk
order, on the pump.

**What stage 1 proves:** the partition covers every draw exactly once in order; the
dispatch/out-of-order-completion/in-order-concatenation machinery works against the
live pool; the drain never stalls the frame boundary; and the whole thing is free.

**The five gate proofs** (title screen, ~45 s arms, before the commit):

| arm | prediction | result |
|---|---|---|
| `CW_VK_ORDER_GATE=1` (skeleton on) | 0 failed | 13,179 frames / **0 failed**; 471,225 of 471,240 range jobs ran on workers (15 inline before the pool started, 0 stolen), 0 mismatches, drain blocked 0 |
| `CW_VK_ORDER_POISON=5` | every frame fails | **11,546/11,546 failed** |
| `CW_VK_PREC_POISON=1` (range transposition) | fails | **7,447/7,463 failed** — the 16 passes are single-range frames the poison cannot transpose |
| `CW_VK_PREC_POISON=2` (worker-hash perturb) | mismatch counter fires, gate passes | **228,781 mismatches, 0 failed** |
| `CW_VK_NO_PARALLEL_RECORD=1` | announces, gate passes on fallback | both |

Title-screen shape for the record: ~630 draws/frame in ~36 ranges (32 pass-break pairs
per frame — the frontend is many small passes), 17.6 draws/range.

## 2. Stage 1 at the heavy band: the gate passed, THE NULL FAILED, and the skeleton was revised by its own A/B

Pre-registered before the runs: **(a)** the gated soak-route run holds 0 failed frames
at 6,500-7,000 draws; **(b)** the A/B (skeleton on vs `CW_VK_NO_PARALLEL_RECORD=1`,
3 accepted runs per arm, alternated back-to-back in one display state) reads **NULL**
through `cw_trace_band.py` — stage 1 is bookkeeping only, and a skeleton that costs
frame time before it distributes any work is a defect, not a stage.

**(a) PASSED, at scale** (run `prec7gate_r1`, 2026-08-29): 23,951 frames, **99.1 M
draws, 0 failed**, 1,611,302 ranges (67.3/frame at the crowd; peak frame 7,329 draws in
96 ranges), 1,611,286 jobs on the shared workers, 0 stolen, drain never blocked, 0
mismatches. A Case Zero session ran on the machine for part of this run (operator);
that contaminates nothing — this is a correctness gate whose frame times were declared
non-quotable in advance, and scheduler contention only makes the concurrency proof
harder. The A/B runs began after Case Zero exited.

**(b) FAILED — the first honest refutation of this campaign** (runs `prec7on_r{1,3,5}`
vs `prec7off_r{2,4,6}`, 7/7 accepted, no attrition): the skeleton-on arm was SLOWER in
every heavy band with a dose-response gradient — 5,000-7,000: −1.3%, −2.2%, −4.2%,
**−3.6% / −0.41 ms at the decisive 6,500-7,000 band** (26,120 vs 21,312 frames). Light
bands null (+0.6% at 500-1,000, 39 k frames each). The 7,000-7,500 row (−9.5%) is a
composition artifact: 468 vs 4,719 frames, and the on-arm's few frames there carried
9.05 ms of GPU against 7.52. Verdict: **stage 1 as first committed cost ~0.4 ms/frame
at the crowd — ~60 ns/draw for bookkeeping nobody consumed.**

**The mechanisms, all of the boring kind** (commit `8ebe410`, revised in the next
commit): (1) the kick was `notify_all` — three workers waking for a nanosecond hash
job, ~67 times a frame; (2) the workers' wake predicate took prec's mutex on every
guard-pool wake; (3) `prec::On()` put a static-init guard load on the per-draw path —
the exact shape gotcha 453 already took off it once; (4) the per-draw identity hash +
vector append ran in every run, gate or no gate.

**The revision** — two modes, chosen by the caller per draw:

* **id mode** (order gate armed): full identities, per-range single-worker kick on the
  queue's empty→non-empty transition. The complete concurrency proof, priced as a
  diagnostic arm.
* **count mode** (every default run): ranges carry a draw count; no per-draw hash, no
  per-range kick (one at FrameSeal if anything is still queued); `HasWork` is one
  atomic load; `prec::g_on` is a plain global. Title-screen shape: **8 kicks in 13,385
  frames** against id mode's ~468,000 — the workers absorb the jobs opportunistically
  on wakes they were already taking.

Stage 2 note: real range jobs are tens of microseconds, so per-range wakes stop being
pathological exactly when the jobs become worth waking for. Count mode is stage 1's
shape, not the campaign's.

All four title-screen gate proofs re-passed on the revision (clean 0/13,215; range
poison 13,172/13,184 failed; hash poison 470,818 mismatches with the gate correctly
passing; control announces). **Owed: the heavy-band gate rerun and the A/B rerun on
the revised binary — same pre-registered null.**

*(v2 results land here)*

## 3. Stage 2 — the flip (not started)

The hazards, named by the sibling's §6eb §3c and adopted by the kickoff, live here:

* Workers record their ranges into **secondary command buffers** (dynamic-rendering
  inheritance); the pump executes them in range order inside the pass's instance. The
  range breaks stage 1 placed at pass boundaries are exactly the legal ones.
* **Per-draw capture**: a worker needs the draw's register state (fetch constants, ALU
  windows, render state) as of the walk — the capture cost is real and was the sibling's
  reason to price this as a campaign.
* **UploadStream re-entrancy** against the shared stream store, and an arena discipline
  that must not change which buffer a stream lands in (a per-worker arena would defeat
  the vertex/index bind cache).
* Each range **re-establishes full state** — the `R->bound` elision cannot span a range
  boundary.
* `RunImmediate` (texture uploads) stays on the pump; a range that needs one breaks.
* The guard-pool share: each worker is ~23% busy on prehash at the crowd; the trade of
  loading them with record work is measured (guard served-% before/after), not assumed.
