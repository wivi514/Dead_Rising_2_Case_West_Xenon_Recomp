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

// The ctors and the 0x1CC accessors that used to sit in this list moved into the
// three-class vtable census below, where their counts come back CLASS-FILTERED.
#define CW_FE_FUNCS(X)                                                                   \
    X(827815D0, "CONTROL A: name intern (everything registers through here)")            \
    X(82815B80, "cFEMeter  creator [TABLE ENTRY 16] alloc 0x2F0")                        \
    X(82814A50, "cFEBitmap creator [TABLE ENTRY  6] alloc 0xF0")                         \
    X(828168D0, "cFEText   creator [TABLE ENTRY  4] alloc 0x2C0")

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

// ============================================================================
// THE THREE-CLASS VTABLE CENSUS — the measurement part 4 exists to make.
//
// Part 3 ended with cFEMeter's whole lifecycle healthy (created 70, linked,
// walked, updated 100% class-specifically, laid out ~1.5x each — finding 47)
// and still not one draw with the shader that paints meters. Guessing which
// vtable slot is "the draw" failed four times, so the question is converted
// into a comparison: WHICH SLOT DO WIDGETS THAT DRAW RECEIVE THAT METERS DO
// NOT? cFEBitmap (1000 built last session) and cFEText (247) both render
// correctly, so their censuses beside the meter's is the diff that names the
// missing call — or, if every slot matches, proves the draw is not a widget
// virtual at all, which is also an answer.
//
// PROVENANCE of the two new chains — factory table entries [6] and [4], READ
// OUT OF GUEST MEMORY on the 2026-08-16 run (the meter's off-by-one, gotcha
// 322, is why nothing here comes from reading registration code by eye), then
// thunk -> allocator -> ctor -> the vtable store in each ctor's body:
//   cFEBitmap [ 6] creator 0x828194E0 -> 0x82814A50 (alloc 0xF0)  -> ctor 0x8280B990
//                  ctor stores vtable 0x820BD8D8 at [this+0]
//   cFEText   [ 4] creator 0x828194C0 -> 0x828168D0 (alloc 0x2C0) -> ctor 0x8280DCD8
//                  ctor stores vtable 0x820BDE50 at [this+0]
// The vtable CONTENTS are a static read of default_image.bin at those
// addresses — the one inference left in this chain. Two things check it: the
// meter row of the same read reproduced part 3's runtime census exactly (13
// active slots, same functions), and the ctor hooks below read [this+0] AFTER
// construction at runtime and the report compares that against the constant —
// a mismatch voids the census and says so in the output rather than
// zero-filling it (gotcha 30: an instrument must be able to fail loudly).
//
// KNOWN BLIND SPOT, unchanged from part 3: a slot invoked on an adjusted
// sub-object pointer carries a `this` whose [0] is not the class's primary
// vtable, fails the filter, and reads 0. A zero here is "not observed on the
// primary this", not "never called".
namespace
{

constexpr uint32_t kMeterVtable  = 0x820BDBE8;
constexpr uint32_t kBitmapVtable = 0x820BD8D8;
constexpr uint32_t kTextVtable   = 0x820BDE50;
constexpr uint32_t kVtOf[3]      = { kMeterVtable, kBitmapVtable, kTextVtable };
const char* const  kVtClassName[3] = { "cFEMeter", "cFEBitmap", "cFEText" };
constexpr int      kVtSlotCount  = 40;

// Slots 0..39 of each vtable, from default_image.bin. Beyond slot 39 the
// meter's table holds a zero word, so 40 is the base class's method count.
constexpr uint32_t kVtSlots[3][kVtSlotCount] = {
    { // cFEMeter 0x820BDBE8
      0x82815BC8, 0x82816368, 0x82806000, 0x82463648, 0x8281C0A8, 0x8280FD98,
      0x8281A5E0, 0x82463648, 0x82815C18, 0x82804808, 0x82810160, 0x82466CB8,
      0x8280FEB0, 0x82805D70, 0x82804840, 0x8280D448, 0x828023B8, 0x82802408,
      0x828107D8, 0x82810628, 0x82810698, 0x82810708, 0x828064F8, 0x82810210,
      0x828048C0, 0x82466CB8, 0x828109D8, 0x82810A60, 0x82466CB8, 0x82810AF0,
      0x8280D438, 0x8280D440, 0x821C3380, 0x82809A70, 0x82809AB0, 0x8280D458,
      0x82332528, 0x821C3380, 0x828162C8, 0x82815A78 },
    { // cFEBitmap 0x820BD8D8
      0x82814A98, 0x82814AE8, 0x82806000, 0x82463648, 0x8281BB70, 0x8280FD98,
      0x821C3380, 0x82466CB8, 0x8280FF30, 0x82803678, 0x82810160, 0x82466CB8,
      0x8280FEB0, 0x82805D70, 0x828036F0, 0x8280BA10, 0x8280BBA0, 0x8280BB08,
      0x828107D8, 0x82810628, 0x82810698, 0x82810708, 0x828064F8, 0x82810210,
      0x82810960, 0x82466CB8, 0x828109D8, 0x82810A60, 0x82466CB8, 0x82810AF0,
      0x821C3380, 0x82466CB8, 0x821C3380, 0x82809A70, 0x82809AB0, 0x8280BD38,
      0x82803ED0, 0x82803928, 0x8280BEB8, 0x8281B8E0 },
    { // cFEText 0x820BDE50
      0x82816920, 0x8280E308, 0x82806000, 0x82463648, 0x8281C370, 0x8280FD98,
      0x821C3380, 0x82466CB8, 0x8280FF30, 0x82816970, 0x82810160, 0x82466CB8,
      0x8280FEB0, 0x82805D70, 0x82804D98, 0x8280DDF8, 0x82807EA8, 0x82807E48,
      0x828107D8, 0x82810628, 0x82810698, 0x82810708, 0x828064F8, 0x82816BB0,
      0x82810960, 0x82466CB8, 0x828109D8, 0x82810A60, 0x82466CB8, 0x82810AF0,
      0x821C3380, 0x82466CB8, 0x8280E978, 0x82809A70, 0x82809AB0, 0x82805C38,
      0x8280DE00, 0x82816AA8, 0x82804DB0, 0x8280BA10 },
};

inline uint32_t GuestU32(uint8_t* b, uint32_t va)
{
    const uint8_t* p = b + va;
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

// -1 = not one of the three (or not a plausible pointer). The filter is the
// object's own vtable pointer, written by its constructor — the one label the
// guest itself provides (the rule findings 43/44/46 bought).
inline int VtClass(uint8_t* b, uint32_t self)
{
    if (self < 0x1000)
        return -1;
    const uint32_t vt = GuestU32(b, self);
    for (int c = 0; c < 3; c++)
        if (vt == kVtOf[c])
            return c;
    return -1;
}

inline bool IsMeter(uint8_t* b, uint32_t self)
{
    return self >= 0x1000 && GuestU32(b, self) == kMeterVtable;
}

} // namespace

// One counter TRIPLE per distinct function in the union of the three vtables
// (65 of them). Counts are per FUNCTION: where the same function occupies
// several slots of one vtable (the shared no-op stubs do), the report repeats
// the total on each slot and marks it.
#define CW_VT_ALL(X)                                                                     \
    X(821C3380) X(82332528) X(82463648) X(82466CB8) X(828023B8) X(82802408)              \
    X(82803678) X(828036F0) X(82803928) X(82803ED0) X(82804808) X(82804840)              \
    X(828048C0) X(82804D98) X(82804DB0) X(82805C38) X(82805D70) X(82806000)              \
    X(828064F8) X(82807E48) X(82807EA8) X(82809A70) X(82809AB0) X(8280BA10)              \
    X(8280BB08) X(8280BBA0) X(8280BD38) X(8280BEB8) X(8280D438) X(8280D440)              \
    X(8280D448) X(8280D458) X(8280DDF8) X(8280DE00) X(8280E308) X(8280E978)              \
    X(8280FD98) X(8280FEB0) X(8280FF30) X(82810160) X(82810210) X(82810628)              \
    X(82810698) X(82810708) X(828107D8) X(82810960) X(828109D8) X(82810A60)              \
    X(82810AF0) X(82814A98) X(82814AE8) X(82815A78) X(82815BC8) X(82815C18)              \
    X(828162C8) X(82816368) X(82816920) X(82816970) X(82816AA8) X(82816BB0)              \
    X(8281A5E0) X(8281B8E0) X(8281BB70) X(8281C0A8) X(8281C370)

// Hooks for the union MINUS sub_8281A5E0 and sub_8280D440, which keep their
// richer custom hooks below; those feed the same counters so the census stays
// complete.
#define CW_VT_HOOKS(X)                                                                   \
    X(821C3380) X(82332528) X(82463648) X(82466CB8) X(828023B8) X(82802408)              \
    X(82803678) X(828036F0) X(82803928) X(82803ED0) X(82804808) X(82804840)              \
    X(828048C0) X(82804D98) X(82804DB0) X(82805C38) X(82805D70) X(82806000)              \
    X(828064F8) X(82807E48) X(82807EA8) X(82809A70) X(82809AB0) X(8280BA10)              \
    X(8280BB08) X(8280BBA0) X(8280BD38) X(8280BEB8) X(8280D438)                          \
    X(8280D448) X(8280D458) X(8280DDF8) X(8280DE00) X(8280E308) X(8280E978)              \
    X(8280FD98) X(8280FEB0) X(8280FF30) X(82810160) X(82810210) X(82810628)              \
    X(82810698) X(82810708) X(828107D8) X(82810960) X(828109D8) X(82810A60)              \
    X(82810AF0) X(82814A98) X(82814AE8) X(82815A78) X(82815BC8) X(82815C18)              \
    X(828162C8) X(82816368) X(82816920) X(82816970) X(82816AA8) X(82816BB0)              \
    X(8281B8E0) X(8281BB70) X(8281C0A8) X(8281C370)

namespace
{

#define X(addr) std::atomic<uint64_t> g_vtc_##addr[3];
CW_VT_ALL(X)
#undef X

struct VtCtrRow
{
    uint32_t addr;
    std::atomic<uint64_t>* c;   // [3], indexed by class
};
const VtCtrRow kVtCtrRows[] = {
#define X(addr) { 0x##addr, g_vtc_##addr },
    CW_VT_ALL(X)
#undef X
};

inline std::atomic<uint64_t>* VtCtrFor(uint32_t addr)
{
    for (const auto& r : kVtCtrRows)
        if (r.addr == addr)
            return r.c;
    return nullptr;
}

inline void VtNote(uint8_t* b, uint32_t self, std::atomic<uint64_t>* ctr3)
{
    const int c = VtClass(b, self);
    if (c >= 0)
        ctr3[c].fetch_add(1, std::memory_order_relaxed);
}

} // namespace

#define X(addr)                                                                          \
    extern "C" PPC_FUNC(__imp__sub_##addr);                                              \
    PPC_FUNC(sub_##addr)                                                                 \
    {                                                                                    \
        VtNote(base, ctx.r3.u32, g_vtc_##addr);                                          \
        __imp__sub_##addr(ctx, base);                                                    \
    }
CW_VT_HOOKS(X)
#undef X

// THE CTOR HOOKS, which are the census's validity gate. Each counts (the
// count must reproduce CreateWidget's independent name-based count, the same
// two-direction agreement that settled the meter's chain in finding 45) and
// records the FIRST vtable pointer observed at [this+0] after construction —
// the runtime check on the three constants every filter above depends on.
namespace
{
std::atomic<uint64_t> g_ctorCount[3];
std::atomic<uint32_t> g_ctorVtSeen[3];
} // namespace

#define CW_FE_CTORS(X)                                                                   \
    X(8280D300, 0)                                                                       \
    X(8280B990, 1)                                                                       \
    X(8280DCD8, 2)

#define X(addr, cls)                                                                     \
    extern "C" PPC_FUNC(__imp__sub_##addr);                                              \
    PPC_FUNC(sub_##addr)                                                                 \
    {                                                                                    \
        g_ctorCount[cls].fetch_add(1, std::memory_order_relaxed);                        \
        __imp__sub_##addr(ctx, base);                                                    \
        if (ctx.r3.u32 >= 0x1000)                                                        \
        {                                                                                \
            uint32_t expect = 0;                                                         \
            g_ctorVtSeen[cls].compare_exchange_strong(expect, GuestU32(base, ctx.r3.u32),\
                                                      std::memory_order_relaxed);        \
        }                                                                                \
    }
CW_FE_CTORS(X)
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
// The guest base, stashed from a hook so the report can read the factory table itself.
// Reading the table AT RUNTIME is the only way to get the {creator, id, name} mapping
// right: deriving it by eye from the registration code produced a wrong creator for
// cFEMeter AND for cFELockBox (gotcha 322, twice). The table is the ground truth.
std::atomic<uint8_t*> g_base{ nullptr };
constexpr uint32_t kFactoryTable = 0x82AF3118;   // 25 entries x {creator, id, name}
constexpr uint32_t kFactoryCount = 25;
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

// WHO READS THE METER'S VALUE? The 0x1CC getter is called 10,192 times in one play
// session while the meter draws nothing, so its callers ARE the meter's live update/draw
// path — and cFEMeter has no draw method of its own, so that path is inherited code that
// no per-class hook can isolate. The link register names it directly: the recompiler sets
// ctx.lr on every `bl`, so on entry it holds the return address of the call site.
namespace
{
std::vector<std::pair<uint32_t, uint64_t>> g_getCallers;   // {return address, count}
std::atomic<uint64_t> g_getCalls{ 0 };
} // namespace

extern "C" PPC_FUNC(__imp__sub_8280D440);
PPC_FUNC(sub_8280D440)
{
    VtNote(base, ctx.r3.u32, g_vtc_8280D440);   // meter vtable slot 31
    const uint32_t lr = uint32_t(ctx.lr);
    g_getCalls.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(g_missMutex);
        bool seen = false;
        for (auto& e : g_getCallers)
            if (e.first == lr) { ++e.second; seen = true; break; }
        if (!seen && g_getCallers.size() < 32)
            g_getCallers.emplace_back(lr, 1);
    }
    __imp__sub_8280D440(ctx, base);
}

// THE PER-WIDGET UPDATE, AND THE ACTION IT GUARDS.
//
// sub_8281C9D0 walks the widget list and calls vtable[6] on each element; for cFEMeter that
// is sub_8281A5E0, which is a CONDITIONAL:
//
//     bl   0x82804770                 ; r3 = <some current value>
//     lwz  r11, 0x25C(this)
//     cmpw r11, r3
//     bne  -> call [this+0xC0]->vt[0xC]   ; the "changed" path
//     lwz  r11, 0x254(this) ; cmpwi 0 ; bne -> DO NOTHING
//     lwz  r11, 0x268(this) ; lwz r10, 0x264(this) ; cmpw ; bgt -> DO NOTHING
//     bl   0x82816128                 ; <-- the guarded action
//
// Both functions are SHARED with widget classes that draw correctly, so a plain hook counts
// everything and isolates nothing. The filter is the object's own vtable pointer: a cFEMeter
// has 0x820BDBE8 at offset 0, written by its constructor. That makes a shared function
// measurable per class without guessing which slot is class-specific — and after three wrong
// labels in this investigation (findings 43, 44, 46) the rule is to identify things by
// something the guest itself wrote, not by where they sit.
namespace
{
std::atomic<uint64_t> g_updAll{ 0 }, g_updMeter{ 0 };
std::atomic<uint64_t> g_actAll{ 0 }, g_actMeter{ 0 };
std::atomic<uint64_t> g_f254nz{ 0 }, g_fGT{ 0 };
} // namespace

extern "C" PPC_FUNC(__imp__sub_8281A5E0);
PPC_FUNC(sub_8281A5E0)
{
    const uint32_t self = ctx.r3.u32;
    VtNote(base, self, g_vtc_8281A5E0);   // meter vtable slot 6
    g_updAll.fetch_add(1, std::memory_order_relaxed);
    if (IsMeter(base, self))
    {
        g_updMeter.fetch_add(1, std::memory_order_relaxed);
        // The two guards that are plain field reads, sampled here so a blocked action can
        // say WHICH test blocked it rather than only that it did.
        if (GuestU32(base, self + 0x254) != 0)
            g_f254nz.fetch_add(1, std::memory_order_relaxed);
        if (int32_t(GuestU32(base, self + 0x268)) > int32_t(GuestU32(base, self + 0x264)))
            g_fGT.fetch_add(1, std::memory_order_relaxed);
    }
    __imp__sub_8281A5E0(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_82816128);
PPC_FUNC(sub_82816128)
{
    g_actAll.fetch_add(1, std::memory_order_relaxed);
    if (IsMeter(base, ctx.r3.u32))
        g_actMeter.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_82816128(ctx, base);
}

// (Part 3's meter-only vtable census lived here; the three-class census above
// replaces it — same filter, same hooks, plus the two drawing classes.)

// THE BYTE AT +0x6A, which gates the frontend's slot-21 call on a widget.
//
// Both frontend call sites through vtable slot 21 are guarded the same way:
//
//     lbz  r11, 0x6A(widget) ; cmplwi r11,0 ; beq -> SKIP
//     lwz  r3, 0xB0(self)    ; lwz r11,0(r3) ; lwz r11,0x54(r11) ; bctrl
//
// so the call is made on [self+0xB0] WITH the widget as an argument — slot 21 belongs to
// that object, not to the widget — and a zero at widget+0x6A skips it entirely.
//
// Slot 21 of cFEMeter's own vtable never runs (the vtable census), and this flag is the only
// gate found on the frontend's slot-21 path. THAT IS A LEAD, NOT A CONCLUSION: whether the
// two are the same call has not been shown. So this measures the flag directly, on meters and
// on non-meters, in the same traversal — a comparison rather than a story, after three wrong
// labels in findings 43, 44 and 46.
namespace
{
std::atomic<uint64_t> g_walkMeter{ 0 }, g_walkOther{ 0 };
std::atomic<uint64_t> g_flagMeterSet{ 0 }, g_flagOtherSet{ 0 };
inline void NoteWidgetFlag(uint8_t* b, uint32_t w)
{
    if (w < 0x1000)
        return;
    const bool meter = GuestU32(b, w) == kMeterVtable;
    const bool set = *(b + w + 0x6A) != 0;
    (meter ? g_walkMeter : g_walkOther).fetch_add(1, std::memory_order_relaxed);
    if (set)
        (meter ? g_flagMeterSet : g_flagOtherSet).fetch_add(1, std::memory_order_relaxed);
}
} // namespace

extern "C" PPC_FUNC(__imp__sub_82803570);
PPC_FUNC(sub_82803570)
{
    NoteWidgetFlag(base, ctx.r4.u32);   // this site takes the widget in r4
    __imp__sub_82803570(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_82784588);
PPC_FUNC(sub_82784588)
{
    g_base.store(base, std::memory_order_relaxed);
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
    // NO SECOND lock_guard HERE. g_missMutex is already held by the outer scope and it is
    // NOT recursive, so re-locking it self-deadlocked the report: the run of 2026-08-16
    // printed cleanly as far as the CreateWidget list and then stopped dead, the shutdown
    // never reached _Exit, and guest threads kept logging over the top of it. An
    // instrument that hangs the shutdown it is reporting from destroys the run it was
    // measuring — worse than one that prints nothing, because the log still looks alive.
    {
        std::fprintf(stderr,
                     "[fe]   cFEMeter get 0x1CC called %llu x. CALL SITES (return addresses):\n",
                     (unsigned long long)g_getCalls.load());
        if (g_getCallers.empty())
            std::fprintf(stderr, "[fe]     (never called)\n");
        else
            for (const auto& e : g_getCallers)
                std::fprintf(stderr, "[fe]     from 0x%08X   %llu x\n", e.first,
                             (unsigned long long)e.second);
    }

    std::fprintf(stderr,
                 "[fe]   per-widget update sub_8281A5E0: %llu calls, %llu ON A METER\n"
                 "[fe]     of those meter calls: [0x254]!=0 %llu x, [0x268]>[0x264] %llu x\n"
                 "[fe]   guarded action sub_82816128: %llu calls, %llu ON A METER\n",
                 (unsigned long long)g_updAll.load(), (unsigned long long)g_updMeter.load(),
                 (unsigned long long)g_f254nz.load(), (unsigned long long)g_fGT.load(),
                 (unsigned long long)g_actAll.load(), (unsigned long long)g_actMeter.load());

    std::fprintf(stderr,
                 "[fe]   widget+0x6A (gates the frontend's slot-21 call), sampled in sub_82803570:\n"
                 "[fe]     METERS     seen %llu, flag SET %llu\n"
                 "[fe]     non-meters seen %llu, flag SET %llu\n",
                 (unsigned long long)g_walkMeter.load(),
                 (unsigned long long)g_flagMeterSet.load(),
                 (unsigned long long)g_walkOther.load(),
                 (unsigned long long)g_flagOtherSet.load());
    // ------------------------------------------------------------------
    // THE THREE-CLASS VTABLE CENSUS.
    // First its validity gate: the ctor counts (compare against CreateWidget's
    // independent name-based counts above) and the vtable pointer each ctor was
    // OBSERVED to write, against the constant the filters use.
    std::fprintf(stderr, "[fe]   THREE-CLASS VTABLE CENSUS — ctor validity gate first:\n");
    bool vtValid = true;
    for (int c = 0; c < 3; c++)
    {
        const uint32_t seen = g_ctorVtSeen[c].load();
        const bool ok = seen == kVtOf[c];
        if (g_ctorCount[c].load() && !ok)
            vtValid = false;
        std::fprintf(stderr,
                     "[fe]     %-10s ctor %6llu x   [this+0] observed 0x%08X  expected 0x%08X  %s\n",
                     kVtClassName[c], (unsigned long long)g_ctorCount[c].load(), seen,
                     kVtOf[c], g_ctorCount[c].load() == 0 ? "(never ran)"
                                                          : ok ? "OK" : "MISMATCH");
    }
    if (!vtValid)
        std::fprintf(stderr,
                     "[fe]   A CTOR WROTE A DIFFERENT VTABLE THAN THE FILTER USES — every\n"
                     "[fe]   count below for that class is VOID. Re-derive the constant.\n");
    // Counts are PER FUNCTION: a function occupying several slots of one vtable
    // repeats its total on each ('*'). A zero can also be a sub-object `this`
    // missing the filter — a blind spot, not proof of absence.
    std::fprintf(stderr,
                 "[fe]   slot |        cFEMeter        |        cFEBitmap       |         cFEText\n");
    for (int s = 0; s < kVtSlotCount; s++)
    {
        char cols[3][40];
        uint64_t cnt[3] = { 0, 0, 0 };
        for (int c = 0; c < 3; c++)
        {
            const uint32_t fn = kVtSlots[c][s];
            std::atomic<uint64_t>* ctr = VtCtrFor(fn);
            cnt[c] = ctr ? ctr[c].load() : 0;
            int dup = 0;
            for (int t = 0; t < kVtSlotCount; t++)
                if (kVtSlots[c][t] == fn)
                    dup++;
            std::snprintf(cols[c], sizeof cols[c], "%08X %8llu%s", fn,
                          (unsigned long long)cnt[c], dup > 1 ? "*" : " ");
        }
        const bool drawersOnly = cnt[0] == 0 && (cnt[1] || cnt[2]);
        const bool meterOnly = cnt[0] && !cnt[1] && !cnt[2];
        std::fprintf(stderr, "[fe]    %2d  | %s | %s | %s %s\n", s, cols[0], cols[1],
                     cols[2],
                     drawersOnly ? "<== DRAWERS ONLY" : meterOnly ? "<== meter only" : "");
    }

    // The factory table, read out of guest memory rather than inferred from the code.
    if (uint8_t* b = g_base.load(std::memory_order_relaxed))
    {
        auto rd = [&](uint32_t va) {
            const uint8_t* p = b + va;
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        };
        std::fprintf(stderr, "[fe]   FACTORY TABLE at 0x%08X, read from guest memory:\n",
                     kFactoryTable);
        for (uint32_t i = 0; i < kFactoryCount; i++)
        {
            const uint32_t e = kFactoryTable + i * 12;
            const uint32_t creator = rd(e), id = rd(e + 4), nameVa2 = rd(e + 8);
            char nm[48];
            size_t k = 0;
            if (nameVa2)
            {
                const char* q = reinterpret_cast<const char*>(b + nameVa2);
                while (k < sizeof nm - 1 && q[k] > 0x20 && q[k] < 0x7F) { nm[k] = q[k]; ++k; }
            }
            nm[k] = 0;
            std::fprintf(stderr, "[fe]     [%2u] creator 0x%08X  id 0x%08X  %s\n",
                         i, creator, id, nm);
        }
    }
    if (g_missed.empty())
        std::fprintf(stderr, "[fe]   no class name failed to resolve.\n");
    else
        for (const auto& s : g_missed)
            std::fprintf(stderr, "[fe]     UNRESOLVED CLASS: %s\n", s.c_str());
}
