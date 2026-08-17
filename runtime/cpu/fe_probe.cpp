// Does the frontend's METER widget ever get built, and does anything set its value?
//
// WHY THIS EXISTS
// ---------------
// Findings 35-39 cornered the progress-widget defect (PP bar, mission timer bar, LIFE's
// empty squares, the loading pop-up's segmented bar) to one place and then ran out of
// GPU to blame:
//
//   * one vertex shader draws all of them — vs_a4ae7c2b7c1818c4 (finding 36);
//   * it is in the bank, is real 15-dword microcode, and our runtime LOADS it (38);
//   * we predicate out 0.29% of draws against hardware's 0.3%, so we are not
//     discarding them (39);
//   * and across three captured frames with the HUD raised, ZERO of our draws use it,
//     while hardware's equivalent frame uses it 55 times.
//
// So the guest sets the shader up and never issues a draw with it. That puts the defect
// upstream of the GPU, in the recompiled guest code, and no renderer instrument can see
// any further. This is the first probe on the other side of that line.
//
// HOW THE ADDRESSES WERE FOUND, so they can be re-derived rather than trusted
// --------------------------------------------------------------------------
// The frontend registers its widget classes by name. `cFEMeter`'s string is at
// 0x820BEEF0 and has exactly ONE referencing site, 0x829BC640, which is one entry in a
// run of {creator, id, name} triples written into a factory table:
//
//     addi r11, r11, -0x69d0     ; r11 = 0x82819630   <- creator
//     addi r31, r10, -0x1110     ; r31 = 0x820BEEF0   <- "cFEMeter"
//     bl   0x827815d0            ; intern the name
//     stw  r3,  0xc4(r30)        ; id
//     stw  r31, 0xc8(r30)        ; name
//     stw  r11, 0xc0(r30)        ; creator
//
// 0x82819630 is a thunk that tail-calls 0x82817488, which allocates 0x8920 bytes and
// calls the constructor at 0x8280F468.
//
// AN OFF-BY-ONE HERE COST THE FIRST VERSION OF THIS FILE ITS ANSWER, and it is worth
// the warning. The creator is stored into the table BEFORE its class name is loaded, so
// reading the `stw` that follows the `bl` attributes the NEXT class's creator to this
// one. The first cut of this probe hooked 0x82819640 / 0x8280D300 as "cFEMeter" and got
// a confident "no meter is ever built" — from cFEParticleFX's constructor. What caught
// it was extracting the WHOLE table programmatically instead of reading one entry by
// eye: 22 classes came out in order, and cFEMeter's creator was one slot back.
//
// So the addresses below were derived from the full table, not from a single site, and
// the correct chain is
//     cFEMeter      creator 0x82817488 -> ctor 0x8280F468   (alloc 0x8920)
//     cFEText       creator 0x82815088 -> ctor 0x8280C028   (alloc 0x400)
//     cFEFlipBook   creator 0x828195A0
//
// WHAT THIS PROBE DECIDES, and it is a fact either way
// ----------------------------------------------------
// Three questions, in the order they stop being worth asking:
//
//   1. ctor count == 0    -> no meter is ever BUILT. The defect is far upstream, in
//                            screen construction or the factory, and nothing about
//                            drawing matters yet.
//   2. ctor > 0, set == 0 -> meters exist but nothing ever gives them a value. A widget
//                            asked to draw an unset meter plausibly draws nothing.
//   3. both > 0           -> the meter is built and driven, and the defect is in the
//                            inherited draw path — which is then the thing to hook next,
//                            with the vtable above as the map.
//
// The counters are always on, one relaxed atomic each, reported from both shutdown
// paths. Same reasoning as cpu/gap_probe.cpp: a counter you have to remember to enable
// is a counter that was off on the run that mattered (gotcha 151).
//
// THIS PROBE HAS NOT YET BEEN SHOWN CAPABLE OF COUNTING on these addresses. gap_probe's
// identical mechanism was (an eighth control row reported 11,267 calls), but that is
// evidence about the MECHANISM, not about these six hooks resolving to the right
// bodies — so a zero here is only trustworthy once at least one row in this file is
// nonzero. `cFEWidget name intern` is here to be that control: it is the factory call
// every widget class registers through, so it MUST be nonzero on any run that reaches
// the frontend at all.

#include <atomic>
#include <cstdint>
#include <cstdio>

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>

#include "fe_probe.h"

namespace
{

#define CW_FE_FUNCS(X)                                                                   \
    X(827815D0, "CONTROL A: widget-name intern (every class registers here)")            \
    X(828194B0, "cFEText creator")                                                      \
    X(828194C0, "cFEEBMText creator")                                                   \
    X(828194D0, "cFEBitmap creator")                                                    \
    X(828194E0, "cFEShape creator")                                                     \
    X(828194F0, "cFEAnim creator")                                                      \
    X(82819500, "cFESpinGroup creator")                                                 \
    X(82819540, "cFETextList creator")                                                  \
    X(82819550, "cFETextBox creator")                                                   \
    X(82819560, "cFEBitmapList creator")                                                \
    X(828195A0, "cFEFlipBook creator")                                                  \
    X(828195B0, "cFEFlipFrame creator")                                                 \
    X(828195F0, "cFETable creator")                                                     \
    X(82819630, "cFEMeter creator")                                                     \
    X(82819640, "cFEParticleFX creator")                                                \
    X(82819650, "cFEEdit creator")                                                      \
    X(8281B698, "cFENineGrid creator")                                                  \
    X(82813068, "cFEThreeGrid creator")                                                 \
    X(82813078, "cFEGenericTable creator")                                              \
    X(82819690, "cFEMovieBox creator")                                                  \
    X(82813088, "cFELockBox creator")                                                   \
    X(82813098, "cFEKeyFrame creator")                                                  

#define X(addr, what) std::atomic<uint64_t> g_fe_##addr{ 0 };
CW_FE_FUNCS(X)
#undef X

} // namespace

#define X(addr, what) extern "C" PPC_FUNC(__imp__sub_##addr);
CW_FE_FUNCS(X)
#undef X

#define X(addr, what)                                                                    \
    PPC_FUNC(sub_##addr)                                                                 \
    {                                                                                    \
        g_fe_##addr.fetch_add(1, std::memory_order_relaxed);                             \
        __imp__sub_##addr(ctx, base);                                                    \
    }
CW_FE_FUNCS(X)
#undef X

void FeProbe_Report()
{
    std::fprintf(stderr, "\n[fe] frontend METER widget probe — findings 35-39\n");
#define X(addr, what)                                                                    \
    std::fprintf(stderr, "[fe]   sub_%-10s %10llu   %s\n", #addr,                        \
                 (unsigned long long)g_fe_##addr.load(), what);
    CW_FE_FUNCS(X)
#undef X

    const uint64_t control = g_fe_827815D0.load();
    if (control == 0)
        std::fprintf(stderr,
                     "[fe]   CONTROL A IS ZERO -- this run never reached the frontend's\n"
                     "[fe]   class registration, so every zero below is uninterpretable.\n");
    else
        std::fprintf(stderr,
                     "[fe]   A class with a NONZERO count is constructed by this drive; a\n"
                     "[fe]   zero is only meaningful beside a nonzero sibling on the SAME\n"
                     "[fe]   screen. See findings 35-40.\n");
}
