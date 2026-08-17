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

#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

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

// THE WIDGET FACTORY'S LOOKUP, which is where a missing class becomes silence.
//
// sub_82784588 is "create a widget of class <name>". It calls sub_82784508 to turn the
// name into a table index and, if that comes back -1, RETURNS 0 WITHOUT A WORD:
//
//     bl   0x82784508            ; index = FindClass(name)
//     cmpwi r3, -1
//     beq  -> li r3,0 ; return   ; <- no class, no widget, no complaint
//     lwz  r11, 0x20(r31)        ; the table
//     mulli r10, r3, 0xc         ; index * 12
//     lwzx r11, r10, r11         ; creator = table[index].creator
//     mtctr r11 ; bctrl          ; call it
//
// and sub_82784508 matches by INTERNED ID: it interns the requested name through
// sub_827815D0 and compares that against each entry's stored id, which was interned the
// same way at registration. So a failure here is an id that does not match, and the
// name it was asked for is the single most useful thing to know.
//
// This counts every lookup, counts the failures, and records the NAMES that failed —
// bounded, deduplicated, and read straight out of guest memory, because a count alone
// would say "something was not found" and leave the interesting half out.
namespace
{
std::atomic<uint64_t> g_lookups{ 0 };
std::atomic<uint64_t> g_lookupFails{ 0 };
std::mutex g_missMutex;
std::vector<std::string> g_missed;   // distinct class names that resolved to -1
// Every `cFE*` name the parser ASKS the factory for, with a count — the other half of the
// question. A class that is never requested is a screen-data problem; one that is
// requested and unresolved is a factory problem. Without both, a zero is ambiguous.
std::vector<std::pair<std::string, uint64_t>> g_asked;
// CreateWidget(name) itself — unambiguous, unlike the generic lookup above. Records the
// class name and whether the call returned a widget or 0, because "asked" via
// sub_82784508 conflates creation with any other name->index question the engine has.
std::vector<std::array<uint64_t, 2>> g_createCounts;   // {made, failed} per name
std::vector<std::string> g_createNames;
} // namespace

extern "C" PPC_FUNC(__imp__sub_82784508);
PPC_FUNC(sub_82784508)
{
    // r4 is the name pointer on entry; the callee is free to clobber it, so take it now.
    const uint32_t nameVa = ctx.r4.u32;
    if (nameVa)
    {
        const char* q = reinterpret_cast<const char*>(base + nameVa);
        if (q[0] == 'c' && q[1] == 'F' && q[2] == 'E')
        {
            char nb[64];
            size_t m = 0;
            while (m < sizeof nb - 1 && q[m] > 0x20 && q[m] < 0x7F) { nb[m] = q[m]; ++m; }
            nb[m] = 0;
            std::lock_guard<std::mutex> lock(g_missMutex);
            bool seen = false;
            for (auto& e : g_asked)
                if (e.first == nb) { ++e.second; seen = true; break; }
            if (!seen && g_asked.size() < 64)
                g_asked.emplace_back(nb, 1);
        }
    }
    __imp__sub_82784508(ctx, base);
    g_lookups.fetch_add(1, std::memory_order_relaxed);
    if (ctx.r3.s32 != -1)
        return;
    g_lookupFails.fetch_add(1, std::memory_order_relaxed);

    // Guest strings are plain bytes, so no swap; bound the read so a wild pointer cannot
    // walk off the map, and keep the distinct list small — this is a diagnosis, not a log.
    if (!nameVa)
        return;
    const char* p = reinterpret_cast<const char*>(base + nameVa);
    char buf[64];
    size_t n = 0;
    while (n < sizeof buf - 1 && p[n] >= 0x20 && p[n] < 0x7F)
    {
        buf[n] = p[n];
        ++n;
    }
    buf[n] = 0;
    if (!n)
        return;
    // sub_82784508 is a GENERIC name->index lookup: the screen-definition parser calls it
    // for every token it meets, including property lines like `X=0.18906` and
    // `Font="arialblk18"`, so most failures are the parser falling through to its property
    // handling and are entirely normal. A widget TYPE name is a bare token, so anything
    // carrying '=' or a quote is dropped here — otherwise the bounded list fills with
    // property text and the one interesting name never reaches it.
    if (std::strchr(buf, '=') || std::strchr(buf, '"') || std::strchr(buf, ' '))
        return;
    std::lock_guard<std::mutex> lock(g_missMutex);
    if (g_missed.size() >= 64)
        return;
    for (const auto& s : g_missed)
        if (s == buf)
            return;
    g_missed.emplace_back(buf);
}

extern "C" PPC_FUNC(__imp__sub_82784588);
PPC_FUNC(sub_82784588)
{
    const uint32_t nameVa = ctx.r4.u32;
    char nb[64];
    size_t m = 0;
    if (nameVa)
    {
        const char* q = reinterpret_cast<const char*>(base + nameVa);
        while (m < sizeof nb - 1 && q[m] > 0x20 && q[m] < 0x7F) { nb[m] = q[m]; ++m; }
    }
    nb[m] = 0;
    __imp__sub_82784588(ctx, base);
    const bool made = ctx.r3.u32 != 0;
    if (!m)
        return;
    std::lock_guard<std::mutex> lock(g_missMutex);
    for (size_t i = 0; i < g_createNames.size(); i++)
        if (g_createNames[i] == nb) { g_createCounts[i][made ? 0 : 1]++; return; }
    if (g_createNames.size() >= 64)
        return;
    g_createNames.emplace_back(nb);
    g_createCounts.push_back({ made ? 1u : 0u, made ? 0u : 1u });
}

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

    const uint64_t look = g_lookups.load(), fail = g_lookupFails.load();
    std::fprintf(stderr,
                 "[fe]   factory lookups: %llu, of which %llu FAILED (returned -1 and the\n"
                 "[fe]   widget was silently not created)\n",
                 (unsigned long long)look, (unsigned long long)fail);
    std::lock_guard<std::mutex> lock(g_missMutex);
    std::fprintf(stderr, "[fe]   cFE* class names the parser ASKED the factory for:\n");
    if (g_asked.empty())
        std::fprintf(stderr, "[fe]     (none — the screen data never names a widget class)\n");
    else
        for (const auto& e : g_asked)
            std::fprintf(stderr, "[fe]     asked %6llu x  %s\n",
                         (unsigned long long)e.second, e.first.c_str());
    std::fprintf(stderr, "[fe]   CreateWidget(name) — made / FAILED:\n");
    if (g_createNames.empty())
        std::fprintf(stderr, "[fe]     (CreateWidget was never called)\n");
    else
        for (size_t i = 0; i < g_createNames.size(); i++)
            std::fprintf(stderr, "[fe]     %-24s made %6llu   FAILED %6llu%s\n",
                         g_createNames[i].c_str(),
                         (unsigned long long)g_createCounts[i][0],
                         (unsigned long long)g_createCounts[i][1],
                         g_createCounts[i][1] ? "   <== never created" : "");
    if (g_missed.empty())
        std::fprintf(stderr, "[fe]   no class name failed to resolve.\n");
    else
        for (const auto& s : g_missed)
            std::fprintf(stderr, "[fe]     UNRESOLVED CLASS: %s\n", s.c_str());
}
