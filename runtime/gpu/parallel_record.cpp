// parallel_record.cpp — stage 1 of the maximal parallel-record campaign (part 7).
// See the header for the campaign's shape and why this is a module. Everything here is
// bookkeeping and proof; nothing here touches the renderer, the driver, or guest
// memory, which is what "zero behaviour change" means and what the stage-1 soak A/B
// must confirm (prediction: null against CW_VK_NO_PARALLEL_RECORD=1).
//
// REVISED ONCE ALREADY, BY ITS OWN A/B. The first version logged the full draw
// identity and kicked all three workers on every range close in every run, and the
// pre-registered null FAILED: −3.6% (−0.41 ms) at the 6,500-7,000 band, dose-response
// with draw count, every heavy band the same way (perf-part7-notes §2). The costs were
// exactly the boring ones: ~67 notify_all a frame for nanosecond jobs, a mutex in the
// workers' wake predicate, a static-init guard on the per-draw path, and a per-draw
// hash nobody consumed with the gate off. Hence the two modes in the header, the
// empty→non-empty single-worker kick, and the lock-free HasWork.

#include "parallel_record.h"

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <deque>
#include <mutex>

namespace prec
{

bool g_on = false; // resolved by Init(); see the header for why not a local static

namespace
{

Host g_host;

// A contiguous run of draws. In gate mode `ids` is the identity sequence the order
// gate rebuilds from; in count mode the range is just `count` draws and `ids` stays
// empty. Ranges live in a deque so a worker holding a pointer survives the pump
// appending more ranges (deque growth never moves elements).
struct Range
{
    std::vector<uint64_t> ids;
    uint32_t count = 0;
    uint64_t workerHash = 0;
    uint8_t reason = kFrameSeal; // why the range CLOSED
    bool ranInline = false;      // no worker existed; the pump ran it at dispatch
    bool ranStolen = false;      // drained by the pump at FrameSeal, not by a worker
    bool done = false;
};

std::mutex g_mx;                 // guards g_ranges growth, g_queue, g_outstanding
std::condition_variable g_drainCv;
std::deque<Range> g_ranges;      // this frame's closed ranges, in creation order
std::deque<Range*> g_queue;      // dispatched, unclaimed jobs
std::atomic<size_t> g_queued{ 0 }; // mirror of g_queue.size() for the lock-free HasWork
size_t g_outstanding = 0;        // dispatched jobs not yet finished
std::vector<uint64_t> g_open;    // gate mode: the open range's ids (pump only)
uint32_t g_openCount = 0;        // count mode: the open range's draw count (pump only)
bool g_idsMode = false;          // set by the first OnDraw(id); constant per run

struct Stats
{
    uint64_t frames = 0;
    uint64_t draws = 0;
    uint64_t ranges = 0;
    uint64_t breaks[kReasonCount] = {};
    uint64_t jobsWorker = 0;
    uint64_t jobsInline = 0;
    uint64_t jobsStolen = 0;
    uint64_t kicks = 0;          // worker wakes this module asked for
    uint64_t drainBlocked = 0;   // FrameSeals that had to wait on an in-flight job
    uint64_t drainNs = 0;
    uint64_t hashMismatch = 0;   // worker hash != pump recompute — a real defect
    uint64_t rangeOverflow = 0;  // frames that hit the per-frame range cap
    uint64_t maxRangesFrame = 0;
    uint64_t maxDrawsFrame = 0;
};
Stats g_stats;

// The per-frame range cap is a backstop, not a policy: past it the size-cap break stops
// firing and the open range just grows, so a pathological frame degrades to "one big
// range" rather than to unbounded allocation. Pass breaks still apply (bounded by the
// frame's pass count).
constexpr size_t kMaxRangesPerFrame = 4096;

uint32_t RangeTarget()
{
    static const uint32_t n = [] {
        const char* e = getenv("CW_VK_PREC_RANGE");
        const long v = e ? atol(e) : 0;
        return (v > 0 && v < 1 << 20) ? uint32_t(v) : 128u;
    }();
    return n;
}

// 1 = swap the first two ranges in BuildSubmitted (the order gate must fail);
// 2 = perturb every worker hash (the verify counter must fire). Gotcha 30 twice over.
int PoisonMode()
{
    static const int m = [] {
        const char* e = getenv("CW_VK_PREC_POISON");
        const int v = e ? atoi(e) : 0;
        if (v)
            fprintf(stderr,
                    "[prec] CW_VK_PREC_POISON=%d — %s. THIS RUN MUST SCREAM; if it "
                    "does not, the check is blind\n",
                    v,
                    v == 1 ? "ranges 0 and 1 are transposed at concatenation, the "
                             "order gate must FAIL every multi-range frame"
                           : "every worker hash is perturbed, the range verify must "
                             "count a mismatch per range");
        return v;
    }();
    return m;
}

uint64_t FnvOver(const std::vector<uint64_t>& ids)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint64_t v : ids)
    {
        h ^= v;
        h *= 0x100000001B3ull;
    }
    return h;
}

// The stage-1 job body. Deliberately tiny: what stage 1 proves is the MACHINERY —
// dispatch, out-of-order completion, the drain — not the work. Stage 2 replaces this
// with recording the range into a secondary command buffer. In count mode there are no
// ids; folding the count keeps the job deterministic so the verify still means
// something.
void RunJob(Range& r)
{
    r.workerHash = r.ids.empty()
                       ? (0xCBF29CE484222325ull ^ r.count) * 0x100000001B3ull
                       : FnvOver(r.ids);
    if (PoisonMode() == 2)
        r.workerHash ^= 0x9E3779B97F4A7C15ull;
}

void FinishJob(Range& r, bool stolen)
{
    // The counters live under the lock because several workers finish concurrently;
    // an unlocked ++ here would be the module's only data race.
    std::lock_guard<std::mutex> lk(g_mx);
    r.done = true;
    if (stolen)
        ++g_stats.jobsStolen;
    else
        ++g_stats.jobsWorker;
    if (--g_outstanding == 0)
        g_drainCv.notify_all();
}

// Claim one queued job. `stolen` marks pump-side execution at the drain, which is
// counted separately from "no worker existed at dispatch" — the two are different
// statements about the pool (absent vs busy) and stage 2 sizes itself on the busy one.
bool RunOne(bool stolen)
{
    Range* r = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        if (g_queue.empty())
            return false;
        r = g_queue.front();
        g_queue.pop_front();
        g_queued.store(g_queue.size(), std::memory_order_release);
    }
    RunJob(*r);
    r->ranStolen = stolen;
    FinishJob(*r, stolen);
    return true;
}

// Close the open range (if it holds anything) into g_ranges and hand it to the pool.
//
// THE WAKE POLICY IS THE FIRST A/B's LESSON. Gate mode kicks ONE worker, and only on
// the queue's empty→non-empty transition (a draining worker empties the whole queue
// before sleeping, so a push onto a non-empty queue is already spoken for). Count mode
// — every default run — kicks nobody here at all: the jobs are nanoseconds, the only
// consumer is the frame boundary, and FrameSeal's single kick plus the steal-back
// finishes them there. The kick happens OUTSIDE this module's lock: the workers' wait
// predicate calls HasWork() while holding the POOL's mutex, so kicking while holding
// ours would be the classic two-lock inversion.
void CloseOpenRange(BreakReason r)
{
    ++g_stats.breaks[r];
    if (g_open.empty() && !g_openCount)
        return;
    Range* range = nullptr;
    bool inlineRun = false;
    bool kick = false;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_ranges.emplace_back();
        range = &g_ranges.back();
        range->ids.swap(g_open);
        range->count = g_openCount ? g_openCount : uint32_t(range->ids.size());
        range->reason = uint8_t(r);
        ++g_stats.ranges;
        if (g_host.alive && g_host.alive())
        {
            kick = g_idsMode && g_queue.empty();
            g_queue.push_back(range);
            g_queued.store(g_queue.size(), std::memory_order_release);
            ++g_outstanding;
        }
        else
            inlineRun = true;
    }
    g_openCount = 0;
    if (g_idsMode)
        g_open.reserve(RangeTarget());
    if (inlineRun)
    {
        // No worker thread exists yet (the guard pool starts on its first dispatch).
        // Correctness never depends on the pool — the pump just does the job now.
        RunJob(*range);
        range->ranInline = true;
        range->done = true;
        ++g_stats.jobsInline;
    }
    else if (kick && g_host.kick)
    {
        ++g_stats.kicks;
        g_host.kick();
    }
    // Stage 2's rotation point, after all of this module's own work and outside its
    // lock. Fires only when a range actually closed — the empty-break early return
    // above never reaches here.
    if (g_host.rangeClosed)
        g_host.rangeClosed(uint8_t(r));
}

} // namespace

void Init(const Host& h)
{
    g_host = h;
    g_on = On();
}

bool On()
{
    static const bool on = [] {
        const bool off = getenv("CW_VK_NO_PARALLEL_RECORD") != nullptr;
        if (off)
            fprintf(stderr, "[prec] CW_VK_NO_PARALLEL_RECORD=1 — the parallel-record "
                            "skeleton is OFF (control arm)\n");
        else
            fprintf(stderr,
                    "[prec] parallel-record stage 1 ARMED (bookkeeping only: ranges of "
                    "~%u draws, jobs on the shared guard pool, execution still serial; "
                    "CW_VK_NO_PARALLEL_RECORD=1 is the control)\n",
                    RangeTarget());
        return !off;
    }();
    return on;
}

void RangeBreak(BreakReason r)
{
    if (!g_on)
        return;
    CloseOpenRange(r);
}

void OnDraw(uint64_t id)
{
    if (!g_on)
        return;
    g_idsMode = true;
    g_open.push_back(id);
    ++g_stats.draws;
    if (g_open.size() >= RangeTarget())
    {
        // The backstop reads g_ranges.size() without the lock, which is safe: only the
        // pump appends to it, so the pump's own read cannot tear.
        if (g_ranges.size() < kMaxRangesPerFrame)
            CloseOpenRange(kSizeCap);
        else if (g_open.size() == RangeTarget())
            ++g_stats.rangeOverflow; // once per overflowing range, not per draw
    }
}

void OnDrawCount()
{
    if (!g_on)
        return;
    ++g_openCount;
    ++g_stats.draws;
    if (g_openCount >= RangeTarget())
    {
        if (g_ranges.size() < kMaxRangesPerFrame)
            CloseOpenRange(kSizeCap);
        else if (g_openCount == RangeTarget())
            ++g_stats.rangeOverflow;
    }
}

void FrameSeal()
{
    if (!g_on)
        return;
    CloseOpenRange(kFrameSeal);
    // Count mode never kicked mid-frame; one wake here lets a worker share the drain
    // while the pump steals from the other end.
    if (!g_idsMode && g_queued.load(std::memory_order_acquire) && g_host.kick)
    {
        ++g_stats.kicks;
        g_host.kick();
    }
    // Steal-back: any range the workers have not CLAIMED yet is run here, so the wait
    // below is bounded by one in-flight job rather than by a worker's whole guard
    // chew. This is the same design decision GuardPoolDrain documents — a stall the
    // pump pays must be visible — applied before the stall instead of after.
    while (RunOne(/*stolen=*/true))
    {
    }
    std::unique_lock<std::mutex> lk(g_mx);
    if (g_outstanding)
    {
        ++g_stats.drainBlocked;
        const auto t0 = std::chrono::steady_clock::now();
        g_drainCv.wait(lk, [] { return g_outstanding == 0; });
        g_stats.drainNs += uint64_t(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - t0)
                .count());
    }
}

bool BuildSubmitted(std::vector<uint64_t>& out)
{
    if (!g_on || !g_idsMode || g_ranges.empty())
        return false;
    // Post-drain, so every range is done and no worker holds a pointer; the pump is the
    // only thread here.
    static uint64_t mismatchPrinted = 0;
    size_t first = 0, second = 0, withDraws = 0;
    for (size_t i = 0; i < g_ranges.size(); ++i)
        if (!g_ranges[i].ids.empty() && ++withDraws <= 2)
            (withDraws == 1 ? first : second) = i;
    for (size_t i = 0; i < g_ranges.size(); ++i)
    {
        // The poison transposes the first two non-empty ranges at CONCATENATION — the
        // exact defect a parallel submitter would commit — and must fail the gate.
        size_t pick = i;
        if (PoisonMode() == 1 && withDraws >= 2)
            pick = (i == first) ? second : (i == second) ? first : i;
        const Range& r = g_ranges[pick];
        if (r.ids.empty())
            continue;
        if (r.workerHash != FnvOver(r.ids))
        {
            ++g_stats.hashMismatch;
            if (++mismatchPrinted <= 8)
                fprintf(stderr,
                        "[prec] ** RANGE HASH MISMATCH #%llu: range %zu (%zu draws, "
                        "%s) — the worker's hash disagrees with the pump's recompute\n",
                        (unsigned long long)g_stats.hashMismatch, pick, r.ids.size(),
                        r.ranInline ? "inline" : r.ranStolen ? "stolen" : "worker");
        }
        out.insert(out.end(), r.ids.begin(), r.ids.end());
    }
    return true;
}

void FrameReset()
{
    if (!g_on)
        return;
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_ranges.size() > g_stats.maxRangesFrame)
        g_stats.maxRangesFrame = g_ranges.size();
    uint64_t d = 0;
    for (const Range& r : g_ranges)
        d += r.count;
    if (d > g_stats.maxDrawsFrame)
        g_stats.maxDrawsFrame = d;
    if (!g_ranges.empty())
        ++g_stats.frames;
    g_ranges.clear();
    g_queue.clear(); // provably empty post-drain; cleared anyway so a defect cannot
                     // leave a dangling pointer to a range the line above just freed
    g_queued.store(0, std::memory_order_release);
    g_outstanding = 0;
}

bool HasWork()
{
    // One atomic load, no lock, no init guard: this runs in the workers' wake
    // predicate under the POOL's mutex. When the module is off the counter is
    // simply never non-zero.
    return g_queued.load(std::memory_order_acquire) != 0;
}

bool RunOneJob()
{
    if (g_queued.load(std::memory_order_acquire) == 0)
        return false;
    return RunOne(/*stolen=*/false);
}

void PrintStats(FILE* f)
{
    if (!g_on || !g_stats.frames)
        return;
    const Stats& s = g_stats;
    fprintf(f,
            "[prec]   stage 1 (%s mode): %llu frames, %llu draws in %llu ranges "
            "(%.1f ranges/frame, %.1f draws/range, peak %llu ranges / %llu draws) | "
            "breaks pass %llu/%llu size %llu seal %llu | jobs: worker %llu inline "
            "%llu stolen %llu, %llu kicks | drain blocked %llu (%.2f ms total) | "
            "**%llu hash mismatches**%s%s\n",
            g_idsMode ? "id" : "count",
            (unsigned long long)s.frames, (unsigned long long)s.draws,
            (unsigned long long)s.ranges,
            double(s.ranges) / double(s.frames),
            s.ranges ? double(s.draws) / double(s.ranges) : 0.0,
            (unsigned long long)s.maxRangesFrame, (unsigned long long)s.maxDrawsFrame,
            (unsigned long long)s.breaks[kPassBegin],
            (unsigned long long)s.breaks[kPassEnd],
            (unsigned long long)s.breaks[kSizeCap],
            (unsigned long long)s.breaks[kFrameSeal],
            (unsigned long long)s.jobsWorker, (unsigned long long)s.jobsInline,
            (unsigned long long)s.jobsStolen, (unsigned long long)s.kicks,
            (unsigned long long)s.drainBlocked,
            double(s.drainNs) / 1e6, (unsigned long long)s.hashMismatch,
            s.rangeOverflow ? "  (RANGE CAP HIT)" : "",
            PoisonMode() ? "  (POISONED — a clean line here means a check is BLIND)"
                         : "");
}

} // namespace prec
