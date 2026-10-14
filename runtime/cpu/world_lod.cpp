#include <atomic>
#include <cstdio>
#include <cstdlib>

#include "../host/settings.h"
#include "ppc_recomp_shared.h"

// WORLD DETAIL — Case Zero's static-geometry LOD distance scale (f161cd7 there,
// 2026-10-08: their pawnshop's lettering, cactus and hydrant only appeared from across
// the street), imported 2026-10-14 with both hooked addresses re-derived on this image.
//
// THE MECHANISM, as the sibling read it. Every zone carries a list of static-geometry
// volumes (cZone, count at +0x120, 0xD0-byte records at [+0x124]): a sphere at +0x80 and
// a switch distance. A per-volume vote decides NEAR iff
// |camera - centre| - 0.01 - radius < threshold, and the zone async loader streams the
// detailed static geometry in for the NEAR volumes. Before comparing, the vote boosts the
// threshold:
//
//     if (threshold < cutoff(level)) threshold *= multiplier(level)
//
// with cutoff and multiplier two leaf accessors over per-level tables (or two debug floats
// when a shipped-off byte is set). Both accessors are called from exactly one function,
// the vote, so hooking them changes these switch distances and nothing else.
//
// ON THIS IMAGE (Case West, measured 2026-10-14):
//   * the vote is sub_82178F98 (theirs sub_82175040; 0.893 shape agreement, ours runs 300
//     instructions to their 233). Its boost block is the sibling's instruction for
//     instruction — bl cutoff; fcmpu f31,f1; bge; bl multiplier; fmuls f31,f1,f31 — and it
//     reads the record's sphere at +0x80 through [+0x124] as theirs does;
//   * the cutoff is sub_8236DB18 and the multiplier sub_8236DB58 (theirs sub_82373DC0 /
//     sub_82373E00). shape_match finds EIGHT 1.000 twins — the engine has eight such
//     per-level accessor pairs — so the pair is named by the vote's own two `bl`s, not by
//     the match. Each has exactly one caller (the vote; grep of ppc/). Debug floats
//     0x829F0484/0x829F0488 behind byte 0x82A74A17, the same shape as theirs;
//   * the per-level tables (0x8203D4B8 cutoff, 0x8203D4E4 multiplier) hold ELEVEN levels:
//     cutoff { 9999, 20 x10 }, multiplier 1.0 at EVERY level. Dead Rising 2's levels boost
//     2-2.5x and Case Zero's own (14) is 9999 / 1.0; Case West boosts nothing anywhere.
//     So here, as there, the title's switch distances are the raw +0xA8 values.
//
// THE HOOK: at a WORLD DETAIL of S != 1.0 the cutoff becomes at least 9999 (every
// volume) and the multiplier is the level's own times S, so every switch distance is
// S times the title's. CW_WORLD_LOD=<value> overrides the setting for an A/B;
// CW_WORLD_LOD=0 is the control (the title's own accessors, untouched). Applies LIVE: the
// vote reads the pair every time it runs.
extern "C" PPC_FUNC(__imp__sub_8236DB18);
extern "C" PPC_FUNC(__imp__sub_8236DB58);

namespace
{
// -1 = no override (use the setting), 0 = control (pass through), >0 = forced value.
double EnvOverride()
{
    static const double v = [] {
        const char* e = std::getenv("CW_WORLD_LOD");
        if (!e || !*e)
            return -1.0;
        const double f = std::atof(e);
        if (f <= 0.0)
            fprintf(stderr, "[worldlod] CW_WORLD_LOD=0 — the title's own LOD switch "
                            "distances (1.0), the control\n");
        else
            fprintf(stderr, "[worldlod] CW_WORLD_LOD=%.2f overrides the WORLD DETAIL "
                            "setting\n", f);
        return f <= 0.0 ? 0.0 : f;
    }();
    return v;
}

// The scale in force, or 1.0 when the title's own values should pass through.
double Scale()
{
    const double forced = EnvOverride();
    if (forced == 0.0)
        return 1.0;
    const double s = forced > 0.0 ? forced : 1.0;
    // Printed once per distinct value; the zone streamer may vote from more than one
    // thread, hence the exchange.
    static std::atomic<int> announced{-1};
    const int tenths = int(s * 10.0 + 0.5);
    if (announced.exchange(tenths, std::memory_order_relaxed) != tenths)
    {
        fprintf(stderr, "[worldlod] world LOD scale %.1f (title default 1.0): static "
                        "geometry switches to its detailed version at %.1fx the "
                        "distance\n", s, s);
    }
    return s;
}
} // namespace

// The cutoff below which a switch distance is boosted. Raised to cover every volume
// whenever the scale is not 1, so the scale applies to all of them alike.
PPC_FUNC(sub_8236DB18)
{
    __imp__sub_8236DB18(ctx, base);
    if (Scale() != 1.0 && ctx.f1.f64 < 9999.0)
        ctx.f1.f64 = 9999.0;
}

// The boost itself: the level's own multiplier times the player's scale.
PPC_FUNC(sub_8236DB58)
{
    __imp__sub_8236DB58(ctx, base);
    const double s = Scale();
    if (s != 1.0)
        ctx.f1.f64 = double(float(ctx.f1.f64 * s));   // lfs result: keep it single
}
