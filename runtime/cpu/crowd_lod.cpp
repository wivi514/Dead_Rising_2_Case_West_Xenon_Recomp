#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../host/settings.h"
#include "../kernel/heap.h"
#include "../kernel/memory.h"
#include "ppc_recomp_shared.h"

// ZOMBIE DETAIL — Case Zero's crowd LOD scale (5cb3781 there, 2026-10-08), imported
// 2026-10-14 with the ONE address it hooks re-derived on this image.
//
// THE MECHANISM, as the sibling read it out of its crowd renderer (their sub_825B8680).
// Every frame the crowd is scored by how large each zombie is on screen and sorted largest
// first. LOD1 (the close model) takes the first N1 zombies whose key is >= T1, LOD2 the
// next N2 with key >= T2, LOD3 the next N3. The constants are the title's:
//
//     counts  = { 1, 30, 70, 400 }          (LOD0..3)
//     keys    = { 9999, 0.13, 0.01, 0.001 }
//
// and BOTH are scaled by one quality value Q: count_i * Q and key_i / Q. So Q = 2 gives 60
// close-model zombies and holds the close model to twice the distance. Q comes from the
// engine's option table (0x14-byte entries) through the accessor hooked here,
// (settings, group) -> &entry; group 2 is the crowd scale, and the 360 left it at 1.0.
//
// ON THIS IMAGE (Case West, measured 2026-10-14):
//   * the accessor is sub_82732290 (theirs sub_8279A488): a unique 1.000 shape match, nine
//     instructions, identical but for the table base (0x82A87178 here, 0x82AD0480 there —
//     BSS, filled at run time, so its values are not readable from the image);
//   * group 2 is read at exactly ONE site, inside sub_82589980 (a census of every
//     `bl sub_82732290` and the `li r4,N` before it: groups 0, 3 x4, 5 and 2 once);
//   * that site loads the count table at 0x82085878 right after the call and multiplies
//     by the returned float — and the table reads { 1, 30, 70, 400 }, the sibling's.
// So this is the same crowd renderer and the same knob. The title's own value at the
// first call is printed (below) so a run shows the 1.0 rather than this comment asserting
// it.
//
// THE HOOK answers the group-2 lookup with a pointer to a guest float we own, holding
// the setting (0.5..5.0, default 2.0, live — the crowd renderer reads it every frame).
// Every other group passes through. CW_CROWD_LOD=<value> overrides the setting for an
// A/B, and CW_CROWD_LOD=0 is the control: the title's own lookup.

extern "C" PPC_FUNC(__imp__sub_82732290);

namespace
{
// -1 = no override (use the setting), 0 = control (pass through), >0 = forced value.
float EnvOverride()
{
    static const float v = [] {
        const char* e = std::getenv("CW_CROWD_LOD");
        if (!e || !*e)
            return -1.0f;
        const float f = float(std::atof(e));
        if (f <= 0.0f)
            fprintf(stderr, "[crowdlod] CW_CROWD_LOD=0 — the title's own crowd LOD "
                            "scale (1.0), the control\n");
        else
            fprintf(stderr, "[crowdlod] CW_CROWD_LOD=%.2f overrides the ZOMBIE DETAIL "
                            "setting\n", double(f));
        return f <= 0.0f ? 0.0f : f;
    }();
    return v;
}

uint32_t ScaleSlot()
{
    static const uint32_t guest = [] {
        void* host = g_heap.Alloc(16);
        if (!host)
        {
            fprintf(stderr, "[crowdlod] could not allocate the scale slot — the title's "
                            "own value stays in force\n");
            return 0u;
        }
        std::memset(host, 0, 16);
        return g_memory.MapVirtual(host);
    }();
    return guest;
}
} // namespace

PPC_FUNC(sub_82732290)
{
    const uint32_t group = ctx.r4.u32;
    __imp__sub_82732290(ctx, base);
    if (group != 2)
        return;
    static bool reportedOwn = false;
    if (!reportedOwn)
    {
        reportedOwn = true;
        uint32_t own;
        std::memcpy(&own, base + ctx.r3.u32, 4);
        own = __builtin_bswap32(own);
        float ownF;
        std::memcpy(&ownF, &own, 4);
        fprintf(stderr, "[crowdlod] the title's own crowd LOD scale (group 2) reads %.2f\n",
                double(ownF));
    }
    const float forced = EnvOverride();
    if (forced == 0.0f)
        return;
    const uint32_t slot = ScaleSlot();
    if (!slot)
        return;
    const float q = forced > 0.0f ? forced : float(Settings_CrowdLodX10()) * 0.1f;
    uint32_t bits;
    std::memcpy(&bits, &q, 4);
    bits = __builtin_bswap32(bits);
    std::memcpy(base + slot, &bits, 4);
    static bool announced = false;
    if (!announced)
    {
        announced = true;
        fprintf(stderr, "[crowdlod] crowd LOD scale %.1f (title default 1.0): close "
                        "models for up to %d zombies, %.1fx the distance\n",
                double(q), int(30 * q + 0.5f), double(q));
    }
    ctx.r3.u64 = slot;
}
