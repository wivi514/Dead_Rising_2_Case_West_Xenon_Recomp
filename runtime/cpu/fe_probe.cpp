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
#include <cstdlib>

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "fe_probe.h"

namespace
{

// The ctors and the 0x1CC accessors that used to sit in this list moved into the
// three-class vtable census below, where their counts come back CLASS-FILTERED.
// The intern function 0x827815D0 moved to its own hook below (round 6): it still
// counts as CONTROL A, and now also records the name -> id mapping, because
// [widget+0x4] IS the interned name id (slot 18 compares its r4 against it) and
// the blocked containers can therefore be printed BY NAME.
#define CW_FE_FUNCS(X)                                                                   \
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

// CONTROL A, now also the id -> name dictionary. Every widget name reaches
// sub_827815D0(name) -> id at least once (registration, parsing, lookups), so
// recording first-seen pairs makes every interned id in guest memory readable.
// ~1.2M calls/session, one hash probe each under a mutex — the frontend
// interns from one thread, so contention is nil; the fps counter is the
// canary if that assumption is ever wrong (gotcha 223).
namespace
{
std::atomic<uint64_t> g_fe_827815D0{ 0 };
std::mutex g_nameMutex;
std::unordered_map<uint32_t, std::string> g_idName;   // interned id -> name

inline const char* NameOfId(uint32_t id)
{
    // caller holds no lock; report-time only
    auto it = g_idName.find(id);
    return it == g_idName.end() ? "?" : it->second.c_str();
}
} // namespace

extern "C" PPC_FUNC(__imp__sub_827815D0);
PPC_FUNC(sub_827815D0)
{
    g_fe_827815D0.fetch_add(1, std::memory_order_relaxed);
    const uint32_t nameVa = ctx.r3.u32;
    __imp__sub_827815D0(ctx, base);
    if (!nameVa)
        return;
    const uint32_t id = ctx.r3.u32;
    std::lock_guard<std::mutex> lock(g_nameMutex);
    if (g_idName.size() >= 250000 || g_idName.count(id))
        return;
    const char* p = reinterpret_cast<const char*>(base + nameVa);
    char buf[48];
    size_t n = 0;
    while (n < sizeof buf - 1 && p[n] >= 0x20 && p[n] < 0x7F) { buf[n] = p[n]; ++n; }
    buf[n] = 0;
    if (n)
        g_idName.emplace(id, buf);
}

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
    X(82332528) X(82463648) X(82466CB8) X(828023B8) X(82802408)                          \
    X(82803678) X(828036F0) X(82803928) X(82803ED0) X(82804808) X(82804840)              \
    X(828048C0) X(82804D98) X(82804DB0) X(82805C38) X(82805D70) X(82806000)              \
    X(828064F8) X(82807E48) X(82807EA8) X(82809A70) X(82809AB0) X(8280BA10)              \
    X(8280BB08) X(8280BBA0) X(8280BD38) X(8280BEB8) X(8280D438)                          \
    X(8280D448) X(8280D458) X(8280DDF8) X(8280DE00) X(8280E308) X(8280E978)              \
    X(8280FD98) X(8280FEB0) X(82810160) X(82810210) X(82810628)                          \
    X(82810698) X(82810708) X(82810960) X(828109D8) X(82810A60)                          \
    X(82810AF0) X(82814A98) X(82814AE8) X(82815A78) X(82815BC8)                          \
    X(828162C8) X(82816368) X(82816920) X(82816970) X(82816AA8) X(82816BB0)              \
    X(8281B8E0) X(8281BB70) X(8281C0A8) X(8281C370)

// Comment above says CW_VT_HOOKS is the union minus the custom-hooked
// functions; after the first census run that exclusion list grew to FOUR —
// sub_8281A5E0, sub_8280D440, and now sub_8280FF30 / sub_82815C18 / the shared
// stub sub_821C3380, which get the slot-8 traversal probes below.

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

// Slot 18 is FindChildById(this, id) — it compares r4 against [this+0x4] and
// recurses the child tree. Whoever calls it with a blocked container's id is
// the game code that MANIPULATES that container, so record {id, LR} pairs.
// (Excluded from CW_VT_HOOKS above; this hook feeds the same census counter.)
namespace
{
std::mutex g_findMutex;
std::vector<std::array<uint32_t, 2>> g_findRows;   // {id, lr}
std::vector<uint64_t> g_findCounts;
} // namespace

extern "C" PPC_FUNC(__imp__sub_828107D8);
PPC_FUNC(sub_828107D8)
{
    VtNote(base, ctx.r3.u32, g_vtc_828107D8);
    const uint32_t id = ctx.r4.u32, lr = uint32_t(ctx.lr);
    {
        std::lock_guard<std::mutex> lock(g_findMutex);
        bool seen = false;
        for (size_t i = 0; i < g_findRows.size(); i++)
            if (g_findRows[i][0] == id && g_findRows[i][1] == lr)
            {
                ++g_findCounts[i];
                seen = true;
                break;
            }
        if (!seen && g_findRows.size() < 2048)
        {
            g_findRows.push_back({ id, lr });
            g_findCounts.push_back(1);
        }
    }
    __imp__sub_828107D8(ctx, base);
}

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

// ============================================================================
// THE SLOT-8 TRAVERSAL PROBE — added after the census's first run, 2026-08-16.
//
// The census answered with a disproportion, not a hole: slots 8 and 9 are
// called TOGETHER (identical counts per class) at per-frame magnitude on the
// drawing classes — 32,024 on cFEBitmap, 9,247 on cFEText, ~32-37 per widget —
// and only 118 times on cFEMeter, ~1.7 per meter, the same one-shot magnitude
// as the layout. So a traversal that hits drawers every frame skips meters
// almost always. Both slot-8 bodies (drawers' sub_8280FF30 and the meter's own
// override sub_82815C18) open by testing bit 0x02000000 of the flag word at
// [this+0x10] and do nothing when it is clear.
//
// Two measurements, both comparisons rather than stories:
//   * the LR at slot-8 entry names the traversal that decides who gets drawn —
//     the walker we have been unable to name for four findings;
//   * the flag bit, sampled per class BOTH at slot-8 entry and in the update
//     walk (which reaches every widget, meters included, 4,480x last run),
//     splits the outcomes: meters flag-clear in the update walk -> the defect
//     is whoever should SET the flag; meters flag-set but still skipped ->
//     the pruning is upstream of the flag (parent chain or list membership).
// ROUND 2 REFINED THE QUESTION. The flag census answered: meters are 97%
// bit-0x02000000-SET in the update walk, so THAT bit is not the defect — and
// the recorded LRs (0x828100E8 / 0x8281014C, 60,668 calls between them) name
// the walker: the drawers' slot-8 body ITSELF recurses over a child list at
// [child+0xC], and its per-child gate is NOT the bit the callee tests:
//
//     lwz  r11, 0x10(child)
//     oris r11, r11, 0x200        ; the walker SETS 0x02000000 itself (loop A)
//     rlwinm. r10, r11, 0, 8, 8   ; ...and gates on bit 0x00800000
//     beq  -> skip child
//     lfs  f0, 0x6C(child) ; fcmpu ; ble -> skip child   ; alpha-shaped float
//     bctrl [vt+0x20]             ; child->slot8()
//
// So the per-class samples that decide the defect are bit 0x00800000 and the
// float at [this+0x6C], taken where every widget is reachable — the update
// walk.
namespace
{
std::atomic<uint64_t> g_s8Flag[3][2];    // [class][bit 0x02000000 clear/set] at slot-8 entry
std::atomic<uint64_t> g_updFlag[3][2];   // same, sampled in the update walk
std::atomic<uint64_t> g_updBit8[3][2];   // bit 0x00800000 — the WALKER's gate
std::atomic<uint64_t> g_updAlpha[3][2];  // [this+0x6C] <= 0 / > 0 — the walker's other gate
std::mutex g_s8Mutex;
std::vector<std::pair<uint32_t, uint64_t>> g_s8Callers;   // {LR, count} across both bodies

inline float GuestF32(uint8_t* b, uint32_t va)
{
    const uint32_t u = GuestU32(b, va);
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

// ROUND 3 ANSWERED: meters PASS both walker gates where they are sampled —
// bit 0x00800000 SET 92%, [+0x6C] positive 100% — and still receive ~250
// slot-8 calls against 9,236 update-walk visits. So the pruning is not the
// meter's own state. Slot 8 is a TREE recursion (children at [this+0x8],
// siblings at [+0xC], self-render via own slot 9), which leaves exactly two
// places to lose a meter: an ANCESTOR failing the same gates (the recursion
// prunes whole subtrees), or the meter not being linked under a drawn parent
// at all. Both are measured here, per class, in the update walk:
//   * parent = [self+0xB0] (the field cFEMeter::Draw reads its inherited
//     scale through); null / self-found-in-parent's-child-chain / not-found;
//   * climb the parent chain and test each ancestor's bit 0x00800000 and
//     [+0x6C]; count which gate the FIRST failing ancestor fails, and record
//     that ancestor's VTABLE POINTER — identity by what the guest wrote.
namespace
{
std::atomic<uint64_t> g_parNull[3], g_inChain[3], g_notInChain[3];
std::atomic<uint64_t> g_ancFailBit8[3], g_ancFailAlpha[3], g_ancAllPass[3];
std::mutex g_ancMutex;
// [class] -> distinct {failing ancestor's vtable, count}
std::vector<std::pair<uint32_t, uint64_t>> g_ancFailVt[3];
// ROUND 4 ANSWERED: tree membership is perfect for all three classes, and the
// meters' loss is an ANCESTOR failing bit 0x00800000 — 92% of samples, and the
// failing class is the BASE cFEWidget (vtable 0x820BD310, the grouping node).
// The base ctor sets 0x03C00000, i.e. every widget is BORN shown — so the
// meters' containers are being actively HIDDEN. These record the failing
// ancestors' ADDRESSES (meters only) so the report can dump each one and
// correlate it against the hide/show recorder below, within the same run.
std::vector<std::pair<uint32_t, uint64_t>> g_meterAncFail;   // {ancestor VA, count}
std::atomic<uint64_t> g_ancOverflow{ 0 };   // fail samples beyond the 64-row cap
} // namespace

// THE INTERVENTION ARM — CW_FE_FORCESHOW=1. Round 5 found the meters' own
// containers hidden (bit8 clear) or transparent (alpha 0) while NONE of the
// twelve frontend hide/show functions ever ran, so the actor writes the flag
// word by some idiom the static scans did not cover. Before hunting it, prove
// the causal chain end to end: force bit8 set and alpha positive on every
// node of every meter's ancestor chain and let the operator LOOK. Widgets
// appearing = findings 48-50 are the whole story and the remaining defect is
// "who should show these groups"; nothing appearing = a fourth gate exists.
// The same binary without the env var is the control arm, and the forced-write
// counters below prove engagement either way (gotcha 151).
namespace
{
const bool kForceShow = [] {
    const char* e = std::getenv("CW_FE_FORCESHOW");
    return e && *e && *e != '0';
}();
std::atomic<uint64_t> g_forcedBit8{ 0 }, g_forcedAlpha{ 0 };

inline void GuestStoreU32(uint8_t* b, uint32_t va, uint32_t v)
{
    uint8_t* p = b + va;
    p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v);
}
} // namespace

inline void TreeSample(uint8_t* b, uint32_t self, int c)
{
    if (c == 0 && kForceShow)
    {
        uint32_t w = self;
        for (int d = 0; d < 32 && w >= 0x1000; d++, w = GuestU32(b, w + 0xB0))
        {
            const uint32_t fl = GuestU32(b, w + 0x10);
            if (!(fl & 0x00800000u))
            {
                GuestStoreU32(b, w + 0x10, fl | 0x00800000u);
                g_forcedBit8.fetch_add(1, std::memory_order_relaxed);
            }
            if (!(GuestF32(b, w + 0x6C) > 0.0f))
            {
                GuestStoreU32(b, w + 0x6C, 0x3F800000u);   // 1.0f
                g_forcedAlpha.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    const uint32_t parent = GuestU32(b, self + 0xB0);
    if (parent < 0x1000)
    {
        g_parNull[c].fetch_add(1, std::memory_order_relaxed);
        return;
    }
    // Is self in parent's child chain? head [parent+0x8], siblings [+0xC].
    bool found = false;
    uint32_t w = GuestU32(b, parent + 0x8);
    for (int i = 0; i < 512 && w >= 0x1000; i++, w = GuestU32(b, w + 0xC))
        if (w == self) { found = true; break; }
    (found ? g_inChain : g_notInChain)[c].fetch_add(1, std::memory_order_relaxed);

    // Climb ancestors; the walker prunes a subtree on the first gate failure.
    uint32_t a = parent;
    for (int d = 0; d < 32 && a >= 0x1000; d++, a = GuestU32(b, a + 0xB0))
    {
        const bool bit8ok = (GuestU32(b, a + 0x10) & 0x00800000u) != 0;
        const bool alphaok = GuestF32(b, a + 0x6C) > 0.0f;
        if (bit8ok && alphaok)
            continue;
        (bit8ok ? g_ancFailAlpha : g_ancFailBit8)[c].fetch_add(1, std::memory_order_relaxed);
        const uint32_t avt = GuestU32(b, a);
        std::lock_guard<std::mutex> lock(g_ancMutex);
        bool seen = false;
        for (auto& e : g_ancFailVt[c])
            if (e.first == avt) { ++e.second; seen = true; break; }
        if (!seen && g_ancFailVt[c].size() < 24)
            g_ancFailVt[c].emplace_back(avt, 1);
        if (c == 0)
        {
            for (auto& e : g_meterAncFail)
                if (e.first == a) { ++e.second; return; }
            if (g_meterAncFail.size() < 64)
                g_meterAncFail.emplace_back(a, 1);
            else
                g_ancOverflow.fetch_add(1, std::memory_order_relaxed);   // gotcha 109
        }
        return;
    }
    g_ancAllPass[c].fetch_add(1, std::memory_order_relaxed);
}

// THE HIDE/SHOW RECORDER. Every widget is born shown (base ctor sets
// 0x03C00000), so whoever last clears bit 0x00800000 on the meters' container
// is the defect's actor — or the actor whose matching Show never fires. The
// image has exactly ten frontend-band clear-bit8 sites and nine set-bit8
// sites; they resolve to the twelve functions below (several pair a hide and
// a show — pager idioms over [this+0x640], and HUD-wrapper objects that own a
// widget at [this+0x3C4] with a bool at +0x3CC). Each call records
// {function, LR, r3, [r3+0x3C4]}; the report prints them beside the failing
// ancestors' addresses so the actor can be matched within one run.
// The 0x824C-0x8250 band has ~300 more clear sites — game-side, hooked only
// if this frontend-band set fails to name the actor.
#define CW_HS_FUNCS(X)                                                                   \
    X(82805738, "hide-all over [r3+0x63C..], pager")                                     \
    X(828058E0, "show-selected, same pager")                                             \
    X(828065D0, "wrapper SHOW  [r3+0x3C4], bool +0x3CC")                                 \
    X(82808900, "wrapper HIDE  [r3+0x3C4], bool +0x3CC")                                 \
    X(82809608, "hide via [+0x74] chain")                                                \
    X(82809698, "show via [+0x74] chain")                                                \
    X(82817E20, "hide [r3+0x3C4] variant A")                                             \
    X(82818030, "hide+show [r3+0x3C4] variant B")                                        \
    X(8281A890, "list show-first-N / hide-rest")                                         \
    X(8281ACB8, "hide loop of 3")                                                        \
    X(8281D268, "hide loop over children")                                               \
    X(8281D5C0, "show, reads [r3+0xB0]/[+0xB8]")

namespace
{
struct HsRow
{
    uint32_t func, lr, obj, widget;
    uint64_t count;
};
std::mutex g_hsMutex;
std::vector<HsRow> g_hsRows;

inline void NoteHideShow(uint8_t* b, uint32_t func, uint32_t lr, uint32_t obj)
{
    uint32_t widget = 0;
    if (obj >= 0x1000)
        widget = GuestU32(b, obj + 0x3C4);
    std::lock_guard<std::mutex> lock(g_hsMutex);
    for (auto& r : g_hsRows)
        if (r.func == func && r.lr == lr && r.obj == obj && r.widget == widget)
        {
            ++r.count;
            return;
        }
    if (g_hsRows.size() < 192)
        g_hsRows.push_back({ func, lr, obj, widget, 1 });
}
} // namespace

#define X(addr, what)                                                                    \
    extern "C" PPC_FUNC(__imp__sub_##addr);                                              \
    PPC_FUNC(sub_##addr)                                                                 \
    {                                                                                    \
        NoteHideShow(base, 0x##addr, uint32_t(ctx.lr), ctx.r3.u32);                      \
        __imp__sub_##addr(ctx, base);                                                    \
    }
CW_HS_FUNCS(X)
#undef X

// ROUNDS 9-10 REWROTE THE MODEL: nothing draws per frame through slot 8 — the
// widget tree is a RETAINED rebuild, and the pp/mission chains are walked all
// the way down to their meters. The failure is inside cFEMeter's render
// (slot 9, sub_82804808), which is one gate:
//     if ([this+0x244] == -1) return;   // silently draw nothing
//     submit(global, [this+0x1D0], [this+0x244], 2);
// and [this+0x244] is written exactly twice in the frontend band: the ctor
// (initial value) and the property setter sub_82816368, which resolves a
// name string (copied to [this+0x2C0]) through registry [0x82AF3028] via
// sub_82813940 — returning -1 on failure, and the meter then never draws.
// These two instruments close the loop: the handle per NAMED meter, and every
// registry lookup's {name -> result}.
namespace
{
std::mutex g_mhMutex;
// widget id -> {last [this+0x244], samples, last [this+0x1D0]}
struct MeterHandle { uint32_t handle, value; uint64_t samples; };
std::unordered_map<uint32_t, MeterHandle> g_meterHandle;

struct RegRow { std::string name; uint32_t result; uint64_t count; };
std::vector<RegRow> g_regRows;
} // namespace

// THE RETAINED-ITEM UPDATE, sub_8273A510(mgr, index, id, type, ...): items are
// a 128-byte-stride array at [mgr+0x28], and the work happens through a raw
// function pointer at [[mgr+0]+8] — unhookable until named. This records the
// pointer's distinct targets plus the index/id/type ranges, so the NEXT hook
// has an address instead of a guess.
namespace
{
std::mutex g_riMutex;
std::vector<std::array<uint32_t, 4>> g_riRows;   // {fptr, id(r5), type(r6), count-ish}
std::vector<uint64_t> g_riCounts;
} // namespace

extern "C" PPC_FUNC(__imp__sub_8273A510);
PPC_FUNC(sub_8273A510)
{
    uint32_t fptr = 0;
    const uint32_t mgr = ctx.r3.u32;
    if (mgr >= 0x1000)
    {
        const uint32_t inner = GuestU32(base, mgr);
        if (inner >= 0x1000)
            fptr = GuestU32(base, inner + 8);
    }
    const uint32_t id = ctx.r5.u32, ty = ctx.r6.u32;
    {
        std::lock_guard<std::mutex> lock(g_riMutex);
        bool seen = false;
        for (size_t i = 0; i < g_riRows.size(); i++)
            if (g_riRows[i][0] == fptr && g_riRows[i][1] == id && g_riRows[i][2] == ty)
            {
                ++g_riCounts[i];
                seen = true;
                break;
            }
        if (!seen && g_riRows.size() < 96)
        {
            g_riRows.push_back({ fptr, id, ty, 0 });
            g_riCounts.push_back(1);
        }
    }
    __imp__sub_8273A510(ctx, base);
}

// THE DRAW'S REAL SUBMIT CHAIN (round 12): cFEMeter::Draw's emitting branch
// runs only when [this+0x254] == 0 and goes
//     sub_8272EB40(batch, 0, -1)                       ; begin
//     id = sub_8275CD58(global, name)                  ; resolve, cached in
//                                                      ;   [0x82AF3648/44]
//     sub_8273C870(batch, name, id, 1, &this+0x26C)    ; submit (x2)
//     sub_8274A698 ; sub_8272EC50(batch, r, 0x28)      ; end
// — NOT slot 9's sub_8273A510, which never runs. The two resource-name
// pointers ([0x82AF363C/40]) are runtime-populated, so everything here is
// recorded dynamically: begin/end counts say whether the branch is entered,
// and the resolve rows name the resources and their results.
namespace
{
std::atomic<uint64_t> g_batchBegin{ 0 }, g_batchEnd{ 0 }, g_batchSubmit{ 0 };
std::mutex g_rsMutex;
struct ResolveRow { std::string name; uint32_t result; uint64_t count; };
std::vector<ResolveRow> g_resolveRows;
} // namespace

extern "C" PPC_FUNC(__imp__sub_8272EB40);
PPC_FUNC(sub_8272EB40)
{
    g_batchBegin.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_8272EB40(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_8272EC50);
PPC_FUNC(sub_8272EC50)
{
    g_batchEnd.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_8272EC50(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_8273C870);
PPC_FUNC(sub_8273C870)
{
    g_batchSubmit.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_8273C870(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_8275CD58);
PPC_FUNC(sub_8275CD58)
{
    const uint32_t nameVa = ctx.r4.u32;
    char buf[64];
    size_t n = 0;
    if (nameVa >= 0x1000)
    {
        const char* p = reinterpret_cast<const char*>(base + nameVa);
        while (n < sizeof buf - 1 && p[n] >= 0x20 && p[n] < 0x7F) { buf[n] = p[n]; ++n; }
    }
    buf[n] = 0;
    __imp__sub_8275CD58(ctx, base);
    std::lock_guard<std::mutex> lock(g_rsMutex);
    for (auto& r : g_resolveRows)
        if (r.name == buf && r.result == ctx.r3.u32) { ++r.count; return; }
    if (g_resolveRows.size() < 96)
        g_resolveRows.push_back({ buf, ctx.r3.u32, 1 });
}

extern "C" PPC_FUNC(__imp__sub_82813940);
PPC_FUNC(sub_82813940)
{
    const uint32_t nameVa = ctx.r4.u32;
    char buf[48];
    size_t n = 0;
    if (nameVa >= 0x1000)
    {
        const char* p = reinterpret_cast<const char*>(base + nameVa);
        while (n < sizeof buf - 1 && p[n] >= 0x20 && p[n] < 0x7F) { buf[n] = p[n]; ++n; }
    }
    buf[n] = 0;
    __imp__sub_82813940(ctx, base);
    const uint32_t res = ctx.r3.u32;
    std::lock_guard<std::mutex> lock(g_mhMutex);
    for (auto& r : g_regRows)
        if (r.name == buf && r.result == res) { ++r.count; return; }
    if (g_regRows.size() < 128)
        g_regRows.push_back({ buf, res, 1 });
}

inline void FlagSample(uint8_t* b, uint32_t self, std::atomic<uint64_t> (*ctr)[2])
{
    const int c = VtClass(b, self);
    if (c < 0)
        return;
    const uint32_t flags = GuestU32(b, self + 0x10);
    ctr[c][(flags & 0x02000000u) ? 1 : 0].fetch_add(1, std::memory_order_relaxed);
    if (ctr == g_updFlag)   // the update walk samples the walker's gates too
    {
        g_updBit8[c][(flags & 0x00800000u) ? 1 : 0].fetch_add(1, std::memory_order_relaxed);
        g_updAlpha[c][GuestF32(b, self + 0x6C) > 0.0f ? 1 : 0]
            .fetch_add(1, std::memory_order_relaxed);
        TreeSample(b, self, c);
        if (c == 0)
        {
            std::lock_guard<std::mutex> lock(g_mhMutex);
            MeterHandle& m = g_meterHandle[GuestU32(b, self + 0x4)];
            m.handle = GuestU32(b, self + 0x244);
            m.value = GuestU32(b, self + 0x1D0);
            m.samples++;
        }
    }
}

// ROUND 8 LEFT ONE CONTRADICTION: the pp/health chains pass bit8+alpha in
// every update-walk sample, all container classes on the chain use the
// STANDARD recursive slot 8 — and the meters still collect ~250 draw-walk
// entries a session. The only reading that fits is that the gates FLIP WITHIN
// THE FRAME: healthy when the update walk samples them, hidden when the draw
// recursion arrives. So measure at draw time, by name: every slot-8 entry
// records the widget's interned id, and samples each CHILD's gates as the
// walker is about to test them. The recursion's stopping frontier — and the
// gate that stops it — become readable per named widget.
struct S8Gate
{
    uint64_t entered, childPass, childFailBit8, childFailAlpha;
};
namespace
{
std::unordered_map<uint32_t, S8Gate> g_s8ById;   // widget id -> draw-time stats
} // namespace

inline void NoteSlot8(uint8_t* b, uint32_t self, uint32_t lr)
{
    FlagSample(b, self, g_s8Flag);
    std::lock_guard<std::mutex> lock(g_s8Mutex);
    for (auto& e : g_s8Callers)
        if (e.first == lr) { ++e.second; break; }
    if (g_s8Callers.size() < 32 &&
        (g_s8Callers.empty() ||
         std::none_of(g_s8Callers.begin(), g_s8Callers.end(),
                      [lr](const auto& e) { return e.first == lr; })))
        g_s8Callers.emplace_back(lr, 1);
    if (g_s8ById.size() < 20000)
    {
        g_s8ById[GuestU32(b, self + 0x4)].entered++;
        uint32_t ch = GuestU32(b, self + 0x8);
        for (int i = 0; i < 512 && ch >= 0x1000; i++, ch = GuestU32(b, ch + 0xC))
        {
            S8Gate& g = g_s8ById[GuestU32(b, ch + 0x4)];
            const bool bit8ok = (GuestU32(b, ch + 0x10) & 0x00800000u) != 0;
            const bool alphaok = GuestF32(b, ch + 0x6C) > 0.0f;
            if (bit8ok && alphaok)
                g.childPass++;
            else if (!bit8ok)
                g.childFailBit8++;
            else
                g.childFailAlpha++;
        }
    }
}
} // namespace

extern "C" PPC_FUNC(__imp__sub_8280FF30);
PPC_FUNC(sub_8280FF30)
{
    VtNote(base, ctx.r3.u32, g_vtc_8280FF30);
    NoteSlot8(base, ctx.r3.u32, uint32_t(ctx.lr));
    __imp__sub_8280FF30(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_82815C18);
PPC_FUNC(sub_82815C18)
{
    VtNote(base, ctx.r3.u32, g_vtc_82815C18);
    NoteSlot8(base, ctx.r3.u32, uint32_t(ctx.lr));
    __imp__sub_82815C18(ctx, base);
}

// The screen-node class (vtable 0x820BE440 — IGOverlay/HUD/hud_* nodes) has
// its own slot 8: a wrapper that calls base draw on itself when its bit8 is
// set. Hooked so the roots appear in the draw-time frontier too.
extern "C" PPC_FUNC(__imp__sub_82812098);
PPC_FUNC(sub_82812098)
{
    NoteSlot8(base, ctx.r3.u32, uint32_t(ctx.lr));
    __imp__sub_82812098(ctx, base);
}

// The shared no-op stub is the drawers' UPDATE slot 6 (also their 30/32), so a
// flag sample here is the drawers' side of the update-walk comparison. The
// slot conflation does not matter: the flag belongs to the object, not the
// slot.
extern "C" PPC_FUNC(__imp__sub_821C3380);
PPC_FUNC(sub_821C3380)
{
    VtNote(base, ctx.r3.u32, g_vtc_821C3380);
    FlagSample(base, ctx.r3.u32, g_updFlag);
    __imp__sub_821C3380(ctx, base);
}

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
    FlagSample(base, self, g_updFlag);    // the meters' side of the update-walk flag census
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
    std::fprintf(stderr, "[fe]   sub_%-10s %10llu   %s\n", "827815D0",
                 (unsigned long long)g_fe_827815D0.load(),
                 "CONTROL A: name intern (everything registers through here)");
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

    // ------------------------------------------------------------------
    // THE SLOT-8 TRAVERSAL PROBE: the flag bit both slot-8 bodies gate on,
    // per class, at two sampling points — and the traversal's own call sites.
    std::fprintf(stderr,
                 "[fe]   SLOT-8 PROBE — flag [this+0x10] & 0x02000000, per class:\n"
                 "[fe]                     at slot-8 entry        in the update walk\n"
                 "[fe]                     clear      SET         clear      SET\n");
    for (int c = 0; c < 3; c++)
        std::fprintf(stderr, "[fe]     %-10s %9llu %9llu   %9llu %9llu\n", kVtClassName[c],
                     (unsigned long long)g_s8Flag[c][0].load(),
                     (unsigned long long)g_s8Flag[c][1].load(),
                     (unsigned long long)g_updFlag[c][0].load(),
                     (unsigned long long)g_updFlag[c][1].load());
    std::fprintf(stderr,
                 "[fe]   THE WALKER'S GATES, sampled in the update walk, per class:\n"
                 "[fe]                bit 0x00800000          [this+0x6C]\n"
                 "[fe]                clear      SET         <=0        >0\n");
    for (int c = 0; c < 3; c++)
        std::fprintf(stderr, "[fe]     %-10s %9llu %9llu   %9llu %9llu\n", kVtClassName[c],
                     (unsigned long long)g_updBit8[c][0].load(),
                     (unsigned long long)g_updBit8[c][1].load(),
                     (unsigned long long)g_updAlpha[c][0].load(),
                     (unsigned long long)g_updAlpha[c][1].load());
    std::fprintf(stderr,
                 "[fe]   DRAW-TREE MEMBERSHIP AND ANCESTOR GATES (update-walk samples):\n"
                 "[fe]                parent=0  in-chain  NOT-in-chain  ancFAILbit8  ancFAILalpha  all-pass\n");
    for (int c = 0; c < 3; c++)
        std::fprintf(stderr, "[fe]     %-10s %8llu %9llu %13llu %12llu %13llu %9llu\n",
                     kVtClassName[c], (unsigned long long)g_parNull[c].load(),
                     (unsigned long long)g_inChain[c].load(),
                     (unsigned long long)g_notInChain[c].load(),
                     (unsigned long long)g_ancFailBit8[c].load(),
                     (unsigned long long)g_ancFailAlpha[c].load(),
                     (unsigned long long)g_ancAllPass[c].load());
    {
        std::lock_guard<std::mutex> anclock(g_ancMutex);
        for (int c = 0; c < 3; c++)
            for (const auto& e : g_ancFailVt[c])
                std::fprintf(stderr,
                             "[fe]     %-10s first-failing ancestor vtable 0x%08X   %llu x\n",
                             kVtClassName[c], e.first, (unsigned long long)e.second);
        std::fprintf(stderr,
                     "[fe]   meter fail samples NOT attributed below (row cap): %llu\n",
                     (unsigned long long)g_ancOverflow.load());
        // Dump each distinct failing METER ancestor from live guest memory:
        // its state, its parent chain to the root, and its first children —
        // the identity data the hide/show rows below get matched against.
        if (uint8_t* b = g_base.load(std::memory_order_relaxed))
            for (const auto& e : g_meterAncFail)
            {
                const uint32_t a = e.first;
                std::fprintf(stderr,
                             "[fe]   METER-BLOCKING ancestor 0x%08X (seen %llu x):\n",
                             a, (unsigned long long)e.second);
                std::lock_guard<std::mutex> nmlock(g_nameMutex);
                uint32_t w = a;
                for (int d = 0; d < 12 && w >= 0x1000; d++, w = GuestU32(b, w + 0xB0))
                {
                    const uint32_t fl = GuestU32(b, w + 0x10);
                    std::fprintf(stderr,
                                 "[fe]     %*s0x%08X vt 0x%08X flags 0x%08X bit8=%d alpha=%g  \"%s\"\n",
                                 d * 2, "", w, GuestU32(b, w), fl,
                                 (fl & 0x00800000u) ? 1 : 0, GuestF32(b, w + 0x6C),
                                 NameOfId(GuestU32(b, w + 0x4)));
                }
                uint32_t ch = GuestU32(b, a + 0x8);
                for (int i = 0; i < 8 && ch >= 0x1000; i++, ch = GuestU32(b, ch + 0xC))
                    std::fprintf(stderr,
                                 "[fe]       child 0x%08X vt 0x%08X flags 0x%08X  \"%s\"\n",
                                 ch, GuestU32(b, ch), GuestU32(b, ch + 0x10),
                                 NameOfId(GuestU32(b, ch + 0x4)));
            }
    }
    std::fprintf(stderr,
                 "[fe]   FORCESHOW arm %s: forced bit8 %llu x, forced alpha %llu x\n",
                 kForceShow ? "ARMED" : "off", (unsigned long long)g_forcedBit8.load(),
                 (unsigned long long)g_forcedAlpha.load());
    std::fprintf(stderr, "[fe]   HIDE/SHOW calls — {function, caller LR, r3, [r3+0x3C4]}:\n");
    {
        std::lock_guard<std::mutex> hslock(g_hsMutex);
        const char* what = "";
        for (const auto& r : g_hsRows)
        {
#define X(addr, w2)                                                                      \
            if (r.func == 0x##addr) what = w2;
            CW_HS_FUNCS(X)
#undef X
            std::fprintf(stderr,
                         "[fe]     sub_%08X lr 0x%08X r3 0x%08X w 0x%08X %6llu x  %s\n",
                         r.func, r.lr, r.obj, r.widget, (unsigned long long)r.count, what);
        }
        if (g_hsRows.empty())
            std::fprintf(stderr, "[fe]     (none of the twelve ever ran)\n");
    }
    // THE RENDER GATE: per named meter, the retained-draw handle [this+0x244]
    // (-1 = the render silently returns) and the value field [this+0x1D0];
    // then every registry lookup {name -> result} through sub_82813940.
    {
        std::lock_guard<std::mutex> mhlock(g_mhMutex);
        std::lock_guard<std::mutex> nmlock4(g_nameMutex);
        std::fprintf(stderr, "[fe]   METER RENDER GATE — per meter: handle 0x244, value 0x1D0:\n");
        for (const auto& e : g_meterHandle)
            std::fprintf(stderr,
                         "[fe]     %-28s handle 0x%08X%s  value 0x%08X  (%llu samples)\n",
                         NameOfId(e.first), e.second.handle,
                         e.second.handle == 0xFFFFFFFFu ? " <== NEVER DRAWS" : "",
                         e.second.value, (unsigned long long)e.second.samples);
        {
            std::lock_guard<std::mutex> rilock(g_riMutex);
            std::fprintf(stderr,
                         "[fe]   retained-item updates via sub_8273A510 {fptr, id, type}:\n");
            for (size_t i = 0; i < g_riRows.size(); i++)
                std::fprintf(stderr, "[fe]     fptr 0x%08X  id 0x%08X  type %u  %llu x\n",
                             g_riRows[i][0], g_riRows[i][1], g_riRows[i][2],
                             (unsigned long long)g_riCounts[i]);
            if (g_riRows.empty())
                std::fprintf(stderr, "[fe]     (never called)\n");
        }
        std::fprintf(stderr,
                     "[fe]   meter submit chain: begin %llu  submit %llu  end %llu\n",
                     (unsigned long long)g_batchBegin.load(),
                     (unsigned long long)g_batchSubmit.load(),
                     (unsigned long long)g_batchEnd.load());
        {
            std::lock_guard<std::mutex> rslock(g_rsMutex);
            std::fprintf(stderr, "[fe]   resource resolves via sub_8275CD58 {name -> id}:\n");
            if (g_resolveRows.empty())
                std::fprintf(stderr, "[fe]     (never called)\n");
            for (const auto& r : g_resolveRows)
                std::fprintf(stderr, "[fe]     %-36s -> 0x%08X%s  %llu x\n", r.name.c_str(),
                             r.result,
                             (r.result == 0xFFFFFFFFu || r.result == 0) ? " <== SUSPECT" : "",
                             (unsigned long long)r.count);
        }
        std::fprintf(stderr, "[fe]   registry lookups via sub_82813940 {name -> result}:\n");
        if (g_regRows.empty())
            std::fprintf(stderr, "[fe]     (never called)\n");
        for (const auto& r : g_regRows)
            std::fprintf(stderr, "[fe]     %-36s -> 0x%08X%s  %llu x\n", r.name.c_str(),
                         r.result, r.result == 0xFFFFFFFFu ? " <== FAILED" : "",
                         (unsigned long long)r.count);
    }
    // THE DRAW-TIME FRONTIER: which named widgets the draw recursion visits,
    // and per widget, how its own children's gates read AT DRAW TIME. A widget
    // that is 'entered' while its child fails is the exact block point.
    std::fprintf(stderr,
                 "[fe]   DRAW-TIME frontier — per widget: entered / as-a-child pass|failBit8|failAlpha:\n");
    {
        std::lock_guard<std::mutex> s8lock2(g_s8Mutex);
        std::lock_guard<std::mutex> nmlock3(g_nameMutex);
        std::vector<std::pair<uint32_t, const S8Gate*>> rows;
        for (const auto& e : g_s8ById)
            rows.emplace_back(e.first, &e.second);
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            return a.second->entered + a.second->childPass + a.second->childFailBit8 +
                       a.second->childFailAlpha >
                   b.second->entered + b.second->childPass + b.second->childFailBit8 +
                       b.second->childFailAlpha;
        });
        size_t shown = 0;
        for (const auto& r : rows)
        {
            (void)shown;   // print EVERY row — a capped list is not a count (gotcha 109)
            std::fprintf(stderr,
                         "[fe]     %-28s entered %8llu   pass %8llu  failBit8 %8llu  failAlpha %8llu\n",
                         NameOfId(r.first), (unsigned long long)r.second->entered,
                         (unsigned long long)r.second->childPass,
                         (unsigned long long)r.second->childFailBit8,
                         (unsigned long long)r.second->childFailAlpha);
        }
        std::fprintf(stderr, "[fe]     (%zu named widgets total in the draw walk)\n",
                     rows.size());
    }
    std::fprintf(stderr, "[fe]   FindChildById (slot 18) — {id -> name, caller LR}:\n");
    {
        std::lock_guard<std::mutex> flock(g_findMutex);
        std::lock_guard<std::mutex> nmlock2(g_nameMutex);
        for (size_t i = 0; i < g_findRows.size(); i++)
            std::fprintf(stderr, "[fe]     id 0x%08X \"%s\"  lr 0x%08X  %6llu x\n",
                         g_findRows[i][0], NameOfId(g_findRows[i][0]), g_findRows[i][1],
                         (unsigned long long)g_findCounts[i]);
        if (g_findRows.empty())
            std::fprintf(stderr, "[fe]     (never called)\n");
        std::fprintf(stderr, "[fe]   name dictionary: %zu distinct ids recorded\n",
                     g_idName.size());
    }
    std::fprintf(stderr, "[fe]   slot-8 CALL SITES (return addresses, both bodies):\n");
    {
        std::lock_guard<std::mutex> s8lock(g_s8Mutex);
        if (g_s8Callers.empty())
            std::fprintf(stderr, "[fe]     (slot 8 never ran)\n");
        else
            for (const auto& e : g_s8Callers)
                std::fprintf(stderr, "[fe]     from 0x%08X   %llu x\n", e.first,
                             (unsigned long long)e.second);
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
