// parallel_record.h — the maximal parallel-record campaign's module seam (part 7).
//
// WHY THIS IS A MODULE AND NOT MORE LINES IN vk_renderer.cpp. This is the first item
// whose export flows TOWARD Case Zero rather than from it (they never built it — their
// tree has zero secondary command buffers and their own commit calls the low-risk
// design dead). The sibling's three-way-merge lesson, applied in reverse and in
// advance: keep the campaign in one file plus named call sites, so the eventual import
// into the sibling is "add the module, add the call sites" rather than a hand-merge of
// a 23,000-line renderer.
//
// STAGE 1 (this file today): a worker-pool skeleton that RECORDS RANGES BUT EXECUTES
// SERIALLY. The pump partitions each frame's draws into contiguous ranges; every range
// is dispatched to the shared guard-pool workers, whose stage-1 job is only to hash the
// range's draw identities; the order gate's "submitted" sequence is rebuilt from the
// concatenated ranges instead of copied from the intended log. Zero behaviour change —
// no draw, bind, upload or vkCmd moves — but every piece of machinery stage 2 needs
// (partitioning, dispatch, out-of-order completion, in-order concatenation, the drain)
// runs live and is adjudicated by the order gate every frame.
//
// STAGE 2 (not yet written): flip execution — workers record their ranges into
// secondary command buffers, the pump executes them in range order. The hazards named
// by the sibling's §6eb §3c live there: per-draw register capture, UploadStream
// re-entrancy against the shared stream store, and an arena discipline that must not
// change which buffer a stream lands in.
//
// Pump-thread-only entry points: OnDraw, RangeBreak, FrameSeal, BuildSubmitted,
// FrameReset. Worker-safe entry points: HasWork, RunOneJob. The host hooks exist so
// this file links against nothing in the renderer — the renderer registers them.

#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace prec
{

// The renderer's side of the shared-pool seam. `kick` wakes ONE guard-pool worker
// (they sleep on the pool's own condition variable, which this module must not know
// about — and one range job needs one worker, where the pool's own dispatch of a whole
// frame's guard list rightly wakes them all); `alive` says whether any worker thread
// exists yet — before the first guard dispatch none do, and this module runs its jobs
// inline on the pump instead.
struct Host
{
    void (*kick)() = nullptr;
    bool (*alive)() = nullptr;
};
void Init(const Host& h);

// Armed by default; `CW_VK_NO_PARALLEL_RECORD=1` is the same-binary control arm (the
// name is stage 2's, adopted now so the soak A/B that prices stage 1 is the same A/B
// that will price stage 2). Announces itself either way — an arm that is silent is an
// arm nobody can tell was on (gotcha 151).
bool On();

// The hot-path read of the same answer, resolved once in Init(). A function-local
// static here would put an init-guard load on the per-draw path — the exact shape
// part 76 had to take back off it (gotcha 453) — and Init() runs from BeginFrame,
// which every frame's first draw passes through before any call site below.
extern bool g_on;

// Why a range closed. `kSizeCap` is the balance knob (CW_VK_PREC_RANGE, default 128
// draws); the pass reasons exist because a secondary command buffer cannot span a
// rendering-instance boundary, so stage 2 inherits exactly these break points.
enum BreakReason : uint8_t
{
    kPassBegin = 0,
    kPassEnd,
    kSizeCap,
    kFrameSeal,
    kReasonCount
};

void RangeBreak(BreakReason r);

// One call per accepted draw. TWO MODES, chosen by the caller per draw and constant
// per run, because the first A/B refuted the "always log the identity" version — it
// cost 0.4 ms/frame at the crowd before distributing any work (perf-part7-notes §2):
//
// - OnDraw(id): the ORDER-GATE arm. The same identity the gate hashes — pipeline,
//   primitive, index range, shader pair — computed by the caller (only DoDraw has the
//   draw in hand). Ranges carry the full id sequence and every range close kicks a
//   worker, so the gate adjudicates the complete concurrent machinery.
// - OnDrawCount(): the DEFAULT arm. One increment; ranges carry a count, jobs fold the
//   count, no per-range wake (one kick at FrameSeal). The plumbing stays exercised
//   every frame at a cost the soak cannot see. Stage 2 note: real range jobs are tens
//   of microseconds, so per-range wakes stop being 67 futex calls a frame exactly when
//   the jobs become worth waking for — the count mode is stage 1's shape, not the
//   campaign's.
void OnDraw(uint64_t id);
void OnDrawCount();

// Close and dispatch the frame's final range, then drain: steal any un-started range
// job back onto the pump (a worker mid-guard-chew must not stall the frame boundary),
// then wait for the in-flight remainder, which is bounded by one job's length.
void FrameSeal();

// Rebuild the submitted order by concatenating the ranges in creation order, verifying
// each range's worker-computed hash against a pump-side recompute on the way (a
// disagreement means a worker raced or a slot was mixed up — counted and printed).
// Returns false when the module is off or the frame logged nothing, in which case the
// caller falls back to its own intended log. CW_VK_PREC_POISON=1 swaps the first two
// ranges here (the order gate MUST fail); =2 perturbs every worker hash at job time
// (the verify counter MUST fire). Both exist because a gate that has never been seen
// to fail has not been shown capable of it (gotcha 30).
bool BuildSubmitted(std::vector<uint64_t>& out);

// Forget the frame. Called after the order gate has consumed the ranges.
void FrameReset();

// The shared pool's side. HasWork is one atomic load — it runs inside every worker
// wake's predicate while the POOL's mutex is held, so it must take no lock and touch
// no init guard. RunOneJob claims and runs one range job, returning false when the
// queue is empty.
bool HasWork();
bool RunOneJob();

// One line on the stats dump, run totals. Prints only when armed; the control arm
// announced itself at init instead.
void PrintStats(FILE* f);

} // namespace prec
