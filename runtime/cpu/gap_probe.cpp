// Do the recompiler's UNIMPLEMENTED INSTRUCTIONS actually execute? — W0.3's first question
//
// WHY THIS EXISTS
// ---------------
// XenonRecomp cannot translate six mnemonics in this image and one pack form, and it
// says so at recompile time and then carries on:
//
//     39 x "Unrecognized instruction at <addr>: vminsw|vpkshss|vavgsw|stdux|
//                                              vpkshss128|stvebx"
//     20 x "Unexpected float16_4 pack instruction at <addr>"
//
// An unimplemented instruction is NOT a build failure and NOT a crash. The emitted
// function is missing that instruction's effect and runs anyway, so the failure mode is
// a silently wrong register or a silently wrong store — the worst class this project
// deals with, because nothing reports it (docs/port-plan.md W0.3).
//
// 59 sites sounds like 59 problems. It is not: they land in exactly SEVEN functions, so
// the real question is whether those seven ever run. A function that never executes
// carries no risk at all and implementing its instructions is wasted work on a SHARED
// tool — XenonRecomp is used by all four ports in this workspace, so a change there is
// not ours to make unilaterally.
//
// WHY THIS IS A RUNTIME INSTRUMENT AND NOT A TRACE QUERY
// ------------------------------------------------------
// The obvious way to answer it is the Xenia coverage traces, and that answer is WRONG.
// Asked of C1 and C2, all seven functions come back "never executed" — and so does
// `sub_825D7D20`, the XGI user-context builder that `kernel/imports.cpp` derived its
// struct layout from by reading that very function, and which our own runtime calls at
// boot every time. The traces have a hole: **zero executed functions between 0x825D1028
// and 0x825F0000**, a ~124 KB stretch that contains all seven of these plus the XAM
// wrapper code that demonstrably runs (finding 31).
//
// So the trace cannot answer this and neither can any static reasoning about it. The
// positive control is what caught that, and it is the only reason this file exists
// instead of a one-line "they never run" in a document — which would have been this
// port's characteristic error for the sixth time (gotcha 3: a zero is a detection
// failure, not a fact).
//
// THIS COUNTER HAS BEEN SHOWN CAPABLE OF COUNTING (gotcha 30). Adding sub_825B5B90 —
// the vblank callback — to the table as an eighth row made it report 11,267 calls on a
// 70 s boot run, while all seven real rows stayed 0. So a zero here is the absence of
// the call, not the absence of an instrument. The control row was then removed.
//
// WHAT IT COSTS
// -------------
// One relaxed atomic increment per call to seven functions, against a gameplay session
// that issues 26 M draws. It is always on because a counter you have to remember to
// enable is a counter that is off on the run that mattered (gotcha 151) — and because
// the honest form of "does this execute" is a number from the run you actually did.
//
// READING THE RESULT
// ------------------
// A nonzero count means that function's unimplemented instructions ARE executing and
// their absence is corrupting something. It does NOT say what: the next step for a hot
// row is to disassemble the site and work out what the missing instruction contributes.
// A zero count means only that THIS run did not reach it — an absence again, with all
// the usual caveats, so the report prints what the run was rather than claiming
// coverage.

#include <atomic>
#include <cstdint>
#include <cstdio>

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>

#include "gap_probe.h"

namespace
{

// address, how many unimplemented sites it contains, which mnemonics, and what the
// function looks like from a first disassembly. The shapes are descriptive, not
// identifications — nothing here has been traced to a caller yet.
#define CW_GAP_FUNCS(X)                                                                  \
    X(825D75B0, 28, "vminsw/vpkshss/vpkshss128", "saturating vector pack, 28 sites")     \
    X(825D9B50,  1, "stvebx",                    "vector byte store")                    \
    X(825E0290,  4, "vavgsw",                    "vperm tables at 0x82095120")            \
    X(825E05D0,  4, "vavgsw",                    "sibling of 825E0290")                  \
    X(825E6808, 20, "float16_4 pack",            "converter, gates on count==4 type==2") \
    X(825EB5C0,  1, "stdux",                     "doubleword store with update")         \
    X(825ED6C0,  1, "stdux",                     "doubleword store with update")

#define X(addr, sites, mnem, shape) std::atomic<uint64_t> g_hits_##addr{ 0 };
CW_GAP_FUNCS(X)
#undef X

} // namespace

// The recompiled bodies. ppc_recomp_shared.h declares only the weak `sub_X` alias, so a
// strong PPC_FUNC(sub_X) below takes over every call site in the image and forwards.
#define X(addr, sites, mnem, shape) extern "C" PPC_FUNC(__imp__sub_##addr);
CW_GAP_FUNCS(X)
#undef X

#define X(addr, sites, mnem, shape)                                                      \
    PPC_FUNC(sub_##addr)                                                                 \
    {                                                                                    \
        g_hits_##addr.fetch_add(1, std::memory_order_relaxed);                           \
        __imp__sub_##addr(ctx, base);                                                    \
    }
CW_GAP_FUNCS(X)
#undef X

void GapProbe_Report()
{
    uint64_t total = 0;
#define X(addr, sites, mnem, shape) total += g_hits_##addr.load();
    CW_GAP_FUNCS(X)
#undef X

    std::fprintf(stderr,
                 "\n[gap] functions containing instructions XenonRecomp could not "
                 "translate — did they run?\n");
    std::fprintf(stderr, "[gap]   %-14s %6s  %-26s %s\n", "function", "calls", "mnemonics",
                 "sites");
#define X(addr, sites, mnem, shape)                                                      \
    std::fprintf(stderr, "[gap]   sub_%-10s %6llu  %-26s %2d   %s\n", #addr,             \
                 (unsigned long long)g_hits_##addr.load(), mnem, sites, shape);
    CW_GAP_FUNCS(X)
#undef X

    if (total == 0)
    {
        std::fprintf(stderr,
                     "[gap]   TOTAL 0 — none of the seven ran IN THIS RUN. That is an "
                     "absence, not a\n"
                     "[gap]   clearance: it says what this drive reached, nothing more. "
                     "Re-read it after a\n"
                     "[gap]   drive that goes somewhere new before treating W0.3 as "
                     "dead work.\n");
    }
    else
    {
        std::fprintf(stderr,
                     "[gap]   TOTAL %llu calls — these functions ARE executing with "
                     "instructions missing.\n"
                     "[gap]   Their effects are silently absent. W0.3 is live; the hot "
                     "rows above are the\n"
                     "[gap]   ones to disassemble first. XenonRecomp is SHARED with the "
                     "other three ports,\n"
                     "[gap]   so changing it is the operator's call, not a unilateral "
                     "one.\n",
                     (unsigned long long)total);
    }
}
